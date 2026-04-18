# Plano de Desenvolvimento: PSR Arranger Emulator
## C++ / JUCE / SoundFont Multitimbal

---

## 1. Visão Geral da Arquitetura

O software é composto por **5 subsistemas principais** que se comunicam via 
fila de mensagens MIDI interna (thread-safe):

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         PSR ARRANGER EMULATOR                           │
│                                                                         │
│  ┌──────────────┐    ┌───────────────────────────────────────────────┐  │
│  │  MIDI INPUT  │───▶│              MIDI ROUTER / MIXER              │  │
│  │  (JUCE)      │    │  (juce::MidiMessageCollector + thread)        │  │
│  └──────────────┘    └───────────────┬───────────────────────────────┘  │
│                                      │                                  │
│  ┌───────────────┐    ┌──────────────▼──────────────────────────────┐  │
│  │  STY PARSER   │───▶│           STYLE ENGINE                      │  │
│  │  (SFF1/SFF2)  │    │  (Loop • Fill • Intro • Ending • Transpose) │  │
│  └───────────────┘    └──────────────┬──────────────────────────────┘  │
│                                      │                                  │
│  ┌───────────────┐    ┌──────────────▼──────────────────────────────┐  │
│  │  CHORD        │───▶│         SOUNDFONT ENGINE (FluidSynth)        │  │
│  │  DETECTOR     │    │  16 canais MIDI • Program Change • SysEx    │  │
│  └───────────────┘    └─────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Stack Tecnológico

| Componente | Tecnologia | Justificativa |
|---|---|---|
| Framework base | **JUCE 8** | Audio I/O, MIDI I/O, UI, threading |
| SoundFont engine | **FluidSynth 2.x** (embedded) | 16 ch multitimbal, Program Change, Bank Select, reverb/chorus |
| Build system | **CMake 3.24+** | Integração JUCE + FluidSynth |
| Padrão C++ | **C++20** | std::span, ranges, jthread |
| UI Renderer | **JUCE OpenGL** | Performance em desktop (Mac/Win/Linux) |

### Por que FluidSynth e não SFZero/TSF?
- **FluidSynth**: maturidade de 20 anos, suporte completo a SF2 v2.1, 
  16 canais reais simultâneos, Bank Select MSB/LSB correto, reverb/chorus 
  nativos, licença LGPL (uso comercial possível)
- **TinySoundFont**: leve mas sem suporte a modulação complexa de SF2
- **SFZero (JUCE)**: não suporta multitimbal de 16 canais nativamente

---

## 3. Estrutura de Módulos do Projeto

```
PSREmulator/
├── CMakeLists.txt
├── Source/
│   ├── Main.cpp
│   ├── MainComponent.h/.cpp          ← janela principal JUCE
│   │
│   ├── SoundFont/
│   │   ├── FluidSynthEngine.h/.cpp   ← wrapper FluidSynth
│   │   └── SoundFontManager.h/.cpp   ← carrega/seleciona SF2
│   │
│   ├── STY/
│   │   ├── StyParser.h/.cpp          ← lê arquivo .sty binário
│   │   ├── StySection.h              ← enum + struct das seções
│   │   ├── CasmParser.h/.cpp         ← decodifica bloco CASM/Ctab
│   │   └── StyFile.h                 ← modelo de dados completo
│   │
│   ├── Engine/
│   │   ├── StyleEngine.h/.cpp        ← motor de arranjo principal
│   │   ├── LoopPlayer.h/.cpp         ← toca seções em loop
│   │   ├── TransposeEngine.h/.cpp    ← NTR/NTT por canal
│   │   ├── ChordDetector.h/.cpp      ← detecta acorde da mão esquerda
│   │   └── MidiRouter.h/.cpp         ← roteador central de MIDI
│   │
│   └── UI/
│       ├── StylePanel.h/.cpp         ← botões Intro/Main/Fill/Ending
│       ├── TransportPanel.h/.cpp     ← BPM, start/stop, transpose
│       ├── MixerPanel.h/.cpp         ← volume/mute por parte
│       ├── ChordDisplay.h/.cpp       ← exibe acorde detectado
│       └── SetupDialog.h/.cpp        ← configura MIDI IN + SF2
```

---

## 4. Módulo 1: FluidSynth Engine

### FluidSynthEngine — responsabilidades
- Inicializar FluidSynth com driver de áudio via **JUCE AudioDeviceManager**
  (substituir o driver FluidSynth nativo pelo callback do JUCE)
