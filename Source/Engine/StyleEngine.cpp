#include "StyleEngine.h"
#include "MidiRouter.h"

StyleEngine::StyleEngine (FluidSynthEngine& synth, MidiRouter& router)
    : synthEngine (synth), midiRouter (router)
{
    partVolume.fill (100);
    partMuted.fill  (false);
    for (auto& ch : noteMap)
        ch.fill (-1);

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
    for (auto& ch : noteMap) ch.fill (-1);
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

    // Tentar os fills disponíveis na ordem de preferência.
    // Diferentes STY files usam diferentes combinações:
    //   Alguns: AA, AB, BA, BB
    //   Outros: AA, BB, CC, DD, BA
    static const StyleSection allFills[] = {
        StyleSection::FillInAA, StyleSection::FillInAB,
        StyleSection::FillInBA, StyleSection::FillInBB,
        StyleSection::FillInCC, StyleSection::FillInDD,
        StyleSection::BreakFill
    };

    // Para o botão 0-3, tenta na ordem: a fill solicitada, depois qualquer fill disponível
    static const StyleSection preferred[4][4] = {
        { StyleSection::FillInAA, StyleSection::FillInCC, StyleSection::FillInBA, StyleSection::FillInBB },
        { StyleSection::FillInAB, StyleSection::FillInBB, StyleSection::FillInDD, StyleSection::FillInAA },
        { StyleSection::FillInBA, StyleSection::FillInAA, StyleSection::FillInCC, StyleSection::FillInBB },
        { StyleSection::FillInBB, StyleSection::FillInDD, StyleSection::FillInAB, StyleSection::FillInAA },
    };

    ab = std::clamp (ab, 0, 3);
    StyleSection sec = StyleSection::Unknown;
    for (auto candidate : preferred[ab])
    {
        if (currentSty.hasSection (candidate))
        {
            sec = candidate;
            break;
        }
    }
    if (sec == StyleSection::Unknown) return;

    // Determinar para qual Main retornar após o fill
    // Fills "xB" → Main B(1), "xA" → Main A(0), outros → manter o atual
    if (sec == StyleSection::FillInAB || sec == StyleSection::FillInBB)
        activeMainIdx = 1;
    else if (sec == StyleSection::FillInAA || sec == StyleSection::FillInBA)
        activeMainIdx = 0;
    // CC, DD, BreakFill → mantém o activeMainIdx atual

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

// ─── onMidiFromStyle ─────────────────────────────────────────────────────────
// Segue a arquitetura real do PSR:
//  1. Consulta CASM para obter NTR/NTT/HighKey/Limits do canal
//  2. Drums (bankMsb=127 ou NTR=BYPASS): passam direto
//  3. Note Off: usa noteMap para casar com o Note On original
//  4. Note On: transpõe via TransposeEngine com parâmetros CASM
//  5. CC/PC/SysEx: passam direto no canal original
void StyleEngine::onMidiFromStyle (const juce::MidiMessage& rawMsg)
{
    const int sourceCh = rawMsg.getChannel() - 1; // 0-indexed (0-15)
    if (sourceCh < 0 || sourceCh > 15) return;

    // ── Consultar CASM para este canal ───────────────────────────────────────
    CasmChannel casmCh;  // defaults: NTR=BYPASS, highKey=127, full range
    bool hasCasm = false;
    const int casmIdx = currentSty.casmIndex[sourceCh];
    if (casmIdx >= 0 && casmIdx < (int)currentSty.casmChannels.size())
    {
        casmCh = currentSty.casmChannels[casmIdx];
        hasCasm = true;
    }

    // Drum = CASM diz BYPASS, ou bankMsb=127 (fallback sem CASM)
    const bool isDrum = (hasCasm && casmCh.ntr == NTR::BYPASS)
                      || synthEngine.isDrumBank (sourceCh);

    // ── Mute: usa destChannel do CASM para o partIdx ─────────────────────────
    const int destCh  = hasCasm ? casmCh.destChannel : sourceCh;
    const int partIdx = destCh - 8;
    if (partIdx >= 0 && partIdx < 8 && partMuted[partIdx]) return;

    // ── CC, PC, SysEx: enviar diretamente ────────────────────────────────────
    if (!rawMsg.isNoteOn() && !rawMsg.isNoteOff())
    {
        synthEngine.sendMidiMessage (rawMsg);
        return;
    }

    // ── Note Off: usar noteMap (nota salva no Note On correspondente) ────────
    const int origNote = rawMsg.getNoteNumber();

    if (rawMsg.isNoteOff() || (rawMsg.isNoteOn() && rawMsg.getVelocity() == 0))
    {
        const int saved = noteMap[sourceCh][origNote];
        if (saved >= 0)
        {
            auto msg = juce::MidiMessage::noteOff (sourceCh + 1, saved);
            synthEngine.sendMidiMessage (msg);
            noteMap[sourceCh][origNote] = -1;
        }
        return;
    }

    // ── Note On ──────────────────────────────────────────────────────────────
    int note = origNote;

    if (!isDrum)
    {
        ChordInfo chord;
        {
            juce::ScopedLock sl (chordLock);
            chord = currentChord;
        }

        if (chord.valid)
        {
            // Usar TransposeEngine com os parâmetros CASM do canal:
            // NTR::ROOT   → shift paralelo (Phrase/melody)
            // NTR::GUITAR → Root Fixed / close-voice (Chord/Pad)
            // NTR::BASS   → segue fundamental (Bass)
            // NTR::BYPASS → sem transposição
            NTR ntr = hasCasm ? casmCh.ntr : NTR::ROOT;
            NTT ntt = hasCasm ? casmCh.ntt : NTT::MELODY;
            int hk  = hasCasm ? casmCh.highKey : 127;
            int lo  = hasCasm ? casmCh.noteLowLimit  : 0;
            int hi  = hasCasm ? casmCh.noteHighLimit  : 127;

            note = TransposeEngine::transposeNote (note, chord, ntr, ntt, hk, lo, hi);
        }

        note += transposeOffset;
    }

    note = std::clamp (note, 0, 127);

    // Salvar mapeamento para o Note Off futuro
    noteMap[sourceCh][origNote] = note;

    auto msg = juce::MidiMessage::noteOn (sourceCh + 1, note, rawMsg.getVelocity());
    synthEngine.sendMidiMessage (msg);
}
