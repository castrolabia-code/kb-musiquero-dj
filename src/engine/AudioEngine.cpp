#include "engine/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace kb::engine
{
    static constexpr double kParamSmoothingSeconds = 0.02;   // 20 ms: sin "zipper noise"

    AudioEngine::AudioEngine() = default;

    AudioEngine::~AudioEngine() = default;

    // =========================================================================
    //  Curvas
    // =========================================================================
    float AudioEngine::faderGain (float p) noexcept
    {
        // Curva cuadrática (aprox. logarítmica): 1 -> 0 dB, 0.5 -> -12 dB, 0 -> silencio.
        p = juce::jlimit (0.0f, 1.0f, p);
        return p * p;
    }

    void AudioEngine::crossfaderGains (float position, float& gainA, float& gainB) noexcept
    {
        crossfaderGains (position, CrossfaderCurve::smooth, false, gainA, gainB);
    }

    void AudioEngine::crossfaderGains (float position, CrossfaderCurve curve, bool reverse,
                                       float& gainA, float& gainB) noexcept
    {
        auto x = (juce::jlimit (-1.0f, 1.0f, position) + 1.0f) * 0.5f;   // 0 = deck 1, 1 = deck 2
        if (reverse)
            x = 1.0f - x;   // inversión ("hamster")

        if (curve == CrossfaderCurve::smooth)
        {
            // Potencia constante: al centro ambos a -3 dB.
            gainA = std::cos (x * juce::MathConstants<float>::halfPi);
            gainB = std::sin (x * juce::MathConstants<float>::halfPi);
        }
        else
        {
            // Corte: los dos al máximo; cada uno cae solo en la punta opuesta.
            const auto w = curve == CrossfaderCurve::cut ? 0.10f : 0.02f;
            gainA = juce::jlimit (0.0f, 1.0f, (1.0f - x) / w);
            gainB = juce::jlimit (0.0f, 1.0f, x / w);
        }

        if (gainA < 1.0e-6f) gainA = 0.0f;
        if (gainB < 1.0e-6f) gainB = 0.0f;
    }

    // =========================================================================
    //  Ciclo de vida
    // =========================================================================
    void AudioEngine::prepare (const EngineConfig& config)
    {
        jassert (config.sampleRate > 0.0);
        jassert (config.maxBlockSize > 0);

        const auto channels = std::clamp (config.numOutputChannels, 0, kMaxOutputChannels);
        const auto block    = std::max (config.maxBlockSize, 1);

        // Reserva única: process() trabaja siempre dentro de este buffer.
        work.setSize (numBuses, block, false, true, false);
        work.clear();

        for (auto& d : decks)
            d.prepare (config.sampleRate, block);
        for (auto& s : strips)
            s.prepare (config.sampleRate);
        sampler.prepare (config.sampleRate);
        for (auto& u : fxUnits)
            u.prepare (config.sampleRate, block);
        masterLimiter.prepare (config.sampleRate);
        phonesLimiter.prepare (config.sampleRate);

        for (int i = 0; i < kNumDecks; ++i)
        {
            for (auto* sv : { &smoothedDeckGain[(size_t) i], &smoothedCueGain[(size_t) i] })
            {
                sv->reset (config.sampleRate, kParamSmoothingSeconds);
                sv->setCurrentAndTargetValue (0.0f);
            }
        }
        for (auto* sv : { &smoothedMaster, &smoothedPhones, &smoothedMixCue, &smoothedMixMaster })
        {
            sv->reset (config.sampleRate, kParamSmoothingSeconds);
            sv->setCurrentAndTargetValue (0.0f);
        }

        sampleRate.store (config.sampleRate, std::memory_order_relaxed);
        maxBlockSize.store (block, std::memory_order_relaxed);
        numOutputs.store (channels, std::memory_order_relaxed);
        processedBlocks.store (0, std::memory_order_relaxed);
        prepared.store (true, std::memory_order_release);
    }

    void AudioEngine::release()
    {
        prepared.store (false, std::memory_order_release);
        work.setSize (0, 0);
    }

    // =========================================================================
    //  Control (hilo de mensajes)
    // =========================================================================
    bool AudioEngine::send (const Command& c)
    {
        const auto ok = commands.push (c);
        jassert (ok);   // 255 comandos sin procesar: el audio está detenido o colgado
        return ok;
    }

    bool AudioEngine::loadTrack (int deck, std::shared_ptr<TrackBuffer> track)
    {
        if (! juce::isPositiveAndBelow (deck, kNumDecks))
            return false;

        Command c;
        c.type  = Command::Type::load;
        c.deck  = (std::int8_t) deck;
        c.track = track.get();

        if (! send (c))
            return false;

        if (track != nullptr)
            liveTracks.push_back (std::move (track));
        return true;
    }

    void AudioEngine::ejectTrack (int deck) { loadTrack (deck, nullptr); }

    // =========================================================================
    //  Performance (Fase 4)
    // =========================================================================
    bool AudioEngine::sendDeck (Command::Type type, int deck, double value, int index)
    {
        if (! juce::isPositiveAndBelow (deck, kNumDecks))
            return false;
        Command c;
        c.type  = type;
        c.deck  = (std::int8_t) deck;
        c.value = value;
        c.index = (std::int8_t) index;
        return send (c);
    }

    void AudioEngine::hotCue (int d, int i)                    { sendDeck (Command::Type::hotCue, d, 0.0, i); }
    void AudioEngine::setHotCue (int d, int i, double seconds) { sendDeck (Command::Type::setHotCue, d, seconds, i); }
    void AudioEngine::autoLoop (int d, double beats)           { sendDeck (Command::Type::autoLoop, d, beats); }
    void AudioEngine::loopRoll (int d, double beats, bool on)  { sendDeck (Command::Type::loopRoll, d, beats, on ? 1 : 0); }
    void AudioEngine::loopIn (int d)                           { sendDeck (Command::Type::loopIn, d); }
    void AudioEngine::loopOut (int d)                          { sendDeck (Command::Type::loopOut, d); }
    void AudioEngine::loopHalve (int d)                        { sendDeck (Command::Type::loopHalve, d); }
    void AudioEngine::loopDouble (int d)                       { sendDeck (Command::Type::loopDouble, d); }
    void AudioEngine::loopExit (int d)                         { sendDeck (Command::Type::loopExit, d); }
    void AudioEngine::setSlip (int d, bool on)                 { sendDeck (Command::Type::slip, d, 0.0, on ? 1 : 0); }
    void AudioEngine::setReverse (int d, bool on)              { sendDeck (Command::Type::reverse, d, 0.0, on ? 1 : 0); }
    void AudioEngine::setDeckEffect (int d, Deck::Effect e, bool on) { sendDeck (Command::Type::effect, d, on ? 1.0 : 0.0, (int) e); }
    void AudioEngine::scratchBegin (int d)                     { sendDeck (Command::Type::scratchBegin, d); }
    void AudioEngine::scratchMove (int d, double seconds)      { sendDeck (Command::Type::scratchMove, d, seconds); }
    void AudioEngine::scratchEnd (int d)                       { sendDeck (Command::Type::scratchEnd, d); }
    void AudioEngine::nudge (int d, double amount)             { sendDeck (Command::Type::nudge, d, amount); }

    bool AudioEngine::loadSample (int slot, std::shared_ptr<TrackBuffer> sample)
    {
        if (! juce::isPositiveAndBelow (slot, Sampler::kNumSlots))
            return false;
        Command c;
        c.type  = Command::Type::sampleLoad;
        c.index = (std::int8_t) slot;
        c.track = sample.get();
        if (! send (c))
            return false;
        if (sample != nullptr)
            liveTracks.push_back (std::move (sample));
        return true;
    }

    void AudioEngine::triggerSample (int slot)
    {
        if (juce::isPositiveAndBelow (slot, Sampler::kNumSlots))
        {
            Command c;
            c.type = Command::Type::sampleTrigger;
            c.index = (std::int8_t) slot;
            send (c);
        }
    }

    void AudioEngine::stopSample (int slot)
    {
        if (juce::isPositiveAndBelow (slot, Sampler::kNumSlots))
        {
            Command c;
            c.type = Command::Type::sampleStop;
            c.index = (std::int8_t) slot;
            send (c);
        }
    }

    void AudioEngine::play (int deck)       { if (juce::isPositiveAndBelow (deck, kNumDecks)) send ({ Command::Type::play,       (std::int8_t) deck, nullptr, 0.0 }); }
    void AudioEngine::pause (int deck)      { if (juce::isPositiveAndBelow (deck, kNumDecks)) send ({ Command::Type::pause,      (std::int8_t) deck, nullptr, 0.0 }); }
    void AudioEngine::togglePlay (int deck) { if (juce::isPositiveAndBelow (deck, kNumDecks)) send ({ Command::Type::togglePlay, (std::int8_t) deck, nullptr, 0.0 }); }
    void AudioEngine::cueDown (int deck)    { if (juce::isPositiveAndBelow (deck, kNumDecks)) send ({ Command::Type::cueDown,    (std::int8_t) deck, nullptr, 0.0 }); }
    void AudioEngine::cueUp (int deck)      { if (juce::isPositiveAndBelow (deck, kNumDecks)) send ({ Command::Type::cueUp,      (std::int8_t) deck, nullptr, 0.0 }); }
    void AudioEngine::seek (int deck, double seconds)
    {
        if (juce::isPositiveAndBelow (deck, kNumDecks))
            send ({ Command::Type::seek, (std::int8_t) deck, nullptr, seconds });
    }

    namespace
    {
        void store01 (std::atomic<float>& a, float v) noexcept { a.store (juce::jlimit (0.0f, 1.0f, v), std::memory_order_relaxed); }
        float load (const std::atomic<float>& a) noexcept      { return a.load (std::memory_order_relaxed); }
    }

    void  AudioEngine::setChannelVolume (int d, float p) noexcept { if (juce::isPositiveAndBelow (d, kNumDecks)) store01 (channel[(size_t) d].volume, p); }
    float AudioEngine::getChannelVolume (int d) const noexcept    { return juce::isPositiveAndBelow (d, kNumDecks) ? load (channel[(size_t) d].volume) : 0.0f; }
    void  AudioEngine::setTrim (int d, float p) noexcept          { if (juce::isPositiveAndBelow (d, kNumDecks)) store01 (channel[(size_t) d].trim, p); }
    float AudioEngine::getTrim (int d) const noexcept             { return juce::isPositiveAndBelow (d, kNumDecks) ? load (channel[(size_t) d].trim) : 0.5f; }
    void  AudioEngine::setFilter (int d, float p) noexcept        { if (juce::isPositiveAndBelow (d, kNumDecks)) store01 (channel[(size_t) d].filter, p); }
    float AudioEngine::getFilter (int d) const noexcept           { return juce::isPositiveAndBelow (d, kNumDecks) ? load (channel[(size_t) d].filter) : 0.5f; }

    void AudioEngine::setEq (int d, EqBand band, float p) noexcept
    {
        if (! juce::isPositiveAndBelow (d, kNumDecks))
            return;
        auto& c = channel[(size_t) d];
        store01 (band == EqBand::low ? c.low : band == EqBand::mid ? c.mid : c.high, p);
    }

    float AudioEngine::getEq (int d, EqBand band) const noexcept
    {
        if (! juce::isPositiveAndBelow (d, kNumDecks))
            return 0.5f;
        const auto& c = channel[(size_t) d];
        return load (band == EqBand::low ? c.low : band == EqBand::mid ? c.mid : c.high);
    }

    void AudioEngine::setChannelCue (int d, bool on) noexcept
    {
        if (juce::isPositiveAndBelow (d, kNumDecks))
            channel[(size_t) d].cue.store (on, std::memory_order_relaxed);
    }

    bool AudioEngine::getChannelCue (int d) const noexcept
    {
        return juce::isPositiveAndBelow (d, kNumDecks) && channel[(size_t) d].cue.load (std::memory_order_relaxed);
    }

    // =========================================================================
    //  Tempo y SYNC
    // =========================================================================
    void AudioEngine::setTempo (int d, float percent) noexcept
    {
        if (! juce::isPositiveAndBelow (d, kNumDecks))
            return;
        channel[(size_t) d].tempo.store (juce::jlimit (-kMaxTempoPercent, kMaxTempoPercent, percent), std::memory_order_relaxed);
        if (channel[(size_t) d].sync.load (std::memory_order_relaxed))
            setSync (d, false);   // mover el tempo a mano suelta el SYNC
    }

    float AudioEngine::getTempo (int d) const noexcept
    {
        return juce::isPositiveAndBelow (d, kNumDecks) ? channel[(size_t) d].tempo.load (std::memory_order_relaxed) : 0.0f;
    }

    bool AudioEngine::getSync (int d) const noexcept
    {
        return juce::isPositiveAndBelow (d, kNumDecks) && channel[(size_t) d].sync.load (std::memory_order_relaxed);
    }

    void AudioEngine::toggleSync (int d) { setSync (d, ! getSync (d)); }

    void AudioEngine::setSync (int d, bool on)
    {
        if (! juce::isPositiveAndBelow (d, kNumDecks))
            return;
        // El cambio de estado lo hace el hilo de audio (necesita las posiciones exactas).
        send ({ on ? Command::Type::syncOn : Command::Type::syncOff, (std::int8_t) d, nullptr, 0.0 });
        // Reflejo inmediato para la UI; el audio lo confirma en el próximo bloque.
        channel[(size_t) d].sync.store (on, std::memory_order_relaxed);
        if (on)
            channel[(size_t) (1 - d)].sync.store (false, std::memory_order_relaxed);
    }

    double AudioEngine::syncRatio (double masterBpm, double slaveBpm) noexcept
    {
        if (masterBpm <= 0.0 || slaveBpm <= 0.0)
            return 1.0;
        const auto r = masterBpm / slaveBpm;
        double best = r;
        for (auto k : { 0.5, 2.0 })
            if (std::abs (std::log (r * k)) < std::abs (std::log (best)))
                best = r * k;
        return best;
    }

    bool AudioEngine::syncPhaseError (int slave, double ratio, double& error) const noexcept
    {
        const auto& s = decks[(size_t) slave];
        const auto& m = decks[(size_t) (1 - slave)];
        const auto* ts = s.getTrack();
        const auto* tm = m.getTrack();
        if (ts == nullptr || tm == nullptr || ts->getBpm() <= 0.0 || tm->getBpm() <= 0.0)
            return false;

        // Beats transcurridos desde la grilla de cada pista.
        const auto masterBeats = (m.getPositionSecondsRT() - tm->getFirstBeatSeconds()) * tm->getBpm() / 60.0;
        const auto slaveBeats  = (s.getPositionSecondsRT() - ts->getFirstBeatSeconds()) * ts->getBpm() / 60.0;

        // Con mitad/doble tempo, 1 beat del master equivale a k beats del esclavo.
        const auto k = ratio * ts->getBpm() / (tm->getBpm() * m.getRate());
        const auto target = masterBeats * k;
        auto e = (target - std::floor (target)) - (slaveBeats - std::floor (slaveBeats));
        if (e > 0.5)  e -= 1.0;
        if (e < -0.5) e += 1.0;
        error = e;
        return true;
    }

    void AudioEngine::engageSync (int d) noexcept
    {
        auto& s = decks[(size_t) d];
        const auto& m = decks[(size_t) (1 - d)];
        const auto* ts = s.getTrack();
        const auto* tm = m.getTrack();
        if (ts == nullptr || tm == nullptr || ts->getBpm() <= 0.0 || tm->getBpm() <= 0.0)
            return;

        // 1) Mismo tempo.
        const auto masterFactor = 1.0 + channel[(size_t) (1 - d)].tempo.load (std::memory_order_relaxed) / 100.0;
        const auto ratio = syncRatio (tm->getBpm() * masterFactor, ts->getBpm());
        s.setRate (ratio);
        channel[(size_t) d].tempo.store ((float) juce::jlimit (-50.0, 50.0, (ratio - 1.0) * 100.0), std::memory_order_relaxed);

        // 2) Misma fase: si el master suena, el esclavo salta (con fundido) al beat alineado.
        double error = 0.0;
        if (m.isPlaying() && syncPhaseError (d, ratio, error) && std::abs (error) > 0.002)
            s.seek (s.getPositionSecondsRT() + error * 60.0 / ts->getBpm());
    }

    void AudioEngine::updateTempo (int numSamples) noexcept
    {
        const auto sr = sampleRate.load (std::memory_order_relaxed);
        for (int d = 0; d < kNumDecks; ++d)
        {
            auto& c = channel[(size_t) d];
            auto& deck = decks[(size_t) d];
            auto rate = 1.0 + c.tempo.load (std::memory_order_relaxed) / 100.0;

            if (c.sync.load (std::memory_order_relaxed))
            {
                const auto* ts = deck.getTrack();
                const auto* tm = decks[(size_t) (1 - d)].getTrack();
                if (ts != nullptr && tm != nullptr && ts->getBpm() > 0.0 && tm->getBpm() > 0.0)
                {
                    // Sigue el tempo del master (si el master mueve su fader, el esclavo acompaña).
                    const auto masterFactor = 1.0 + channel[(size_t) (1 - d)].tempo.load (std::memory_order_relaxed) / 100.0;
                    const auto ratio = syncRatio (tm->getBpm() * masterFactor, ts->getBpm());
                    c.tempo.store ((float) juce::jlimit (-50.0, 50.0, (ratio - 1.0) * 100.0), std::memory_order_relaxed);
                    rate = ratio;

                    // Corrección suave de fase (corrige la deriva en ~2 s, máximo ±1 %).
                    double error = 0.0;
                    if (deck.isPlaying() && decks[(size_t) (1 - d)].isPlaying()
                        && ! deck.isManipulated() && ! decks[(size_t) (1 - d)].isManipulated()
                        && ! deck.isLooping()
                        && syncPhaseError (d, ratio, error)
                        && std::abs (error) > 0.001 && sr > 0.0)
                    {
                        const auto beatsPerSecond = ts->getBpm() * ratio / 60.0;
                        const auto correction = juce::jlimit (-0.01, 0.01, (error / 2.0) / beatsPerSecond);
                        rate *= 1.0 + correction;
                    }
                }
            }
            deck.setRate (rate);
            deck.setPitchMode (c.keyLock.load (std::memory_order_relaxed),
                               (float) c.keyShift.load (std::memory_order_relaxed));
        }
        juce::ignoreUnused (numSamples);
    }

    void AudioEngine::setKeyLock (int d, bool on) noexcept
    {
        if (juce::isPositiveAndBelow (d, kNumDecks))
            channel[(size_t) d].keyLock.store (on, std::memory_order_relaxed);
    }

    bool AudioEngine::getKeyLock (int d) const noexcept
    {
        return juce::isPositiveAndBelow (d, kNumDecks) && channel[(size_t) d].keyLock.load (std::memory_order_relaxed);
    }

    void AudioEngine::setKeyShift (int d, int semitones) noexcept
    {
        if (juce::isPositiveAndBelow (d, kNumDecks))
            channel[(size_t) d].keyShift.store (juce::jlimit (-12, 12, semitones), std::memory_order_relaxed);
    }

    int AudioEngine::getKeyShift (int d) const noexcept
    {
        return juce::isPositiveAndBelow (d, kNumDecks) ? channel[(size_t) d].keyShift.load (std::memory_order_relaxed) : 0;
    }

    void AudioEngine::setCrossfader (float p) noexcept
    {
        crossfader.store (juce::jlimit (-1.0f, 1.0f, p), std::memory_order_relaxed);
        // Si el DJ toca el crossfader durante un AUTO MIX, manda el DJ.
        if (isAutoMixing())
            cancelAutoMix();
    }

    // =========================================================================
    //  AUTO MIX
    // =========================================================================
    int AudioEngine::autoMixSource (float cf, bool reverse, bool playingA, bool playingB) noexcept
    {
        if (reverse)
            cf = -cf;
        if (cf < -0.05f) return 0;
        if (cf >  0.05f) return 1;
        if (playingB && ! playingA) return 1;
        return 0;
    }

    void AudioEngine::startAutoMix (double seconds)
    {
        send ({ Command::Type::autoMixStart, 0, nullptr, juce::jlimit (1.0, 120.0, seconds) });
    }

    void AudioEngine::cancelAutoMix()
    {
        send ({ Command::Type::autoMixStop, 0, nullptr, 0.0 });
    }

    void AudioEngine::beginAutoMix (double seconds) noexcept
    {
        const auto sr = sampleRate.load (std::memory_order_relaxed);
        if (amActive || sr <= 0.0)
            return;

        const auto reverse = xfReverse.load (std::memory_order_relaxed);
        const auto source = autoMixSource (crossfader.load (std::memory_order_relaxed), reverse,
                                           decks[0].isPlaying(), decks[1].isPlaying());
        const auto target = 1 - source;
        auto& tgt = decks[(size_t) target];
        if (tgt.getTrack() == nullptr)
            return;   // no hay nada para mezclar

        // Tempo y golpes iguales, si los BPM no están demasiado lejos.
        const auto* ts = tgt.getTrack();
        const auto* tm = decks[(size_t) source].getTrack();
        if (! tgt.isPlaying() && ts != nullptr && tm != nullptr && ts->getBpm() > 0.0 && tm->getBpm() > 0.0)
        {
            const auto masterFactor = 1.0 + channel[(size_t) source].tempo.load (std::memory_order_relaxed) / 100.0;
            const auto ratio = syncRatio (tm->getBpm() * masterFactor, ts->getBpm());
            if (std::abs (ratio - 1.0) <= kAutoMixMaxTempoChange)
            {
                channel[(size_t) target].sync.store (true, std::memory_order_relaxed);
                channel[(size_t) source].sync.store (false, std::memory_order_relaxed);
            }
        }
        if (! tgt.isPlaying())
        {
            tgt.play();
            if (channel[(size_t) target].sync.load (std::memory_order_relaxed))
                engageSync (target);
        }

        amActive = true;
        amSource = source;
        amTarget = target;
        amTotal  = seconds * sr;
        amDone   = 0.0;
        amStart  = crossfader.load (std::memory_order_relaxed);
        // Punta del crossfader que deja sonando solo al deck que entra.
        amEnd    = ((target == 1) != reverse) ? 1.0f : -1.0f;
        autoMixProgress.store (0.0f, std::memory_order_relaxed);
    }

    void AudioEngine::advanceAutoMix (int n) noexcept
    {
        if (! amActive)
            return;

        amDone += n;
        const auto p = (float) juce::jlimit (0.0, 1.0, amDone / juce::jmax (1.0, amTotal));
        // Curva "S" suave: arranca y termina despacio.
        const auto s = p * p * (3.0f - 2.0f * p);
        crossfader.store (amStart + (amEnd - amStart) * s, std::memory_order_relaxed);

        // Bass swap a la mitad: nunca suenan los dos bajos juntos.
        amLowMultiplier[(size_t) amSource] = p < 0.5f ? 1.0f : 0.0f;
        amLowMultiplier[(size_t) amTarget] = p < 0.5f ? 0.0f : 1.0f;
        autoMixProgress.store (p, std::memory_order_relaxed);

        if (p >= 1.0f)
            endAutoMix (true);
    }

    void AudioEngine::endAutoMix (bool completed) noexcept
    {
        if (! amActive)
            return;
        if (completed)
        {
            crossfader.store (amEnd, std::memory_order_relaxed);
            decks[(size_t) amSource].pause();
            // La bandeja que entró queda libre (si siguiera en SYNC, seguiría a la que salió).
            channel[(size_t) amTarget].sync.store (false, std::memory_order_relaxed);
        }
        amLowMultiplier = { 1.0f, 1.0f };
        amActive = false;
        autoMixProgress.store (-1.0f, std::memory_order_relaxed);
    }
    void AudioEngine::setMasterVolume (float p) noexcept     { store01 (masterVolume, p); }
    void AudioEngine::setHeadphoneVolume (float p) noexcept  { store01 (phonesVolume, p); }
    void AudioEngine::setCueMix (float p) noexcept           { store01 (cueMix, p); }

    MeterSource::Reading AudioEngine::readMeter (MeterId id) noexcept
    {
        switch (id)
        {
            case MeterId::channel1:    return strips[0].getMeter().read();
            case MeterId::channel2:    return strips[1].getMeter().read();
            case MeterId::masterLeft:  return masterMeters[0].read();
            case MeterId::masterRight: return masterMeters[1].read();
            case MeterId::count:       break;
        }
        return {};
    }

    void AudioEngine::collectGarbage()
    {
        TrackBuffer* t = nullptr;
        while (released.pop (t))
        {
            // El audio ya no la usa: si nadie más la referencia, se libera acá.
            auto it = std::find_if (liveTracks.begin(), liveTracks.end(),
                                    [t] (const auto& p) { return p.get() == t; });
            jassert (it != liveTracks.end());
            if (it != liveTracks.end())
            {
                (*it)->cancelLoading();   // si todavía se estaba decodificando
                liveTracks.erase (it);
            }
        }
    }

    double AudioEngine::getDeckBpm (int d) const noexcept
    {
        if (! juce::isPositiveAndBelow (d, kNumDecks))
            return 0.0;
        const auto* t = decks[(size_t) d].getTrack();
        return t != nullptr && t->getBpm() > 0.0 ? t->getBpm() * decks[(size_t) d].getRate() : 0.0;
    }

    Deck::Status AudioEngine::getDeckStatus (int deck) const noexcept
    {
        return juce::isPositiveAndBelow (deck, kNumDecks) ? decks[(size_t) deck].getStatus() : Deck::Status {};
    }

    // =========================================================================
    //  Hilo de audio
    // =========================================================================
    void AudioEngine::handleCommand (const Command& c) noexcept
    {
        auto& d = decks[(size_t) c.deck];
        switch (c.type)
        {
            case Command::Type::load:       d.load (c.track);   break;
            case Command::Type::play:
                d.play();
                if (channel[(size_t) c.deck].sync.load (std::memory_order_relaxed))
                    engageSync (c.deck);   // con SYNC, arranca ya en fase con la otra bandeja
                break;
            case Command::Type::pause:      d.pause();          break;
            case Command::Type::togglePlay:
                d.togglePlay();
                if (d.isPlaying() && channel[(size_t) c.deck].sync.load (std::memory_order_relaxed))
                    engageSync (c.deck);
                break;
            case Command::Type::cueDown:    d.cueDown();        break;
            case Command::Type::cueUp:      d.cueUp();          break;
            case Command::Type::seek:       d.seek (c.value);   break;
            case Command::Type::syncOn:
                channel[(size_t) c.deck].sync.store (true, std::memory_order_relaxed);
                channel[(size_t) (1 - c.deck)].sync.store (false, std::memory_order_relaxed);
                engageSync (c.deck);
                break;
            case Command::Type::syncOff:
                channel[(size_t) c.deck].sync.store (false, std::memory_order_relaxed);
                break;
            case Command::Type::autoMixStart: beginAutoMix (c.value); break;
            case Command::Type::autoMixStop:  endAutoMix (false);     break;
            case Command::Type::hotCue:       d.hotCue (c.index);                 break;
            case Command::Type::setHotCue:    d.setHotCue (c.index, c.value);     break;
            case Command::Type::autoLoop:     d.autoLoop (c.value);               break;
            case Command::Type::loopRoll:     d.loopRoll (c.value, c.index != 0); break;
            case Command::Type::loopIn:       d.loopInPoint();                    break;
            case Command::Type::loopOut:      d.loopOutPoint();                   break;
            case Command::Type::loopHalve:    d.loopHalve();                      break;
            case Command::Type::loopDouble:   d.loopDouble();                     break;
            case Command::Type::loopExit:     d.loopExit();                       break;
            case Command::Type::slip:         d.setSlip (c.index != 0);           break;
            case Command::Type::reverse:      d.setReverse (c.index != 0);        break;
            case Command::Type::effect:       d.setEffect ((Deck::Effect) c.index, c.value > 0.5); break;
            case Command::Type::scratchBegin: d.scratchBegin();                   break;
            case Command::Type::scratchMove:  d.scratchMove (c.value);            break;
            case Command::Type::scratchEnd:   d.scratchEnd();                     break;
            case Command::Type::nudge:        d.nudge (c.value);                  break;
            case Command::Type::sampleLoad:    sampler.load (c.index, c.track);   break;
            case Command::Type::sampleTrigger: sampler.trigger (c.index);         break;
            case Command::Type::sampleStop:    sampler.stop (c.index);            break;
        }
    }

    void AudioEngine::returnRetiredTracks() noexcept
    {
        for (auto& d : decks)
        {
            while (auto* t = d.peekRetired())
            {
                if (! released.push (t))
                    return;   // cola llena: se reintenta en el próximo bloque
                d.dropRetired();
            }
        }
        while (auto* t = sampler.peekRetired())
        {
            if (! released.push (t))
                return;
            sampler.dropRetired();
        }
    }

    void AudioEngine::process (float* const* outputs, int numOutputChannels, int numSamples) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        if (outputs == nullptr || numSamples <= 0)
            return;

        if (! prepared.load (std::memory_order_acquire) || work.getNumSamples() == 0)
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (auto* out = outputs[ch])
                    juce::FloatVectorOperations::clear (out, numSamples);
            return;
        }

        Command c;
        while (commands.pop (c))
            handleCommand (c);

        const auto block = work.getNumSamples();
        for (int offset = 0; offset < numSamples; offset += block)
            renderBlock (outputs, numOutputChannels, offset, std::min (block, numSamples - offset));

        returnRetiredTracks();
        processedBlocks.fetch_add (1, std::memory_order_relaxed);
    }

    void AudioEngine::renderBlock (float* const* outputs, int numOutputChannels, int offset, int n) noexcept
    {
        auto* W = work.getArrayOfWritePointers();

        advanceAutoMix (n);
        updateTempo (n);

        // 1) Decks y tiras de canal (trim, EQ, filtro, vúmetro de canal).
        for (int d = 0; d < kNumDecks; ++d)
        {
            auto* L = W[deck1L + d * 2];
            auto* R = W[deck1R + d * 2];
            decks[(size_t) d].render (L, R, n);

            const auto& c = channel[(size_t) d];
            ChannelStrip::Params p;
            p.trim   = load (c.trim);
            p.low    = load (c.low);
            p.mid    = load (c.mid);
            p.high   = load (c.high);
            p.filter = load (c.filter);
            p.lowMultiplier = amLowMultiplier[(size_t) d];
            strips[(size_t) d].process (L, R, n, p);

            // FX asignados a este deck: después del EQ y el filtro (también llegan al CUE).
            for (auto& u : fxUnits)
                if (u.getTarget() == (fx::Target) d)
                    u.process (L, R, n, getDeckBpm (d));
        }

        // 1b) Sampler (va directo al bus master, antes del volumen MASTER).
        juce::FloatVectorOperations::clear (W[samplerL], n);
        juce::FloatVectorOperations::clear (W[samplerR], n);
        sampler.render (W[samplerL], W[samplerR], n);

        // 2) Ganancias objetivo.
        float xA = 1.0f, xB = 1.0f;
        // Durante el AUTO MIX siempre curva suave (con "corte" el pase sería brusco).
        crossfaderGains (load (crossfader),
                         amActive ? CrossfaderCurve::smooth : (CrossfaderCurve) xfCurve.load (std::memory_order_relaxed),
                         xfReverse.load (std::memory_order_relaxed), xA, xB);
        const float xfade[kNumDecks] = { xA, xB };

        for (int d = 0; d < kNumDecks; ++d)
        {
            const auto& c = channel[(size_t) d];
            smoothedDeckGain[(size_t) d].setTargetValue (faderGain (load (c.volume)) * xfade[d]);
            smoothedCueGain[(size_t) d].setTargetValue (c.cue.load (std::memory_order_relaxed) ? 1.0f : 0.0f);
        }
        smoothedMaster.setTargetValue (faderGain (load (masterVolume)));
        smoothedPhones.setTargetValue (faderGain (load (phonesVolume)));
        const auto mix = load (cueMix);
        smoothedMixCue.setTargetValue (std::cos (mix * juce::MathConstants<float>::halfPi));
        smoothedMixMaster.setTargetValue (std::sin (mix * juce::MathConstants<float>::halfPi));

        // 3) Buses: master, cue y auriculares.
        const auto* aL = W[deck1L]; const auto* aR = W[deck1R];
        const auto* bL = W[deck2L]; const auto* bR = W[deck2R];
        auto* mL = W[masterL]; auto* mR = W[masterR];
        auto* pL = W[phonesL]; auto* pR = W[phonesR];
        const auto* sL = W[samplerL]; const auto* sR = W[samplerR];

        // 3a) Bus master (antes del volumen) y bus de cue.
        for (int i = 0; i < n; ++i)
        {
            const auto gA = smoothedDeckGain[0].getNextValue();
            const auto gB = smoothedDeckGain[1].getNextValue();
            const auto cA = smoothedCueGain[0].getNextValue();
            const auto cB = smoothedCueGain[1].getNextValue();
            mL[i] = aL[i] * gA + bL[i] * gB + sL[i];
            mR[i] = aR[i] * gA + bR[i] * gB + sR[i];
            pL[i] = aL[i] * cA + bL[i] * cB;   // por ahora: bus de CUE (pre-fader)
            pR[i] = aR[i] * cA + bR[i] * cB;
        }

        // 3b) FX asignados al MASTER (antes del volumen y del limitador). Su tempo:
        //     el del deck que más suena "al aire".
        {
            double masterBpm = 0.0;
            float loudest = -1.0f;
            for (int d = 0; d < kNumDecks; ++d)
            {
                const auto level = decks[(size_t) d].isPlaying() ? faderGain (load (channel[(size_t) d].volume)) * xfade[d] : -0.5f;
                if (level > loudest && getDeckBpm (d) > 0.0)
                {
                    loudest = level;
                    masterBpm = getDeckBpm (d);
                }
            }
            for (auto& u : fxUnits)
                if (u.getTarget() == fx::Target::master)
                    u.process (mL, mR, n, masterBpm);
        }

        // 3c) Volumen MASTER y auriculares (el master entra a los auriculares antes del
        //     volumen MASTER: bajar los parlantes no apaga el master en los auriculares).
        float masterPeakBeforeLimiter = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto gM = smoothedMaster.getNextValue();
            const auto gP = smoothedPhones.getNextValue();
            const auto mc = smoothedMixCue.getNextValue();
            const auto mm = smoothedMixMaster.getNextValue();
            const auto busL = mL[i], busR = mR[i];
            pL[i] = (pL[i] * mc + busL * mm) * gP;
            pR[i] = (pR[i] * mc + busR * mm) * gP;
            mL[i] = busL * gM;
            mR[i] = busR * gM;
            masterPeakBeforeLimiter = std::max (masterPeakBeforeLimiter, std::max (std::abs (mL[i]), std::abs (mR[i])));
        }

        // 4) Limitadores de seguridad y vúmetros del master (post-limitador).
        const auto minGain = masterLimiter.process (mL, mR, n);
        phonesLimiter.process (pL, pR, n);
        limiterReductionDb.store (minGain < 1.0f ? -juce::Decibels::gainToDecibels (minGain) : 0.0f, std::memory_order_relaxed);

        masterMeters[0].addBlock (mL, nullptr, n, 2.0f);
        masterMeters[1].addBlock (mR, nullptr, n, 2.0f);
        if (masterPeakBeforeLimiter >= 1.0f)
        {
            // Sin el limitador habría saturado: se muestra como CLIP.
            masterMeters[0].markClip();
            masterMeters[1].markClip();
            clippedSamples.fetch_add (1, std::memory_order_relaxed);
        }

        // 5) Ruteo a las salidas del dispositivo.
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (auto* out = outputs[ch])
                juce::FloatVectorOperations::clear (out + offset, n);

        auto out = [&] (int ch) -> float* { return ch < numOutputChannels ? outputs[ch] : nullptr; };

        if (numOutputChannels == 1)
        {
            if (auto* o = out (0))
                for (int i = 0; i < n; ++i)
                    o[offset + i] = 0.5f * (mL[i] + mR[i]);
            return;
        }

        auto mPair = masterPair.load (std::memory_order_relaxed);
        if (mPair == 1 && numOutputChannels < 4)
            mPair = 0;   // la placa no tiene salidas 3-4: el master va a 1-2
        const auto pPair = 1 - mPair;

        if (split.load (std::memory_order_relaxed))
        {
            // SPLIT: izquierdo = master mono, derecho = auriculares mono.
            auto* oL = out (mPair * 2);
            auto* oR = out (mPair * 2 + 1);
            for (int i = 0; i < n; ++i)
            {
                if (oL != nullptr) oL[offset + i] = 0.5f * (mL[i] + mR[i]);
                if (oR != nullptr) oR[offset + i] = 0.5f * (pL[i] + pR[i]);
            }
            return;
        }

        if (auto* o = out (mPair * 2))     juce::FloatVectorOperations::copy (o + offset, mL, n);
        if (auto* o = out (mPair * 2 + 1)) juce::FloatVectorOperations::copy (o + offset, mR, n);
        if (auto* o = out (pPair * 2))     juce::FloatVectorOperations::copy (o + offset, pL, n);
        if (auto* o = out (pPair * 2 + 1)) juce::FloatVectorOperations::copy (o + offset, pR, n);
    }
} // namespace kb::engine