- Carregar arquivos `.sf2` em runtime
- Expor `sendMidiMessage(const juce::MidiMessage&)` → FluidSynth API
- Suportar **16 canais simultâneos**, cada um com Program Change independente
- Tratar o **fallback de Bank Select**: se MSB ≠ 0 (Yamaha XG/Mega Voice), 
  tentar MSB=0 primeiro; se não encontrar, usar somente PC com banco padrão

```cpp
// Interface pública mínima
class FluidSynthEngine {
public:
    bool loadSoundFont(const juce::File& sf2File);
    void sendMidiMessage(const juce::MidiMessage& msg);
    void sendAllNotesOff();
    // chamado pelo AudioIOCallback do JUCE:
    void processAudio(float** outputs, int numSamples);
private:
    fluid_settings_t* settings;
    fluid_synth_t*    synth;
    int               soundFontId;
    // bank fallback table por canal
    std::array<int,16> channelBankMsb, channelBankLsb, channelPC;
};
```

### Integração JUCE ↔ FluidSynth (áudio)
O FluidSynth será rodado em **modo "no-driver"** (`fluid_settings_setstr(settings,"audio.driver","no")`). 
O JUCE AudioIOCallback chamará `fluid_synth_write_float()` no audio thread:

```cpp
void audioDeviceIOCallbackWithContext(const float* const* inputs,
                                      int numInputChannels,
                                      float* const* outputs,
                                      int numOutputChannels,
                                      int numSamples,
                                      const AudioIODeviceCallbackContext&) override
{
    fluid_synth_write_float(synth, numSamples,
                            outputs[0], 0, 1,   // left channel
                            outputs[1], 0, 1);  // right channel
}
```

---

## 5. Módulo 2: STY Parser

### Estrutura binária do arquivo STY (SFF2)

```
[MIDI SMF Header]  "MThd" - 4 bytes track length header
[MIDI Track]       "MTrk" - trilha única (SMF Type 0)
   Compasso 1:     Marker "SFF2" (ou "SFF1")
                   Marker "SInt"
                   SysEx (GM/XG reset)
                   CC#0 (Bank MSB) + CC#32 (Bank LSB) + PC × 8 canais
                   ---
   Compasso N:     Marker "Intro A" | "Main A" ... "Ending C"
                   [notas MIDI, CC, pitchbend por compasso]
                   ...
   End of Track    0xFF 0x2F 0x00
[CASM Section]     "CASM" 4 bytes + length
   Sub-blocos:     "Cseg" + "Ctab"(SFF1) ou "Ctb2"(SFF2)
[OTS Section]      "OTS " 4 bytes + length  (opcional)
[MDB Section]      "MDB " 4 bytes + length  (opcional)
```

### StyParser — algoritmo de leitura

```cpp
StyFile StyParser::parse(const juce::File& styFile) {
    // 1. Abrir como MemoryBlock
    // 2. Usar juce::MidiFile::readFrom() para extrair a trilha MIDI
    // 3. Iterar MidiMessageSequence procurando Marker Events (0xFF 0x06)
    // 4. Construir mapa: markerName → {startTick, endTick}
    // 5. Após "End of Track": ler seções CASM, OTS, MDB manualmente
    //    como raw bytes (juce::MidiFile não lê além do EOT)
}
```

### Enum das seções

```cpp
enum class StyleSection {
    SInt,
    IntroA, IntroB, IntroC,
    MainA,  MainB,  MainC,  MainD,
    FillInAA, FillInAB, FillInBA, FillInBB,
    FillInCC, FillInDD,
    BreakFill,
    EndingA, EndingB, EndingC
};
```

### CasmParser — decodifica bloco Ctb2 (SFF2)

Para cada entrada Ctb2 (um por canal de origem):

| Offset | Campo | Significado |
|---|---|---|
| 0 | SourceChannel | Canal MIDI de origem (0-15) |
| 1 | DestChannel | Canal de destino (8-15 → ch 9-16) |
| 2 | CloseHarmony | Bit flag: harmonia fechada |
| 3 | NTR | Note Transposition Rule (0=ROOT, 1=GUITAR, 2=BASS, 3=BYPASS) |
| 4 | NTT | Note Transposition Table (0=BYPASS, 1=MELODY, 2=CHORD) |
| 5 | HighKey | Nota MIDI máxima permitida antes de oitavar |
| 6 | NoteLowLimit | Nota MIDI mínima do canal |
| 7 | NoteHighLimit | Nota MIDI máxima do canal |
| 8 | RTag | Retrigger rule |
| 9 | MuteFlags | Bitfield: mute por tipo de acorde |

