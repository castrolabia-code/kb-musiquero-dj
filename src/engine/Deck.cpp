#include "engine/Deck.h"

#include <algorithm>
#include <cmath>

namespace kb::engine
{
    void Deck::prepare (double sr) noexcept
    {
        engineSampleRate = sr > 0.0 ? sr : 48000.0;
        fadeLength = std::max (16, (int) std::lround (kFadeSeconds * engineSampleRate));
        fadeStep   = 1.0f / (float) fadeLength;

        // Si la pista se cargó a otro sample rate, se reproduce interpolando.
        main.ratio  = ratioFor (main.track);
        ghost.ratio = ratioFor (ghost.track);
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

        for (int n = 0; n < numSamples; ++n)
        {
            float outL = 0.0f, outR = 0.0f;

            if (playing && main.track != nullptr)
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
                    main.pos += main.ratio;
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
                ghost.pos += ghost.ratio;
                ghostGain = std::max (0.0f, ghostGain - ghostStep);
                if (--ghostRemaining == 0)
                    endGhost();
            }

            left[n]  = outL;
            right[n] = outR;
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
