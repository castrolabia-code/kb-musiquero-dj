#include "engine/Deck.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace kb::engine
{
    // =========================================================================
    //  Time-stretch: Signalsmith Stretch + buffers pre-reservados
    // =========================================================================
    struct Deck::Stretcher
    {
        signalsmith::stretch::SignalsmithStretch<float> engine { 0x4B42 };   // semilla fija: reproducible
        std::vector<float> inL, inR, outL, outR, seekL, seekR;
        float* in[2]   {};
        float* out[2]  {};
        float* seek[2] {};
        int maxIn = 0, maxOut = 0, maxSeek = 0;

        void allocate (double sampleRate, int maxBlock)
        {
            engine.presetDefault (2, (float) sampleRate);
            maxOut  = std::max (maxBlock, 64);
            maxIn   = maxOut * 2 + 64;   // hasta +50 % de tempo (y algo de margen)
            maxSeek = engine.inputLatency() + 2 * engine.outputLatency() + 64;
            for (auto* v : { &inL, &inR })     v->assign ((size_t) maxIn, 0.0f);
            for (auto* v : { &outL, &outR })   v->assign ((size_t) maxOut, 0.0f);
            for (auto* v : { &seekL, &seekR }) v->assign ((size_t) maxSeek, 0.0f);
            in[0] = inL.data();     in[1] = inR.data();
            out[0] = outL.data();   out[1] = outR.data();
            seek[0] = seekL.data(); seek[1] = seekR.data();

            // "Calentar": la primera vez el stretch dimensiona sus buffers internos.
            engine.outputSeek (seek, engine.outputSeekLength (1.0f));
            engine.process (in, maxOut, out, maxOut);
            engine.outputSeek (seek, engine.outputSeekLength (1.5f));
            engine.reset();
        }
    };

    Deck::Deck() = default;
    Deck::~Deck() = default;

    void Deck::prepare (double sr, int maxBlockSize)
    {
        engineSampleRate = sr > 0.0 ? sr : 48000.0;
        fadeLength = std::max (16, (int) std::lround (kFadeSeconds * engineSampleRate));
        fadeStep   = 1.0f / (float) fadeLength;

        // Si la pista se cargó a otro sample rate, se reproduce interpolando.
        main.ratio  = ratioFor (main.track);
        ghost.ratio = ratioFor (ghost.track);

        stretcher = std::make_unique<Stretcher>();
        stretcher->allocate (engineSampleRate, maxBlockSize);
        appliedTranspose = 1000.0f;
        needPrime = true;
    }

    void Deck::setPitchMode (bool lock, float shift) noexcept
    {
        keyLock  = lock;
        keyShift = shift;
        const auto active = stretcher != nullptr && (lock || std::abs (shift) > 0.01f);
        if (active != stretchActive)
        {
            // Cambio de modo sonando: fundido entre el modo viejo y el nuevo.
            startGhost();
            mainGain = 0.0f;
            stretchActive = active;
            needPrime = true;
        }
    }

    double Deck::stretchLead() const noexcept
    {
        if (stretcher == nullptr)
            return 0.0;
        const auto& s = stretcher->engine;
        return ((double) s.inputLatency() + rate * (double) s.outputLatency()) * main.ratio;
    }

    void Deck::prime() noexcept
    {
        auto& st = *stretcher;
        const auto length = std::min (st.maxSeek, st.engine.outputSeekLength ((float) rate));
        const auto decoded = main.track->getDecodedFrames();
        for (int i = 0; i < length; ++i)
            readFrame (*main.track, decoded, main.pos + i * main.ratio, st.seekL[(size_t) i], st.seekR[(size_t) i]);
        st.engine.outputSeek (st.seek, length);
        stretchReadPos = main.pos + length * main.ratio;
        inputFraction = 0.0;
        needPrime = false;
    }

    bool Deck::renderStretched (int n, std::int64_t decoded) noexcept
    {
        auto& st = *stretcher;

        // Tono: con Key Lock solo el corrimiento pedido; sin Key Lock (solo key
        // shift) también el cambio de tono natural de la velocidad, como un vinilo.
        const auto transpose = keyShift + (keyLock ? 0.0f : 12.0f * (float) std::log2 (rate));
        if (std::abs (transpose - appliedTranspose) > 0.001f)
        {
            st.engine.setTransposeSemitones (transpose);
            appliedTranspose = transpose;
        }

        if (needPrime)
            prime();

        inputFraction += n * rate;
        const auto inCount = std::min (st.maxIn, (int) inputFraction);
        inputFraction -= inCount;

        // Si la entrada todavía no se decodificó, esperar (como en el modo normal).
        if (stretchReadPos + inCount * main.ratio + 2.0 >= (double) decoded && ! main.track->isComplete())
            return false;

        for (int i = 0; i < inCount; ++i)
            readFrame (*main.track, decoded, stretchReadPos + i * main.ratio, st.inL[(size_t) i], st.inR[(size_t) i]);
        stretchReadPos += inCount * main.ratio;

        st.engine.process (st.in, inCount, st.out, std::min (n, st.maxOut));
        main.pos = stretchReadPos - stretchLead();
        return true;
    }

    double Deck::ratioFor (const TrackBuffer* t) const noexcept
    {
        return t != nullptr ? t->getSampleRate() / engineSampleRate : 1.0;
    }

    // =========================================================================
    //  Retiro de pistas (nunca se libera memoria en el hilo de audio)
    // =========================================================================
    void Deck::retire (TrackBuffer* t) noexcept
    {
        if (t == nullptr)
            return;

        jassert (numRetired < kMaxRetired);
        if (numRetired < kMaxRetired)
            retired[(size_t) numRetired++] = t;
        // Si estuviera lleno (imposible en uso normal: el motor vacía la lista en
        // cada bloque) preferimos perder la referencia a liberar en tiempo real.
    }

    void Deck::dropRetired() noexcept
    {
        if (numRetired == 0)
            return;
        for (int i = 1; i < numRetired; ++i)
            retired[(size_t) (i - 1)] = retired[(size_t) i];
        retired[(size_t) --numRetired] = nullptr;
    }

    // =========================================================================
    //  Fundidos
    // =========================================================================
    void Deck::startGhost() noexcept
    {
        const bool audible = playing && main.track != nullptr && mainGain > 0.0f;
        if (! audible)
            return;

        // Si había otro fundido en curso con una pista vieja, esa pista se retira.
        if (ghostRemaining > 0)
            endGhost();

        ghost          = main;
        ghostRate      = rate;
        ghostGain      = mainGain;
        ghostStep      = mainGain / (float) fadeLength;
        ghostRemaining = fadeLength;
    }

    void Deck::endGhost() noexcept
    {
        if (ghost.track != nullptr && ghost.track != main.track)
            retire (ghost.track);

        ghost          = {};
        ghostGain      = 0.0f;
        ghostRemaining = 0;
    }

    // =========================================================================
    //  Comandos
    // =========================================================================
    void Deck::load (TrackBuffer* newTrack) noexcept
    {
        if (newTrack == main.track)
            return;

        startGhost();   // si estaba sonando, la pista vieja se apaga suave

        auto* old = main.track;
        main.track = newTrack;
        main.pos   = 0.0;
        main.ratio = ratioFor (newTrack);

        // La pista vieja se retira ya, salvo que el fundido todavía la esté leyendo
        // (en ese caso endGhost() la retira al terminar).
        if (old != nullptr && old != ghost.track)
            retire (old);

        playing    = false;
        cuePreview = false;
        mainGain   = 0.0f;
        cuePoint   = 0.0;
        needPrime  = true;
        publish();
    }

    void Deck::play() noexcept
    {
        if (cuePreview)
        {
            // PLAY durante la preescucha: al soltar CUE sigue sonando.
            cuePreview = false;
            return;
        }

        if (playing || main.track == nullptr)
            return;

        if (main.track->isComplete() && main.pos >= (double) main.track->getLengthFrames())
            return;   // al final de la pista: no hay nada que reproducir

        playing  = true;
        mainGain = 0.0f;   // entra con fundido
        needPrime = true;
    }

    void Deck::pause() noexcept
    {
        if (! playing)
            return;

        startGhost();
        playing    = false;
        cuePreview = false;
        mainGain   = 0.0f;
    }

    void Deck::togglePlay() noexcept
    {
        if (playing && ! cuePreview)
            pause();
        else
            play();
    }

    void Deck::cueDown() noexcept
    {
        if (main.track == nullptr)
            return;

        if (playing && ! cuePreview)
        {
            // Reproduciendo: vuelve al cue y pausa.
            startGhost();
            playing  = false;
            mainGain = 0.0f;
            main.pos = cuePoint;
            return;
        }

        if (! playing)
        {
            if (std::abs (main.pos - cuePoint) < 0.5)
            {
                // Sobre el cue: preescucha mientras se mantiene apretado.
                playing    = true;
                cuePreview = true;
                mainGain   = 0.0f;
                needPrime  = true;
            }
            else
            {
                // En pausa en otro lugar: fija el cue acá.
                cuePoint = main.pos;
            }
        }
    }

    void Deck::cueUp() noexcept
    {
        if (! cuePreview)
            return;

        startGhost();
        playing    = false;
        cuePreview = false;
        mainGain   = 0.0f;
        main.pos   = cuePoint;
    }

    void Deck::seek (double seconds) noexcept
    {
        if (main.track == nullptr)
            return;

        const auto length = (double) main.track->getLengthFrames();
        const auto target = std::floor (juce::jlimit (0.0, length, seconds * main.track->getSampleRate()));

        if (playing)
        {
            startGhost();
            mainGain = 0.0f;
        }
        main.pos = target;
        needPrime = true;
    }

    // =========================================================================
    //  Render
    // =========================================================================
    void Deck::readFrame (const TrackBuffer& t, std::int64_t decoded, double pos, float& l, float& r) noexcept
    {
        const auto i    = (std::int64_t) std::floor (pos);
        const auto frac = (float) (pos - (double) i);
        const auto* L   = t.getReadPointer (0);
        const auto* R   = t.getReadPointer (1);

        if (juce::exactlyEqual (frac, 0.0f))
        {
            // Caso normal (mismo sample rate): copia exacta.
            if (i >= 0 && i < decoded) { l = L[i]; r = R[i]; }
            else                       { l = r = 0.0f; }
            return;
        }

        // Interpolación Hermite de 4 puntos (sample rate distinto al del motor).
        auto at = [decoded] (const float* d, std::int64_t k) noexcept { return (k >= 0 && k < decoded) ? d[k] : 0.0f; };
        auto hermite = [frac] (float ym1, float y0, float y1, float y2) noexcept
        {
            const auto c1 = 0.5f * (y1 - ym1);
            const auto c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const auto c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * frac + c2) * frac + c1) * frac + y0;
        };

        l = hermite (at (L, i - 1), at (L, i), at (L, i + 1), at (L, i + 2));
        r = hermite (at (R, i - 1), at (R, i), at (R, i + 1), at (R, i + 2));
    }

    void Deck::render (float* left, float* right, int numSamples) noexcept
    {
        // Snapshots por bloque (los atómicos se leen una sola vez).
        std::int64_t mainDecoded = 0, mainLength = 0, ghostDecoded = 0;
        bool mainComplete = false;
        if (main.track != nullptr)
        {
            mainDecoded  = main.track->getDecodedFrames();
            mainLength   = main.track->getLengthFrames();
            mainComplete = main.track->isComplete();
        }
        if (ghost.track != nullptr)
            ghostDecoded = ghost.track->getDecodedFrames();

        // Key Lock / key shift: el bloque principal sale del time-stretch.
        bool stretchedBlock = false;
        if (stretchActive && playing && main.track != nullptr && numSamples <= stretcher->maxOut
            && ! (mainComplete && main.pos >= (double) mainLength))
        {
            stretchedBlock = renderStretched (numSamples, mainDecoded);
            if (! stretchedBlock)
            {
                mainGain = 0.0f;     // esperando decodificación: silencio y re-cebado
                needPrime = true;
            }
        }

        for (int n = 0; n < numSamples; ++n)
        {
            float outL = 0.0f, outR = 0.0f;

            if (stretchedBlock)
            {
                outL += stretcher->outL[(size_t) n] * mainGain;
                outR += stretcher->outR[(size_t) n] * mainGain;
                if (mainGain < 1.0f)
                    mainGain = std::min (1.0f, mainGain + fadeStep);
            }
            else if (playing && main.track != nullptr && ! stretchActive)
            {
                if (mainComplete && main.pos >= (double) mainLength)
                {
                    // Fin de la pista.
                    playing    = false;
                    cuePreview = false;
                    mainGain   = 0.0f;
                    main.pos   = (double) mainLength;
                }
                else if (main.pos + 2.0 >= (double) mainDecoded && ! mainComplete)
                {
                    // Todavía no se decodificó esta parte: esperar sin avanzar y
                    // volver a entrar con fundido cuando llegue el audio.
                    mainGain = 0.0f;
                }
                else
                {
                    float l, r;
                    readFrame (*main.track, mainDecoded, main.pos, l, r);
                    outL += l * mainGain;
                    outR += r * mainGain;
                    main.pos += main.ratio * rate;
                    if (mainGain < 1.0f)
                        mainGain = std::min (1.0f, mainGain + fadeStep);
                }
            }

            if (ghostRemaining > 0)
            {
                float l, r;
                readFrame (*ghost.track, ghostDecoded, ghost.pos, l, r);
                outL += l * ghostGain;
                outR += r * ghostGain;
                ghost.pos += ghost.ratio * ghostRate;
                ghostGain = std::max (0.0f, ghostGain - ghostStep);
                if (--ghostRemaining == 0)
                    endGhost();
            }

            left[n]  = outL;
            right[n] = outR;
        }

        // Fin de la pista también en modo Key Lock.
        if (stretchActive && playing && main.track != nullptr && mainComplete && main.pos >= (double) mainLength)
        {
            playing    = false;
            cuePreview = false;
            mainGain   = 0.0f;
            main.pos   = (double) mainLength;
        }

        publish();
    }

    // =========================================================================
    //  Estado
    // =========================================================================
    void Deck::publish() noexcept
    {
        const auto sr = main.track != nullptr ? main.track->getSampleRate() : engineSampleRate;
        pubTrackId.store (main.track != nullptr ? main.track->getId() : 0, std::memory_order_relaxed);
        pubPlaying.store (playing, std::memory_order_relaxed);
        pubCuePreview.store (cuePreview, std::memory_order_relaxed);
        pubPosition.store (main.pos / sr, std::memory_order_relaxed);
        pubCue.store (cuePoint / sr, std::memory_order_relaxed);
    }

    Deck::Status Deck::getStatus() const noexcept
    {
        Status s;
        s.trackId         = pubTrackId.load (std::memory_order_relaxed);
        s.playing         = pubPlaying.load (std::memory_order_relaxed);
        s.cuePreview      = pubCuePreview.load (std::memory_order_relaxed);
        s.positionSeconds = pubPosition.load (std::memory_order_relaxed);
        s.cueSeconds      = pubCue.load (std::memory_order_relaxed);
        return s;
    }
} // namespace kb::engine