---

## 6. Módulo 3: Chord Detector

### Modos de detecção (configurável pelo usuário)

| Modo | Descrição |
|---|---|
| **Single Finger** | 1 tecla = Maior; + tecla branca à esq = Menor; + tecla preta à esq = 7ª |
| **Fingered** | Detecta acorde real pelas notas pressionadas simultaneamente |
| **Fingered on Bass** | Igual ao Fingered mas a nota mais grave define o baixo |
| **Full Keyboard** | Sem split; acordes detectados em todo o teclado |

### Algoritmo Fingered (principal)

```cpp
struct ChordInfo {
    int   root;        // 0=C, 1=C#, ..., 11=B
    ChordType type;    // Major, Minor, 7th, Maj7, m7, dim, aug, sus2, sus4...
    int   bassNote;    // pode diferir do root (inversões)
};

ChordInfo ChordDetector::detect(const std::vector<int>& midiNotes) {
    // 1. Normalizar notas para dentro de uma oitava (% 12)
    // 2. Testar contra tabela de intervalos de todos os tipos de acorde
    // 3. Para cada raiz possível (0-11): calcular distâncias entre notas
    // 4. Retornar o melhor match por pontuação de completude
}
```

### Tabela de intervalos (exemplos)

```
Major:      [0, 4, 7]
Minor:      [0, 3, 7]
7th:        [0, 4, 7, 10]
Major7:     [0, 4, 7, 11]
Minor7:     [0, 3, 7, 10]
Diminished: [0, 3, 6]
Aug:        [0, 4, 8]
Sus2:       [0, 2, 7]
Sus4:       [0, 5, 7]
```

---

## 7. Módulo 4: Style Engine (coração do programa)

### Máquina de estados do arranjador

```
         ┌──────────────────────────────────────────┐
         │                  IDLE                    │
         └──────────┬───────────────────────────────┘
                    │ [START pressed]
         ┌──────────▼───────────────────────────────┐
         │               INTRO playing              │◀─ botão Intro A/B/C
         └──────────┬───────────────────────────────┘
                    │ [Intro ends → auto-advance]
         ┌──────────▼───────────────────────────────┐
    ┌───▶│            MAIN LOOP (A/B/C/D)           │◀─ botão Main A/B/C/D
    │    └──────────┬────────────┬──────────────────┘
    │               │ [Fill btn] │ [Ending btn]
    │    ┌──────────▼──────┐    ┌▼─────────────────┐
    │    │   FILL playing  │    │  ENDING playing   │
    │    └──────────┬──────┘    └──────────────────┬┘
    └───────────────┘                              │
                                        ┌──────────▼──────┐
                                        │      IDLE        │
                                        └─────────────────┘
```

### LoopPlayer — toca seções em loop

```cpp
class LoopPlayer {
public:
    // Configura a seção para tocar (dados já extraídos pelo StyParser)
    void setSection(const MidiMessageSequence& seq, double bpm, int ppq);
    void start();
    void stop();
    // Chamado pelo timer de alta resolução (juce::HighResolutionTimer)
    void tick(int64_t currentSamplePos);

private:
    MidiMessageSequence section;
    int64_t  loopStartTick, loopEndTick;
    int64_t  playheadTick;
    double   samplesPerTick; // = sampleRate * 60.0 / (bpm * ppq)
    std::function<void(const MidiMessage&)> onMidiEvent;
};
```

### TransposeEngine — NTR/NTT por canal

```cpp
// Dado o acorde detectado e a regra CASM do canal, transpõe a nota
int TransposeEngine::transposeNote(int originalNote,
                                   const ChordInfo& chord,
                                   NTR rule, NTT table)
{
    switch (rule) {
        case NTR::ROOT:   return transposeByRoot(originalNote, chord);
        case NTR::GUITAR: return transposeByChordVoicing(originalNote, chord);
        case NTR::BASS:   return transposeBassLine(originalNote, chord);
        case NTR::BYPASS: return originalNote; // drums
    }
}
```

### Regras de transposição por tipo de canal

