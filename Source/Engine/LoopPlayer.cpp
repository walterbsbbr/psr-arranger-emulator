#include "LoopPlayer.h"
#include <juce_core/juce_core.h>

LoopPlayer::LoopPlayer (MidiCallback callback)
    : midiCallback (std::move (callback))
{
}

LoopPlayer::~LoopPlayer()
{
    stop();
}

void LoopPlayer::prepareToPlay (double bpm, int ppq, double /*sampleRate*/)
{
    currentBpm = bpm;
    currentPpq = ppq;
    // nanosegundos por tick = (60 / BPM) / PPQ * 1e9
    nsPerTick  = (60.0 / bpm) / (double)ppq * 1.0e9;
}

void LoopPlayer::setBpm (double bpm)
{
    currentBpm = bpm;
    nsPerTick  = (60.0 / bpm) / (double)currentPpq * 1.0e9;
}

void LoopPlayer::loadSection (const StyFile::SectionData& sec,
                               StyleSection id,
                               bool loop)
{
    currentEvents = sec.events;
    totalTicks    = sec.endTick - sec.startTick;
    if (totalTicks <= 0) totalTicks = currentPpq * 4; // fallback: 1 compasso

    playheadTick  = 0;
    nextEventIdx  = 0;
    looping       = loop;
    activeSection = id;
}

void LoopPlayer::playSection (const StyFile::SectionData& section,
                               StyleSection sectionId,
                               bool loop)
{
    stop();
    {
        juce::ScopedLock sl (queueLock);
        queued.pending = false;
    }
    loadSection (section, sectionId, loop);
    lastCallNs = (int64_t)(juce::Time::getMillisecondCounterHiRes() * 1.0e6);  // ns, igual ao hiResTimerCallback
    playing = true;
    paused  = false;
    startTimer (TIMER_INTERVAL_MS);
}

void LoopPlayer::queueSection (const StyFile::SectionData& section,
                                StyleSection sectionId,
                                bool loop)
{
    juce::ScopedLock sl (queueLock);
    queued.data    = section;
    queued.id      = sectionId;
    queued.loop    = loop;
    queued.pending = true;
}

void LoopPlayer::stop()
{
    stopTimer();
    playing = false;
    paused  = false;
    playheadTick = 0;
    nextEventIdx = 0;
    activeSection = StyleSection::Unknown;
}

void LoopPlayer::pause()  { paused = true;  }
void LoopPlayer::resume() { paused = false; }

// ─── hiResTimerCallback ───────────────────────────────────────────────────────
void LoopPlayer::hiResTimerCallback()
{
    if (!playing || paused) return;

    // Tempo atual em nanosegundos (aproximado via juce::Time)
    const int64_t nowNs = (int64_t)(juce::Time::getMillisecondCounterHiRes() * 1.0e6);
    if (lastCallNs == 0) { lastCallNs = nowNs; return; }

    const int64_t elapsedNs = nowNs - lastCallNs;

    // Quantos ticks se passaram neste intervalo?
    const int ticksElapsed = (nsPerTick > 0)
                           ? (int)(elapsedNs / nsPerTick)
                           : 0;
    if (ticksElapsed <= 0) return;

    // Consumir apenas os ns correspondentes aos ticks avançados,
    // preservando o restante para a próxima chamada.
    // Sem isso, a divisão inteira perde o tempo fracionário e o playhead
    // nunca avança quando nsPerTick > 1ms (ex: 120 BPM / 480 PPQ).
    lastCallNs += (int64_t)(ticksElapsed * nsPerTick);

    // Avança o playhead e dispara os eventos do intervalo
    const int startTick  = playheadTick;
    int       targetTick = playheadTick + ticksElapsed;

    // Verifica se passou pelo fim da seção
    if (targetTick >= totalTicks)
    {
        // Emite eventos até o fim
        for (int i = nextEventIdx; i < currentEvents.getNumEvents(); ++i)
        {
            const auto& msg = currentEvents.getEventPointer(i)->message;
            if ((int)msg.getTimeStamp() < totalTicks)
                midiCallback (msg);
        }

        // Verifica se há seção enfileirada
        {
            juce::ScopedLock sl (queueLock);
            if (queued.pending)
            {
                loadSection (queued.data, queued.id, queued.loop);
                queued.pending = false;
                return;
            }
        }

        if (looping)
        {
            // Reinicia o loop
            playheadTick = targetTick % totalTicks;
            nextEventIdx = 0;
            targetTick   = playheadTick;
        }
        else
        {
            // Seção one-shot terminou
            playing = false;
            stopTimer();
            if (onSectionEnded)
                juce::MessageManager::callAsync ([this, sec = activeSection] {
                    onSectionEnded (sec);
                });
            return;
        }
    }

    // Emite os eventos no intervalo [playheadTick, targetTick)
    for (int i = nextEventIdx; i < currentEvents.getNumEvents(); ++i)
    {
        const auto* ev = currentEvents.getEventPointer(i);
        const int   t  = (int)ev->message.getTimeStamp();
        if (t >= targetTick) break;
        midiCallback (ev->message);
        nextEventIdx = i + 1;
    }

    playheadTick = targetTick;
}
