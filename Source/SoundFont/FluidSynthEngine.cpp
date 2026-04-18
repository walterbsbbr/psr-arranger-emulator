#include "FluidSynthEngine.h"

FluidSynthEngine::FluidSynthEngine()
{
    bankMsb.fill (0);
    bankLsb.fill (0);
    programNum.fill (0);

    settings = new_fluid_settings();
    fluid_settings_setstr  (settings, "audio.driver",   "no");  // JUCE controla o áudio
    fluid_settings_setnum  (settings, "synth.sample-rate", 44100.0);
    fluid_settings_setint  (settings, "synth.polyphony",   256);
    fluid_settings_setint  (settings, "synth.midi-channels", 16);
    fluid_settings_setnum  (settings, "synth.gain",       0.8);
    fluid_settings_setint  (settings, "synth.reverb.active", 1);
    fluid_settings_setint  (settings, "synth.chorus.active", 1);

    synth = new_fluid_synth (settings);
}

FluidSynthEngine::~FluidSynthEngine()
{
    if (synth)    { delete_fluid_synth    (synth);    synth    = nullptr; }
    if (settings) { delete_fluid_settings (settings); settings = nullptr; }
}

bool FluidSynthEngine::loadSoundFont (const juce::File& sf2File)
{
    juce::ScopedLock sl (synthLock);

    if (soundFontId >= 0)
    {
        fluid_synth_sfunload (synth, (unsigned int)soundFontId, 1);
        soundFontId = -1;
    }

    const auto path = sf2File.getFullPathName().toStdString();
    int id = fluid_synth_sfload (synth, path.c_str(), 1 /*reset presets*/);

    if (id == FLUID_FAILED)
    {
        DBG ("FluidSynth: falha ao carregar " + sf2File.getFullPathName());
        return false;
    }

    soundFontId  = id;
    loadedSfName = sf2File.getFileNameWithoutExtension();
    DBG ("FluidSynth: SF2 carregado -> " + loadedSfName + " (id=" + juce::String(id) + ")");
    return true;
}

void FluidSynthEngine::unloadSoundFont()
{
    juce::ScopedLock sl (synthLock);
    if (soundFontId >= 0)
    {
        fluid_synth_sfunload (synth, (unsigned int)soundFontId, 1);
        soundFontId = -1;
        loadedSfName.clear();
    }
}

void FluidSynthEngine::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    juce::ScopedLock sl (synthLock);
    currentSampleRate = sampleRate;
    fluid_settings_setnum (settings, "synth.sample-rate", sampleRate);
    // Recriar o synth com o novo sample rate
    delete_fluid_synth (synth);
    synth = new_fluid_synth (settings);
    if (soundFontId >= 0)
    {
        // Recarregar o SF2 após recriar o synth
        // (guardamos o id anterior, mas ele vai mudar)
        soundFontId = -1;
    }
}

void FluidSynthEngine::releaseResources()
{
    allNotesOff();
}

void FluidSynthEngine::processAudio (float* leftOut, float* rightOut, int numSamples)
{
    juce::ScopedLock sl (synthLock);
    if (synth == nullptr) return;

    // FluidSynth escreve interleaved ou separado — usamos separado (non-interleaved)
    fluid_synth_write_float (synth, numSamples,
                             leftOut,  0, 1,
                             rightOut, 0, 1);
}