| Canal de destino | Regra típica | Comportamento |
|---|---|---|
| Rhythm (ch10) | BYPASS | Bateria não transpõe |
| Sub-Rhythm (ch9) | BYPASS | Percussão adicional não transpõe |
| Bass (ch11) | ROOT | Transpõe a nota mais grave para a fundamental do acorde |
| Chord 1/2 (ch12,13) | GUITAR | Remapeia notas dentro das notas do acorde |
| Pad (ch14) | CHORD | Sustenta notas do acorde |
| Phrase 1/2 (ch15,16) | MELODY | Transpõe mantendo contorno melódico |

---

## 8. Módulo 5: MIDI Router

### Divisão do teclado (split point configurável, padrão: F#3 = MIDI 54)

```
┌──────────────────────────────────────────────────────┐
│  C1 ──────────── F#3 │ G3 ──────────────────── C8   │
│  LEFT HAND           │ RIGHT HAND                    │
│  (Chord Detector)    │ (→ Canal 1, voz principal)    │
└──────────────────────────────────────────────────────┘
```

### Fluxo de mensagens MIDI IN

```
MIDI IN → juce::MidiInput callback (message thread)
       → MidiRouter::handleIncomingMidiMessage()
           ├─ nota < splitPoint → ChordDetector::addNote()
           │                   → StyleEngine::updateChord()
           └─ nota ≥ splitPoint → FluidSynthEngine::sendMidi(ch=0, note, vel)
```

### Tratamento de Program Change vindos do arquivo STY

```cpp
void MidiRouter::routeStyleMessage(const juce::MidiMessage& msg) {
    if (msg.isProgramChange()) {
        int ch  = msg.getChannel() - 1;  // 0-indexed
        int pc  = msg.getProgramChangeNumber();
        int msb = channelBankMsb[ch];
        int lsb = channelBankLsb[ch];
        // Fallback: se MSB indica Yamaha XG/Mega Voice → usar MSB=0
        if (msb > 0 && !soundFont.hasBankVoice(msb, lsb, pc))
            msb = lsb = 0;
        soundFont.programChange(ch, msb, lsb, pc);
    } else if (msg.isController()) {
        int cc = msg.getControllerNumber();
        if (cc == 0)  channelBankMsb[msg.getChannel()-1] = msg.getControllerValue();
        if (cc == 32) channelBankLsb[msg.getChannel()-1] = msg.getControllerValue();
        soundFont.sendMidi(msg);
    } else {
        soundFont.sendMidi(msg); // notas, pitchbend, aftertouch, etc.
    }
}
```

---

## 9. Interface do Usuário (JUCE Component)

### Layout principal

```
┌────────────────────────────────────────────────────────────────┐
│  [STY FILE: Bossa Nova.sty]  BPM: [120]▲▼  Transpose: [-2]▲▼ │
├────────────────────────────────────────────────────────────────┤
│  CHORD: [Cmaj7]    SPLIT: [F#3]    MODE: [Fingered]           │
├──────────────┬─────────────────────────────────────────────────┤
│  INTRO       │  ●A  ●B  ●C                                     │
│  MAIN        │  ●A  ●B  ●C  ●D                                 │
│  FILL IN     │  ●AA ●AB ●BA ●BB                                │
│  ENDING      │  ●A  ●B  ●C                                     │
├──────────────┴─────────────────────────────────────────────────┤
│  PARTS:  [Sub-Rhy][Rhythm][Bass][Chord1][Chord2][Pad][Phr1][Phr2]│
│          VOL: 80    100    90    75      70      60   85    80  │
│          MUTE:□      □      □     □       □       □    □    □  │
├────────────────────────────────────────────────────────────────┤
│  [START/STOP]    [SYNC START]    [SETUP...]                    │
└────────────────────────────────────────────────────────────────┘
```

### SetupDialog (configuração inicial)

- **MIDI Input**: dropdown com `juce::MidiInput::getAvailableDevices()`
- **SoundFont**: file browser para `.sf2`, botão "Load"
- **Split Point**: seletor de nota (keyboard visual)
- **Chord Mode**: Single / Fingered / Fingered on Bass / Full Keyboard
- **Latency**: buffer size do AudioDeviceManager

---

## 10. Fases de Desenvolvimento

### Fase 1 — Fundação (2–3 semanas)
- [ ] Setup CMake: JUCE 8 + FluidSynth como subdirectory
- [ ] `FluidSynthEngine`: carregar SF2, receber MIDI, gerar áudio via JUCE callback
- [ ] `SoundFontManager`: UI para selecionar/carregar SF2
- [ ] Teste: tocar notas pelo piano roll do JUCE → FluidSynth → áudio

