#include "StyleEngine.h"
#include "MidiRouter.h"

StyleEngine::StyleEngine (FluidSynthEngine& synth, MidiRouter& router)
    : synthEngine (synth), midiRouter (router)
{
    partVolume.fill (100);
    partMuted.fill  (false);

    // Cria o LoopPlayer com callback que recebe mensagens brutas do STY
    player = std::make_unique<LoopPlayer> ([this] (const juce::MidiMessage& msg) {
        onMidiFromStyle (msg);
    });

    player->onSectionEnded = [this] (StyleSection which) {
        onSectionEnded (which);
    };
}

StyleEngine::~StyleEngine()
{
    player->stop();
}

// ─── loadStyle ────────────────────────────────────────────────────────────────
bool StyleEngine::loadStyle (const juce::File& styFile)
{
    player->stop();
    synthEngine.allNotesOff();

    currentSty = StyParser::parse (styFile);
    if (!currentSty.valid)
    {
        DBG ("StyleEngine: falha ao carregar " + styFile.getFileName());
        return false;
    }

    currentBpm = currentSty.defaultBpm;
    player->prepareToPlay (currentBpm, currentSty.ppq, synthEngine.getSampleRate());

    // Envia a inicialização do SInt (Program Changes, SysEx) direto para o FluidSynth.
    // Os eventos do SInt configuram os instrumentos nos canais do acompanhamento.
    // Enviar direto evita duplicação de bank select no MidiRouter.
    if (currentSty.hasSection (StyleSection::SInt))
    {
        const auto& sintEvents = currentSty.getSection (StyleSection::SInt).events;
        for (int i = 0; i < sintEvents.getNumEvents(); ++i)
        {
            const auto& msg = sintEvents.getEventPointer(i)->message;
            synthEngine.sendMidiMessage (msg);
        }
    }

    state = State::Idle;
    DBG ("StyleEngine: estilo carregado -> " + currentSty.name
         + " | BPM=" + juce::String (currentBpm, 1));
    return true;
}

void StyleEngine::unloadStyle()
{
    player->stop();
    synthEngine.allNotesOff();
    currentSty = StyFile{};
    state = State::Idle;
}

// ─── Controle de transporte ───────────────────────────────────────────────────
void StyleEngine::start()
{
    if (!currentSty.valid) return;

    // Toca Intro A se existir, senão vai direto para Main A
    if (currentSty.hasSection (StyleSection::IntroA))
    {
        state = State::Intro;
        player->playSection (currentSty.getSection (StyleSection::IntroA),
                             StyleSection::IntroA,
                             false /*one-shot*/);
    }
    else
    {
        transitionToMain (0);
    }
}

void StyleEngine::stop()
{
    player->stop();
    synthEngine.allNotesOff();
    state = State::Idle;
}

// ─── Seleção de seções ────────────────────────────────────────────────────────
void StyleEngine::selectIntro (int ab)
{
    if (!currentSty.valid) return;
    static const StyleSection intros[] = { StyleSection::IntroA, StyleSection::IntroB, StyleSection::IntroC };
    const auto sec = intros[std::clamp (ab, 0, 2)];
    if (!currentSty.hasSection (sec)) return;

    state = State::Intro;
    player->playSection (currentSty.getSection (sec), sec, false);
}

void StyleEngine::selectMain (int ab)
{
    if (!currentSty.valid) return;
    static const StyleSection mains[] = { StyleSection::MainA, StyleSection::MainB,
                                          StyleSection::MainC, StyleSection::MainD };
    const auto sec = mains[std::clamp (ab, 0, 3)];
    if (!currentSty.hasSection (sec)) return;

    activeMainIdx = ab;
    if (state == State::Main)
        player->queueSection (currentSty.getSection (sec), sec, true);
    else
        transitionToMain (ab);
}

void StyleEngine::selectFill (int ab)
{
    if (!currentSty.valid || state != State::Main) return;

    static const StyleSection fills[] = {
        StyleSection::FillInAA, StyleSection::FillInAB,
        StyleSection::FillInBA, StyleSection::FillInBB
    };
    const auto sec = fills[std::clamp (ab, 0, 3)];
    if (!currentSty.hasSection (sec)) return;

    // O fill determina para qual Main retornar:
    // AA→A(0), AB→B(1), BA→A(0), BB→B(1)
    static const int fillTargetMain[] = { 0, 1, 0, 1 };
    activeMainIdx = fillTargetMain[std::clamp (ab, 0, 3)];

    state = State::Fill;
    player->queueSection (currentSty.getSection (sec), sec, false);
}

void StyleEngine::selectEnding (int ab)
{
    if (!currentSty.valid || state == State::Idle) return;

    static const StyleSection endings[] = { StyleSection::EndingA, StyleSection::EndingB, StyleSection::EndingC };
    const auto sec = endings[std::clamp (ab, 0, 2)];
    if (!currentSty.hasSection (sec)) return;

    state = State::Ending;
    player->queueSection (currentSty.getSection (sec), sec, false);
}

