#include "MidiRouter.h"
#include "StyleEngine.h"

MidiRouter::MidiRouter (FluidSynthEngine& synth, ChordDetector& chords)
    : synthEngine (synth), chordDetector (chords)
{
    styBankMsb.fill (0);
    styBankLsb.fill (0);
}

MidiRouter::~MidiRouter()
{
    closeMidiInput();
}

bool MidiRouter::openMidiInput (const juce::MidiDeviceInfo& device)
{
    closeMidiInput();
    midiInput = juce::MidiInput::openDevice (device.identifier, this);
    if (midiInput != nullptr)
    {
        midiInput->start();
        openedDeviceName = device.name;
        DBG ("MidiRouter: aberto MIDI IN -> " + device.name);
        return true;
    }
    DBG ("MidiRouter: falha ao abrir " + device.name);
    return false;
}

void MidiRouter::closeMidiInput()
{
    if (midiInput != nullptr)
    {
        midiInput->stop();
        midiInput.reset();
        openedDeviceName.clear();
    }
}

// ── Entrada do teclado externo ────────────────────────────────────────────────
void MidiRouter::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                            const juce::MidiMessage& msg)
{
    // Mensagens de canal — dividir entre mão esquerda e direita
    if (msg.isNoteOn() || msg.isNoteOff())
    {
        const int note = msg.getNoteNumber();
        const bool leftHand = (note < splitPoint);

        if (leftHand)
        {
            // Mão esquerda: alimenta o detector de acorde
            if (msg.isNoteOn())
                chordDetector.noteOn (note);
            else
                chordDetector.noteOff (note);

            // Notifica o StyleEngine sobre o novo acorde
            if (styleEngine != nullptr)
                styleEngine->onChordChanged (chordDetector.getCurrentChord());
        }
        else
        {
            // Mão direita: envia direto para o FluidSynth no canal de melodia
            auto routed = msg.isNoteOn()
                ? juce::MidiMessage::noteOn  (melodyChannel, note, msg.getVelocity())
                : juce::MidiMessage::noteOff (melodyChannel, note);
            synthEngine.sendMidiMessage (routed);
        }
    }
    else if (msg.isPitchWheel() || msg.isChannelPressure() || msg.isAftertouch())
    {
        // Redireciona para o canal de melodia
        synthEngine.sendMidiMessage (msg);
    }
    else if (msg.isController())
    {
        // Sustain pedal (CC64) e Sostenuto (CC66): aplica ao canal de melodia
        synthEngine.sendMidiMessage (msg);
    }
}

// ── Mensagens do arquivo STY ──────────────────────────────────────────────────
void MidiRouter::routeStyleMessage (const juce::MidiMessage& msg)
{
    const int ch = msg.getChannel() - 1;  // 0-indexed

    if (msg.isController())
    {
        const int cc  = msg.getControllerNumber();
        const int val = msg.getControllerValue();

        if (cc == 0)        styBankMsb[ch] = val;
        else if (cc == 32)  styBankLsb[ch] = val;

        synthEngine.sendMidiMessage (msg);
    }
    else if (msg.isProgramChange())
    {
        // O FluidSynthEngine cuida do fallback XG→GM internamente.
        // Garantimos que os CC0/CC32 foram enviados antes do PC.
        auto bankMsb = juce::MidiMessage::controllerEvent (ch + 1, 0,  styBankMsb[ch]);
        auto bankLsb = juce::MidiMessage::controllerEvent (ch + 1, 32, styBankLsb[ch]);
        synthEngine.sendMidiMessage (bankMsb);
        synthEngine.sendMidiMessage (bankLsb);
        synthEngine.sendMidiMessage (msg);
    }
    else
    {
        // Notas, pitch bend, SysEx, etc.
        synthEngine.sendMidiMessage (msg);
    }
}

void MidiRouter::resetBankState()
{
    styBankMsb.fill (0);
    styBankLsb.fill (0);
}