### Fase 2 — MIDI IN e Chord Detector (1–2 semanas)
- [ ] `MidiRouter`: captura MIDI IN via `juce::MidiInput`
- [ ] Split point configurável
- [ ] `ChordDetector`: modo Fingered funcional
- [ ] Visualizar acorde detectado na UI

### Fase 3 — STY Parser (2–3 semanas)
- [ ] `StyParser`: ler SMF, extrair marcadores, mapear seções
- [ ] `CasmParser`: decodificar Ctb2, mapear canais src→dest, ler NTR/NTT
- [ ] Testes: carregar 5 arquivos STY diferentes, validar seções extraídas
- [ ] Parser para seções OTS e MDB (opcional nesta fase)

### Fase 4 — Style Engine (3–4 semanas)
- [ ] `LoopPlayer`: toca Main A em loop usando `juce::HighResolutionTimer`
- [ ] Máquina de estados: Idle → Intro → Main → Fill → Ending
- [ ] `TransposeEngine`: NTR ROOT e BYPASS funcionais
- [ ] Roteamento CASM: redirecionar canais de origem para destino correto
- [ ] Sync: transições quantizadas ao fim do compasso

### Fase 5 — Transposição Completa (2 semanas)
- [ ] NTR GUITAR e BASS
- [ ] NTT MELODY e CHORD (lookup tables)
- [ ] Mute flags por tipo de acorde (maior/menor/7ª etc.)
- [ ] Retrigger rules

### Fase 6 — UI Completa e Polimento (2 semanas)
- [ ] `StylePanel` com botões visuais iluminados
- [ ] `MixerPanel`: volume e mute por parte em tempo real
- [ ] `TransportPanel`: BPM tap tempo, transpose global
- [ ] `SetupDialog` completo
- [ ] Persistência de configuração (XML via JUCE PropertiesFile)

### Fase 7 — Testes e Compatibilidade (1–2 semanas)
- [ ] Testar SFF1 vs SFF2
- [ ] Fallback de Bank Select (XG → GM)
- [ ] Tratamento de SysEx (GM reset, XG mode)
- [ ] Múltiplos SF2 files (selecionar qual SF2 por canal)
- [ ] Teste com OneManBand .sty files e arquivos PSR-S970/PSR-EW425

---

## 11. Dependências e Setup Inicial

### CMakeLists.txt (estrutura)

```cmake
cmake_minimum_required(VERSION 3.24)
project(PSREmulator VERSION 1.0.0)

# JUCE via FetchContent ou submodule
add_subdirectory(libs/JUCE)

# FluidSynth como submodule ou via vcpkg
find_package(FluidSynth REQUIRED)
# ou: add_subdirectory(libs/fluidsynth)

juce_add_gui_app(PSREmulator
    PRODUCT_NAME "PSR Emulator"
)

target_sources(PSREmulator PRIVATE
    Source/Main.cpp
    Source/SoundFont/FluidSynthEngine.cpp
    # ...
)

target_link_libraries(PSREmulator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_gui_basics
        FluidSynth::libfluidsynth
)
```

### Primeira etapa de build

```bash
git clone --recurse-submodules https://github.com/seu-usuario/psr-emulator
cd psr-emulator
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

---

## 12. Referências Técnicas Chave

| Recurso | URL / Localização |
|---|---|
| JUCE MidiFile | `docs.juce.com/master/classMidiFile.html` |
| JUCE MidiMessage | `docs.juce.com/master/classMidiMessage.html` |
| JUCE MidiMessageSequence | `docs.juce.com/master/classMidiMessageSequence.html` |
| FluidSynth API | `www.fluidsynth.org/api/` |
| STY Format (Jososoft) | `jososoft.dk/yamaha/articles/style_*.htm` |
| STY CASM Structure | `heikoplate.de` — structure of style files |
| SFF2 Tools (C++) | `github.com/bures/sff2-tools` |
| JZZ-midi-STY (parser JS ref) | `github.com/jazz-soft/JZZ-midi-STY` |
| Mixage Style Format | `mixagesoftware.com/en/midikit/help/Style_Format.html` |
| NotebookLM do projeto | ID: `6af0f474-3f00-4169-a550-2bd08187bbb6` |