// ─── transitionToMain ─────────────────────────────────────────────────────────
void StyleEngine::transitionToMain (int idx)
{
    static const StyleSection mains[] = { StyleSection::MainA, StyleSection::MainB,
                                          StyleSection::MainC, StyleSection::MainD };
    idx = std::clamp (idx, 0, 3);
    const auto sec = mains[idx];

    if (!currentSty.hasSection (sec)) return;

    activeMainIdx = idx;
    state = State::Main;
    player->playSection (currentSty.getSection (sec), sec, true /*loop*/);
}

// ─── onSectionEnded ───────────────────────────────────────────────────────────
void StyleEngine::onSectionEnded (StyleSection which)
{
    // Chamado no message thread via callAsync
    if (isSectionOneShot (which))
    {
        if (state == State::Intro || state == State::Fill)
            transitionToMain (activeMainIdx);
        else if (state == State::Ending)
            stop();
    }
}

// ─── onChordChanged ──────────────────────────────────────────────────────────
void StyleEngine::onChordChanged (const ChordInfo& chord)
{
    juce::ScopedLock sl (chordLock);
    currentChord = chord;
}

// ─── BPM e transpose ─────────────────────────────────────────────────────────
void StyleEngine::setBpm (double bpm)
{
    currentBpm = std::clamp (bpm, 20.0, 300.0);
    player->setBpm (currentBpm);
}

void StyleEngine::setTranspose (int semitones)
{
    transposeOffset = std::clamp (semitones, -12, 12);
}

// ─── Mixer ───────────────────────────────────────────────────────────────────
void StyleEngine::setPartVolume (int destCh, uint8_t volume)
{
    if (destCh < 0 || destCh > 7) return;
    partVolume[destCh] = volume;
    // Envia CC7 (volume) para o canal de destino real (ch 9-16)
    auto msg = juce::MidiMessage::controllerEvent (destCh + 9, 7, volume);
    synthEngine.sendMidiMessage (msg);
}

void StyleEngine::setPartMuted (int destCh, bool mute)
{
    if (destCh < 0 || destCh > 7) return;
    partMuted[destCh] = mute;
    if (mute)
    {
        auto msg = juce::MidiMessage::allNotesOff (destCh + 9);
        synthEngine.sendMidiMessage (msg);
    }
}

// ─── isDrumChannel ────────────────────────────────────────────────────────────
// Canais de bateria/percussão: não sofrem transposição.
// No padrão PSR: Rhythm1 e Rhythm2 (geralmente raw ch 8,9 = JUCE ch 9,10)
// Convenção GM: canal 10 (JUCE 10, 0-idx 9) é bateria.
static bool isDrumChannel (int ch0)
{
    // Aceita canal 9 (GM drums) e canal 8 (Rhythm1 em muitos STY)
    return ch0 == 8 || ch0 == 9;
}

// ─── onMidiFromStyle ─────────────────────────────────────────────────────────
// Chamado do HighResolutionTimer thread pelo LoopPlayer.
// Segue a arquitetura real do PSR:
//   - Canais de bateria/percussão: passam direto, sem transposição
//   - Canais melódicos: transpõem pela fundamental do acorde + correção de tipo
//   - CC/PC/SysEx: passam direto para manter o timbre correto
void StyleEngine::onMidiFromStyle (const juce::MidiMessage& rawMsg)
{
    const int sourceCh = rawMsg.getChannel() - 1; // 0-indexed (0-15)

    // Apenas canais do acompanhamento (raw 7-15 = JUCE 8-16)
    if (sourceCh < 7 || sourceCh > 15) return;

    // Verificar mute da parte (partIdx 0-7 para canais 8-15)
    const int partIdx = sourceCh - 8;
    if (partIdx >= 0 && partIdx < 8 && partMuted[partIdx]) return;

    // ── CC, PC, SysEx: enviar diretamente sem modificação ────────────────────
    if (!rawMsg.isNoteOn() && !rawMsg.isNoteOff())
    {
        // Enviar no canal original para que o FluidSynth mantenha o timbre
        synthEngine.sendMidiMessage (rawMsg);
        return;
    }

    // ── Note On/Off ──────────────────────────────────────────────────────────
    int note = rawMsg.getNoteNumber();

    // Transpor para o acorde da mão esquerda (exceto bateria)
    if (!isDrumChannel (sourceCh))
    {
        ChordInfo chord;
        {
            juce::ScopedLock sl (chordLock);
            chord = currentChord;
        }

        if (chord.valid)
        {
            // Transposição ROOT + correção de tipo (3ª/5ª/7ª)
            note = TransposeEngine::transposeRoot (note, chord);
        }

        // Transpose global (slider UI)
        note += transposeOffset;
    }

    note = std::clamp (note, 0, 127);

    // Enviar no canal ORIGINAL (mantém o mapeamento de instrumentos do SInt)
    auto msg = rawMsg.isNoteOn()
        ? juce::MidiMessage::noteOn  (sourceCh + 1, note, rawMsg.getVelocity())
        : juce::MidiMessage::noteOff (sourceCh + 1, note);

    synthEngine.sendMidiMessage (msg);
}