void FluidSynthEngine::sendMidiMessage (const juce::MidiMessage& msg)
{
    juce::ScopedLock sl (synthLock);
    if (synth == nullptr || soundFontId < 0) return;

    const int ch = msg.getChannel() - 1;  // JUCE usa 1-16, FluidSynth usa 0-15

    if (msg.isNoteOn())
    {
        fluid_synth_noteon (synth, ch, msg.getNoteNumber(), msg.getVelocity());
    }
    else if (msg.isNoteOff())
    {
        fluid_synth_noteoff (synth, ch, msg.getNoteNumber());
    }
    else if (msg.isController())
    {
        const int cc  = msg.getControllerNumber();
        const int val = msg.getControllerValue();

        // Captura Bank Select para fallback XG→GM
        if (cc == 0)       bankMsb[ch] = val;
        else if (cc == 32) bankLsb[ch] = val;
        else               fluid_synth_cc (synth, ch, cc, val);
    }
    else if (msg.isProgramChange())
    {
        programNum[ch] = msg.getProgramChangeNumber();
        applyProgramChange (ch);
    }
    else if (msg.isPitchWheel())
    {
        // JUCE: -8192..8191 → FluidSynth: 0..16383
        int pitchVal = msg.getPitchWheelValue() + 8192;
        fluid_synth_pitch_bend (synth, ch, pitchVal);
    }
    else if (msg.isChannelPressure())
    {
        fluid_synth_channel_pressure (synth, ch, msg.getChannelPressureValue());
    }
    else if (msg.isAftertouch())
    {
        fluid_synth_key_pressure (synth, ch,
                                  msg.getNoteNumber(),
                                  msg.getAfterTouchValue());
    }
    else if (msg.isSysEx())
    {
        // GM System On: F0 7E 7F 09 01 F7
        const uint8_t* data = msg.getSysExData();
        const int      size = msg.getSysExDataSize();
        if (size >= 4 && data[0] == 0x7E && data[2] == 0x09 && data[3] == 0x01)
        {
            fluid_synth_system_reset (synth);
            DBG ("FluidSynth: GM System Reset recebido");
        }
        // SysEx proprietário Yamaha XG: ignorado silenciosamente
    }
}

void FluidSynthEngine::applyProgramChange (int ch)
{
    // Lógica de fallback: tenta MSB/LSB como está; se falhar, tenta GM (0/0)
    int msb = bankMsb[ch];
    int lsb = bankLsb[ch];
    int pc  = programNum[ch];

    // Bancos Yamaha XG/Mega Voice (MSB > 0) podem não existir no SF2 GM
    // Tentativa 1: banco original
    int result = fluid_synth_bank_select      (synth, ch, msb * 128 + lsb);
    result     = fluid_synth_program_change   (synth, ch, pc);

    // Se FluidSynth não encontrou a voz (preset inválido), verifica
    fluid_preset_t* preset = fluid_synth_get_channel_preset (synth, ch);
    if (preset == nullptr && (msb != 0 || lsb != 0))
    {
        // Fallback para GM padrão
        DBG ("FluidSynth: fallback GM para ch=" + juce::String(ch+1)
             + " pc=" + juce::String(pc)
             + " (banco " + juce::String(msb) + "/" + juce::String(lsb) + " nao encontrado)");
        fluid_synth_bank_select    (synth, ch, 0);
        fluid_synth_program_change (synth, ch, pc);
    }

    // Canal 10 (index 9) é sempre bateria no GM
    if (ch == 9)
        fluid_synth_bank_select (synth, ch, 128); // GM percussion bank
}

juce::String FluidSynthEngine::getChannelPresetName (int ch) const
{
    juce::ScopedLock sl (synthLock);
    if (synth == nullptr || ch < 0 || ch > 15) return {};

    fluid_preset_t* preset = fluid_synth_get_channel_preset (synth, ch);
    if (preset == nullptr) return {};

    const char* name = fluid_preset_get_name (preset);
    return name ? juce::String (name) : juce::String ("PC " + juce::String (programNum[ch]));
}

void FluidSynthEngine::allNotesOff()
{
    juce::ScopedLock sl (synthLock);
    if (synth == nullptr) return;
    for (int ch = 0; ch < 16; ++ch)
        fluid_synth_all_notes_off (synth, ch);
}

void FluidSynthEngine::resetAllControllers()
{
    juce::ScopedLock sl (synthLock);
    if (synth == nullptr) return;
    for (int ch = 0; ch < 16; ++ch)
        fluid_synth_cc (synth, ch, 121, 0); // CC 121 = Reset All Controllers
}
