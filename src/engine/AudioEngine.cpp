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
            d.prepare (config.sampleRate);
        for (auto& s : strips)
            s.prepare (config.sampleRate);
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

    void AudioEngine::setCrossfader (float p) noexcept       { crossfader.store (juce::jlimit (-1.0f, 1.0f, p), std::memory_order_relaxed); }
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
            case Command::Type::play:       d.play();           break;
            case Command::Type::pause:      d.pause();          break;
            case Command::Type::togglePlay: d.togglePlay();     break;
            case Command::Type::cueDown:    d.cueDown();        break;
            case Command::Type::cueUp:      d.cueUp();          break;
            case Command::Type::seek:       d.seek (c.value);   break;
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
            strips[(size_t) d].process (L, R, n, p);
        }

        // 2) Ganancias objetivo.
        float xA = 1.0f, xB = 1.0f;
        crossfaderGains (load (crossfader), (CrossfaderCurve) xfCurve.load (std::memory_order_relaxed),
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

        float masterPeakBeforeLimiter = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto gA = smoothedDeckGain[0].getNextValue();
            const auto gB = smoothedDeckGain[1].getNextValue();
            const auto cA = smoothedCueGain[0].getNextValue();
            const auto cB = smoothedCueGain[1].getNextValue();
            const auto gM = smoothedMaster.getNextValue();
            const auto gP = smoothedPhones.getNextValue();
            const auto mc = smoothedMixCue.getNextValue();
            const auto mm = smoothedMixMaster.getNextValue();

            const auto busL = aL[i] * gA + bL[i] * gB;   // master antes del volumen MASTER
            const auto busR = aR[i] * gA + bR[i] * gB;
            const auto cueBusL = aL[i] * cA + bL[i] * cB;
            const auto cueBusR = aR[i] * cA + bR[i] * cB;

            mL[i] = busL * gM;
            mR[i] = busR * gM;
            // Auriculares: el master entra antes del volumen MASTER (bajar los
            // parlantes no apaga el master en los auriculares).
            pL[i] = (cueBusL * mc + busL * mm) * gP;
            pR[i] = (cueBusR * mc + busR * mm) * gP;

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
