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

    Deck::Deck()
    {
        hotCues.fill (-1.0);
        for (auto& h : pubHotCues)
            h.store (-1.0, std::memory_order_relaxed);
    }
    Deck::~Deck() = default;

    void Deck::prepare (double sr, int maxBlockSize)
    {
        engineSampleRate = sr > 0.0 ? sr : 48000.0;
        fadeLength = std::max (16, (int) std::lround (kFadeSeconds * engineSampleRate));
        fadeStep   = 1.0f / (float) fadeLength;

        // Si la pista se cargó a otro sample rate, se reproduce interpolando.
        main.ratio  = ratioFor (main.track);
        ghost.ratio = ratioFor (ghost.track);

        // Scratch: la posición sigue a la mano con un "resorte" (~12 ms) suavizado (~4 ms).
        scratchK      = 1.0 / (0.012 * engineSampleRate);
        scratchSmooth = std::exp (-1.0 / (0.004 * engineSampleRate));
        brakeStep     = 1.0 / (kBrakeSeconds * engineSampleRate);
        spinDecay     = std::exp (-1.0 / (0.45 * engineSampleRate));

        stretcher = std::make_unique<Stretcher>();
        stretcher->allocate (engineSampleRate, maxBlockSize);
        appliedTranspose = 1000.0f;
        needPrime = true;
    }

    void Deck::setPitchMode (bool lock, float shift) noexcept
    {
        keyLock  = lock;
        keyShift = shift;
        // El cambio efectivo de modo (con fundido) se hace en render().
        stretchActive = stretcher != nullptr && (lock || std::abs (shift) > 0.01f);
    }

    double Deck::stretchLead() const noexcept
    {
        if (stretcher == nullptr)
            return 0.0;
        const auto& s = stretcher->engine;
        return ((double) s.inputLatency() + effRate * (double) s.outputLatency()) * main.ratio;
    }

    void Deck::prime() noexcept
    {
        auto& st = *stretcher;
        const auto length = std::min (st.maxSeek, st.engine.outputSeekLength ((float) effRate));
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
        const auto transpose = keyShift + (keyLock ? 0.0f : 12.0f * (float) std::log2 (effRate));
        if (std::abs (transpose - appliedTranspose) > 0.001f)
        {
            st.engine.setTransposeSemitones (transpose);
            appliedTranspose = transpose;
        }

        if (needPrime)
            prime();

        inputFraction += n * effRate;
        const auto inCount = std::min (st.maxIn, (int) inputFraction);
        inputFraction -= inCount;

        // Si la entrada todavía no se decodificó, esperar (como en el modo normal).
        if (stretchReadPos + inCount * main.ratio + 2.0 >= (double) decoded && ! main.track->isComplete())
            return false;

        // La entrada del stretch también respeta el loop (el stretch suaviza la unión).
        for (int i = 0; i < inCount; ++i)
        {
            if (loopActive && loopValid() && stretchReadPos >= loopOut)
                stretchReadPos = loopIn + std::fmod (stretchReadPos - loopOut, loopOut - loopIn);
            readFrame (*main.track, decoded, stretchReadPos, st.inL[(size_t) i], st.inR[(size_t) i]);
            stretchReadPos += main.ratio;
        }

        st.engine.process (st.in, inCount, st.out, std::min (n, st.maxOut));
        main.pos = stretchReadPos - stretchLead();
        if (loopActive && loopValid() && main.pos < loopIn && stretchReadPos >= loopIn && stretchReadPos < loopOut)
            main.pos += loopOut - loopIn;   // lo que se escucha todavía es el final del loop
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
        const bool audible = (playing || scratching) && main.track != nullptr && mainGain > 0.0f;
        if (! audible)
            return;

        // Si había otro fundido en curso con una pista vieja, esa pista se retira.
        if (ghostRemaining > 0)
            endGhost();

        ++ghostSerial;
        ghost          = main;
        // El fantasma puede nacer a mitad de bloque (salto de loop): su largo
        // decodificado se toma acá, no en la foto del comienzo del bloque.
        ghostDecoded   = main.track->getDecodedFrames();
        ghostRate      = rate;
        ghostGain      = mainGain * (usingStretch ? 1.0f : motionGain);
        ghostStep      = ghostGain / (float) fadeLength;
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
        hotCues.fill (-1.0);
        loopActive = rolling = manualInPending = false;
        loopIn = loopOut = -1.0;
        slipTracking = false;
        reverse = scratching = false;
        effect = Effect::none;
        bend = bendTarget = 0.0;
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
        jumpTo (std::floor (juce::jlimit (0.0, length, seconds * main.track->getSampleRate())));
    }

    void Deck::jumpTo (double frames) noexcept
    {
        if (playing || scratching)
        {
            startGhost();
            mainGain = 0.0f;
        }
        if (scratching)
            scratchTarget += frames - main.pos;
        main.pos = std::max (0.0, frames);
        needPrime = true;
    }

    // =========================================================================
    //  Hot cues
    // =========================================================================
    void Deck::hotCue (int index) noexcept
    {
        if (main.track == nullptr || ! juce::isPositiveAndBelow (index, kNumHotCues))
            return;

        auto& cue = hotCues[(size_t) index];
        if (cue < 0.0)
        {
            cue = std::floor (main.pos);   // vacío: se graba acá
            publish();
            return;
        }

        // Saltar fuera del loop activo lo cierra (si no, el loop lo traería de vuelta).
        if (loopActive && (cue < loopIn || cue >= loopOut))
            loopActive = false;

        jumpTo (cue);
        if (! playing)
            play();
        cuePreview = false;
        publish();
    }

    void Deck::setHotCue (int index, double seconds) noexcept
    {
        if (! juce::isPositiveAndBelow (index, kNumHotCues))
            return;
        hotCues[(size_t) index] = (seconds < 0.0 || main.track == nullptr) ? -1.0
                                                                            : std::floor (seconds * main.track->getSampleRate());
        publish();
    }

    // =========================================================================
    //  Loops
    // =========================================================================
    double Deck::framesPerBeat() const noexcept
    {
        if (main.track == nullptr)
            return 0.0;
        const auto bpm = main.track->getBpm() > 0.0 ? main.track->getBpm() : 120.0;
        return 60.0 / bpm * main.track->getSampleRate();
    }

    void Deck::setLoop (double inFrames, double beats) noexcept
    {
        loopIn     = std::max (0.0, inFrames);
        loopOut    = loopIn + std::max (16.0, beats * framesPerBeat());
        loopBeats  = beats;
        loopActive = true;
        manualInPending = false;
        // Si ya pasamos el final (loop achicado), volver adentro con fundido.
        if (main.pos >= loopOut)
            jumpTo (loopIn + std::fmod (main.pos - loopIn, loopOut - loopIn));
    }

    void Deck::autoLoop (double beats) noexcept
    {
        if (main.track == nullptr)
            return;
        beats = juce::jlimit (kMinLoopBeats, kMaxLoopBeats, beats);

        if (loopActive && ! rolling)
        {
            if (std::abs (loopBeats - beats) < 1.0e-9)
            {
                loopExit();   // mismo tamaño: el botón apaga el loop
                return;
            }
            setLoop (loopIn, beats);   // otro tamaño: cambia el largo desde el mismo inicio
            publish();
            return;
        }

        // Inicio cuantizado a la grilla (al beat, o a la fracción de beat si el loop es corto).
        const auto spb  = framesPerBeat();
        const auto unit = spb * std::min (1.0, beats);
        auto in = main.pos;
        if (main.track->getBpm() > 0.0 && unit > 0.0)
        {
            const auto first = main.track->getFirstBeatSeconds() * main.track->getSampleRate();
            in = first + std::floor ((main.pos - first) / unit + 1.0e-6) * unit;
        }
        setLoop (in, beats);
        updateSlip();
        publish();
    }

    void Deck::loopRoll (double beats, bool on) noexcept
    {
        if (main.track == nullptr)
            return;
        if (on)
        {
            rolling = false;
            loopActive = false;
            rolling = true;
            updateSlip();   // la posición fantasma arranca ANTES de saltar
            const auto unit = framesPerBeat() * std::min (1.0, beats);
            auto in = main.pos;
            if (main.track->getBpm() > 0.0 && unit > 0.0)
            {
                const auto first = main.track->getFirstBeatSeconds() * main.track->getSampleRate();
                in = first + std::floor ((main.pos - first) / unit + 1.0e-6) * unit;
            }
            setLoop (in, juce::jlimit (kMinLoopBeats, kMaxLoopBeats, beats));
        }
        else if (rolling)
        {
            rolling = false;
            loopActive = false;
            updateSlip();   // vuelve a donde habría estado
        }
        publish();
    }

    void Deck::loopInPoint() noexcept
    {
        if (main.track == nullptr)
            return;
        loopActive = false;
        loopIn = std::floor (main.pos);
        loopOut = -1.0;
        manualInPending = true;
        updateSlip();
        publish();
    }

    void Deck::loopOutPoint() noexcept
    {
        if (main.track == nullptr)
            return;
        if (loopActive)
        {
            loopExit();
            return;
        }
        if (manualInPending && main.pos > loopIn + 16.0)
        {
            loopOut = std::floor (main.pos);
            const auto spb = framesPerBeat();
            loopBeats = spb > 0.0 ? (loopOut - loopIn) / spb : 0.0;
            loopActive = true;
            manualInPending = false;
            jumpTo (loopIn);
        }
        else if (loopValid())
        {
            loopActive = true;   // reloop
            jumpTo (loopIn);
        }
        updateSlip();
        publish();
    }

    void Deck::loopHalve() noexcept
    {
        if (! loopValid())
            return;
        const auto len = (loopOut - loopIn) * 0.5;
        if (len < framesPerBeat() * kMinLoopBeats * 0.99)
            return;
        loopOut = loopIn + len;
        loopBeats *= 0.5;
        if (loopActive && main.pos >= loopOut)
            jumpTo (loopIn + std::fmod (main.pos - loopIn, len));
        publish();
    }

    void Deck::loopDouble() noexcept
    {
        if (! loopValid() || main.track == nullptr)
            return;
        const auto len = (loopOut - loopIn) * 2.0;
        if (len > framesPerBeat() * kMaxLoopBeats * 1.01 || loopIn + len > (double) main.track->getLengthFrames())
            return;
        loopOut = loopIn + len;
        loopBeats *= 2.0;
        publish();
    }

    void Deck::loopExit() noexcept
    {
        loopActive = false;
        rolling = false;
        updateSlip();
        publish();
    }

    // =========================================================================
    //  Slip, reversa y efectos
    // =========================================================================
    void Deck::updateSlip() noexcept
    {
        const bool needed = rolling || effect == Effect::censor || effect == Effect::backspin
                            || (slipMode && (scratching || loopActive || reverse));
        if (needed && ! slipTracking)
        {
            slipTracking = true;
            slipPos = main.pos;
        }
        else if (! needed && slipTracking)
        {
            slipTracking = false;
            if (playing)
                jumpTo (slipPos);
        }
    }

    void Deck::setSlip (bool on) noexcept
    {
        slipMode = on;
        updateSlip();
        publish();
    }

    void Deck::setReverse (bool on) noexcept
    {
        if (reverse == on)
            return;
        startGhost();          // fundido entre la dirección vieja y la nueva
        mainGain = 0.0f;
        reverse = on;
        updateSlip();
        publish();
    }

    void Deck::setEffect (Effect e, bool on) noexcept
    {
        if (main.track == nullptr)
            return;
        if (on)
        {
            if (! playing || cuePreview)
                return;
            if (effect != Effect::none)
                setEffect (effect, false);
            effect = e;
            brakeSpeed = 1.0;
            spinSpeed = -3.0;   // el backspin arranca a 3x hacia atrás y frena solo
            if (e == Effect::censor)
            {
                startGhost();
                mainGain = 0.0f;
            }
            updateSlip();
        }
        else if (effect == e)
        {
            effect = Effect::none;
            // Al soltar: el freno sigue desde donde quedó; backspin y censor vuelven
            // (slip) a donde la pista habría estado.
            if (e == Effect::brake || e == Effect::censor)
            {
                startGhost();
                mainGain = 0.0f;
            }
            updateSlip();
        }
        publish();
    }

    // =========================================================================
    //  Plato
    // =========================================================================
    void Deck::scratchBegin() noexcept
    {
        if (main.track == nullptr || scratching)
            return;
        scratching    = true;
        scratchTarget = main.pos;
        scratchVel    = playing ? main.ratio * effRate : 0.0;   // la mano frena el disco suavemente
        cuePreview    = false;
        updateSlip();
        publish();
    }

    void Deck::scratchMove (double seconds) noexcept
    {
        if (! scratching || main.track == nullptr)
            return;
        const auto length = (double) main.track->getLengthFrames();
        scratchTarget = juce::jlimit (0.0, std::max (0.0, length), scratchTarget + seconds * main.track->getSampleRate());
    }

    void Deck::scratchEnd() noexcept
    {
        if (! scratching)
            return;
        scratching = false;
        if (! playing)
            mainGain = std::min (mainGain, 1.0f);
        needPrime = true;
        updateSlip();
        publish();
    }

    void Deck::nudge (double amount) noexcept
    {
        bendTarget = juce::jlimit (-0.3, 0.3, amount);
    }

    double Deck::nextAdvance() noexcept
    {
        if (scratching)
        {
            const auto maxV = main.ratio * 8.0;
            const auto want = juce::jlimit (-maxV, maxV, (scratchTarget - main.pos) * scratchK);
            scratchVel = scratchVel * scratchSmooth + want * (1.0 - scratchSmooth);
            return scratchVel;
        }
        switch (effect)
        {
            case Effect::brake:
                brakeSpeed = std::max (0.0, brakeSpeed - brakeStep);
                return main.ratio * effRate * brakeSpeed * (reverse ? -1.0 : 1.0);
            case Effect::backspin:
                spinSpeed *= spinDecay;
                return main.ratio * spinSpeed;
            case Effect::censor:
                return -main.ratio * effRate;
            case Effect::none:
                break;
        }
        return main.ratio * effRate * (reverse ? -1.0 : 1.0);
    }

    void Deck::wrapLoop (double previousPos, double advance) noexcept
    {
        if (! loopActive || ! loopValid())
            return;
        const auto len = loopOut - loopIn;
        if (advance > 0.0 && main.pos >= loopOut && previousPos < loopOut)
            jumpTo (loopIn + std::fmod (main.pos - loopOut, len));
        else if (advance < 0.0 && main.pos < loopIn && previousPos >= loopIn)
            jumpTo (loopOut - std::fmod (loopIn - main.pos, len));
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
        std::int64_t mainDecoded = 0, mainLength = 0;
        bool mainComplete = false;
        if (main.track != nullptr)
        {
            mainDecoded  = main.track->getDecodedFrames();
            mainLength   = main.track->getLengthFrames();
            mainComplete = main.track->isComplete();
        }
        if (ghost.track != nullptr)
            ghostDecoded = ghost.track->getDecodedFrames();
        else
            ghostDecoded = 0;

        // Pitch bend: se desvanece solo si no se sigue empujando el plato.
        const auto blockSeconds = numSamples / engineSampleRate;
        bend += (bendTarget - bend) * std::min (1.0, blockSeconds / 0.03);
        bendTarget *= std::exp (-blockSeconds / 0.15);
        if (std::abs (bendTarget) < 1.0e-4 && std::abs (bend) < 1.0e-4)
            bend = bendTarget = 0.0;
        effRate = rate * (1.0 + bend);

        // Key Lock / key shift: el bloque sale del time-stretch, salvo durante
        // scratch, reversa o efectos (tienen que sonar como un vinilo).
        const bool wantStretch = stretchActive && ! isManipulated();
        if (wantStretch != usingStretch)
        {
            startGhost();
            mainGain = 0.0f;
            usingStretch = wantStretch;
            needPrime = true;
        }

        bool stretchedBlock = false;
        if (usingStretch && playing && main.track != nullptr && numSamples <= stretcher->maxOut
            && ! (mainComplete && main.pos >= (double) mainLength))
        {
            stretchedBlock = renderStretched (numSamples, mainDecoded);
            if (! stretchedBlock)
            {
                mainGain = 0.0f;     // esperando decodificación: silencio y re-cebado
                needPrime = true;
            }
        }

        // Posición "fantasma" del slip: avanza como si nada se hubiera tocado.
        if (slipTracking && playing)
            slipPos += numSamples * main.ratio * rate;

        for (int n = 0; n < numSamples; ++n)
        {
            float outL = 0.0f, outR = 0.0f;
            const auto serialAtStart = ghostSerial;

            if (stretchedBlock)
            {
                outL += stretcher->outL[(size_t) n] * mainGain;
                outR += stretcher->outR[(size_t) n] * mainGain;
                if (mainGain < 1.0f)
                    mainGain = std::min (1.0f, mainGain + fadeStep);
            }
            else if ((playing || scratching) && main.track != nullptr && ! usingStretch)
            {
                const auto advance = nextAdvance();
                if (! scratching && advance >= 0.0 && mainComplete && main.pos >= (double) mainLength)
                {
                    // Fin de la pista.
                    playing    = false;
                    cuePreview = false;
                    mainGain   = 0.0f;
                    main.pos   = (double) mainLength;
                    effect     = Effect::none;
                    loopActive = false;
                    slipTracking = false;
                }
                else if (main.pos + 2.0 >= (double) mainDecoded && ! mainComplete && advance >= 0.0)
                {
                    // Todavía no se decodificó esta parte: esperar sin avanzar y
                    // volver a entrar con fundido cuando llegue el audio.
                    mainGain = 0.0f;
                }
                else
                {
                    // Disco casi quieto (scratch con la mano quieta, fin del freno): silencio
                    // gradual en vez de un nivel de continua fijo.
                    const auto speed = std::abs (advance) / main.ratio;
                    const auto motionTarget = (float) std::min (1.0, speed * 8.0);
                    motionGain += (motionTarget - motionGain) * 0.005f;

                    float l, r;
                    readFrame (*main.track, mainDecoded, main.pos, l, r);
                    outL += l * mainGain * motionGain;
                    outR += r * mainGain * motionGain;
                    const auto previous = main.pos;
                    main.pos = std::max (0.0, main.pos + advance);
                    if (mainComplete)
                        main.pos = std::min (main.pos, (double) mainLength);
                    wrapLoop (previous, advance);
                    if (mainGain < 1.0f)
                        mainGain = std::min (1.0f, mainGain + fadeStep);
                }
            }

            if (ghostRemaining > 0 && ghostSerial == serialAtStart)
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
        if (usingStretch && playing && main.track != nullptr && mainComplete && main.pos >= (double) mainLength)
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
        for (int i = 0; i < kNumHotCues; ++i)
            pubHotCues[(size_t) i].store (hotCues[(size_t) i] < 0.0 ? -1.0 : hotCues[(size_t) i] / sr, std::memory_order_relaxed);
        pubLoopActive.store (loopActive, std::memory_order_relaxed);
        pubLoopIn.store (loopIn < 0.0 ? -1.0 : loopIn / sr, std::memory_order_relaxed);
        pubLoopOut.store (loopOut < 0.0 ? -1.0 : loopOut / sr, std::memory_order_relaxed);
        pubSlipMode.store (slipMode, std::memory_order_relaxed);
        pubSlipping.store (slipTracking, std::memory_order_relaxed);
        pubSlip.store (slipPos / sr, std::memory_order_relaxed);
        pubReverse.store (reverse, std::memory_order_relaxed);
        pubScratching.store (scratching, std::memory_order_relaxed);
        pubRolling.store (rolling, std::memory_order_relaxed);
        pubEffect.store ((int) effect, std::memory_order_relaxed);
    }

    Deck::Status Deck::getStatus() const noexcept
    {
        Status s;
        s.trackId         = pubTrackId.load (std::memory_order_relaxed);
        s.playing         = pubPlaying.load (std::memory_order_relaxed);
        s.cuePreview      = pubCuePreview.load (std::memory_order_relaxed);
        s.positionSeconds = pubPosition.load (std::memory_order_relaxed);
        s.cueSeconds      = pubCue.load (std::memory_order_relaxed);
        for (int i = 0; i < kNumHotCues; ++i)
            s.hotCues[(size_t) i] = pubHotCues[(size_t) i].load (std::memory_order_relaxed);
        s.loopActive     = pubLoopActive.load (std::memory_order_relaxed);
        s.loopInSeconds  = pubLoopIn.load (std::memory_order_relaxed);
        s.loopOutSeconds = pubLoopOut.load (std::memory_order_relaxed);
        s.slipMode       = pubSlipMode.load (std::memory_order_relaxed);
        s.slipping       = pubSlipping.load (std::memory_order_relaxed);
        s.slipSeconds    = pubSlip.load (std::memory_order_relaxed);
        s.reverse        = pubReverse.load (std::memory_order_relaxed);
        s.scratching     = pubScratching.load (std::memory_order_relaxed);
        s.rolling        = pubRolling.load (std::memory_order_relaxed);
        s.effect         = pubEffect.load (std::memory_order_relaxed);
        return s;
    }
} // namespace kb::engine
