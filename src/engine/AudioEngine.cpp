#include "engine/AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace kb::engine
{
    static constexpr double kParamSmoothingSeconds = 0.02;   // 20 ms: sin "zipper noise"

    AudioEngine::AudioEngine()
    {
        for (auto& v : channelVolume)
            v.store (1.0f);
    }

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
        // Potencia constante (curva "suave"): al centro ambos a -3 dB.
        // Las curvas cut/scratch e inversión llegan en la Fase 2.
        const auto x = (juce::jlimit (-1.0f, 1.0f, position) + 1.0f) * 0.5f;
        gainA = std::cos (x * juce::MathConstants<float>::halfPi);
        gainB = std::sin (x * juce::MathConstants<float>::halfPi);
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
        deckBuffers.setSize (kNumDecks * 2, block, false, true, false);
        deckBuffers.clear();

        for (auto& d : decks)
            d.prepare (config.sampleRate);

        for (int i = 0; i < kNumDecks; ++i)
        {
            smoothedDeckGain[(size_t) i].reset (config.sampleRate, kParamSmoothingSeconds);
            smoothedDeckGain[(size_t) i].setCurrentAndTargetValue (0.0f);
        }
        smoothedMaster.reset (config.sampleRate, kParamSmoothingSeconds);
        smoothedMaster.setCurrentAndTargetValue (faderGain (masterVolume.load()));

        sampleRate.store (config.sampleRate, std::memory_order_relaxed);
        maxBlockSize.store (block, std::memory_order_relaxed);
        numOutputs.store (channels, std::memory_order_relaxed);
        processedBlocks.store (0, std::memory_order_relaxed);
        prepared.store (true, std::memory_order_release);
    }

    void AudioEngine::release()
    {
        prepared.store (false, std::memory_order_release);
        deckBuffers.setSize (0, 0);
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

    void AudioEngine::setChannelVolume (int deck, float p) noexcept
    {
        if (juce::isPositiveAndBelow (deck, kNumDecks))
            channelVolume[(size_t) deck].store (juce::jlimit (0.0f, 1.0f, p), std::memory_order_relaxed);
    }

    float AudioEngine::getChannelVolume (int deck) const noexcept
    {
        return juce::isPositiveAndBelow (deck, kNumDecks) ? channelVolume[(size_t) deck].load (std::memory_order_relaxed) : 0.0f;
    }

    void AudioEngine::setCrossfader (float p) noexcept      { crossfader.store (juce::jlimit (-1.0f, 1.0f, p), std::memory_order_relaxed); }
    void AudioEngine::setMasterVolume (float p) noexcept    { masterVolume.store (juce::jlimit (0.0f, 1.0f, p), std::memory_order_relaxed); }

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

        if (! prepared.load (std::memory_order_acquire) || deckBuffers.getNumSamples() == 0)
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (auto* out = outputs[ch])
                    juce::FloatVectorOperations::clear (out, numSamples);
            return;
        }

        Command c;
        while (commands.pop (c))
            handleCommand (c);

        const auto block = deckBuffers.getNumSamples();
        for (int offset = 0; offset < numSamples; offset += block)
            renderBlock (outputs, numOutputChannels, offset, std::min (block, numSamples - offset));

        returnRetiredTracks();
        processedBlocks.fetch_add (1, std::memory_order_relaxed);
    }

    void AudioEngine::renderBlock (float* const* outputs, int numOutputChannels, int offset, int n) noexcept
    {
        // 1) Cada deck renderiza su pista.
        for (int d = 0; d < kNumDecks; ++d)
            decks[(size_t) d].render (deckBuffers.getWritePointer (d * 2), deckBuffers.getWritePointer (d * 2 + 1), n);

        // 2) Ganancias objetivo: fader de canal x crossfader, y master.
        float xA = 1.0f, xB = 1.0f;
        crossfaderGains (crossfader.load (std::memory_order_relaxed), xA, xB);
        const float xfade[kNumDecks] = { xA, xB };

        for (int d = 0; d < kNumDecks; ++d)
            smoothedDeckGain[(size_t) d].setTargetValue (faderGain (channelVolume[(size_t) d].load (std::memory_order_relaxed)) * xfade[d]);
        smoothedMaster.setTargetValue (faderGain (masterVolume.load (std::memory_order_relaxed)));

        // 3) Mezcla a master con suavizado por muestra.
        const auto* aL = deckBuffers.getReadPointer (0);
        const auto* aR = deckBuffers.getReadPointer (1);
        const auto* bL = deckBuffers.getReadPointer (2);
        const auto* bR = deckBuffers.getReadPointer (3);

        float* outL = numOutputChannels > 0 ? outputs[0] : nullptr;
        float* outR = numOutputChannels > 1 ? outputs[1] : nullptr;
        std::uint64_t clipped = 0;

        for (int i = 0; i < n; ++i)
        {
            const auto gA = smoothedDeckGain[0].getNextValue();
            const auto gB = smoothedDeckGain[1].getNextValue();
            const auto gM = smoothedMaster.getNextValue();

            auto l = (aL[i] * gA + bL[i] * gB) * gM;
            auto r = (aR[i] * gA + bR[i] * gB) * gM;

            // Protección mínima hasta el limitador de la Fase 2: nunca más de 0 dBFS.
            if (std::abs (l) > 1.0f) { l = juce::jlimit (-1.0f, 1.0f, l); ++clipped; }
            if (std::abs (r) > 1.0f) { r = juce::jlimit (-1.0f, 1.0f, r); ++clipped; }

            if (outR == nullptr)
                l = 0.5f * (l + r);   // una sola salida: mono

            if (outL != nullptr) outL[offset + i] = l;
            if (outR != nullptr) outR[offset + i] = r;
        }

        // Salidas 3-4 (cue de auriculares): Fase 2. Por ahora silencio.
        for (int ch = 2; ch < numOutputChannels; ++ch)
            if (auto* out = outputs[ch])
                juce::FloatVectorOperations::clear (out + offset, n);

        if (clipped > 0)
            clippedSamples.fetch_add (clipped, std::memory_order_relaxed);
    }
} // namespace kb::engine
