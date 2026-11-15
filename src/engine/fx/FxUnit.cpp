#include "engine/fx/FxUnit.h"

#include <algorithm>
#include <cmath>

namespace kb::engine::fx
{
    namespace
    {
        constexpr double kTwoPi = 6.283185307179586;

        /** Filtro de variable de estado TPT, una muestra. mode: 0 LP, 1 BP, 2 HP. */
        inline float svf (double& ic1, double& ic2, float x, double g, double k, int mode) noexcept
        {
            const auto a1 = 1.0 / (1.0 + g * (g + k));
            const auto a2 = g * a1;
            const auto a3 = g * a2;
            const auto v3 = (double) x - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            if (mode == 0) return (float) v2;
            if (mode == 1) return (float) v1;
            return (float) ((double) x - k * v1 - v2);
        }
    }

    FxUnit::FxUnit() = default;

    bool FxUnit::usesBeats (Type t) noexcept
    {
        return t != Type::reverb && t != Type::bitcrusher && t != Type::noise;
    }

    void FxUnit::prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        size = juce::jmax (1024, (int) std::ceil (kMaxBufferSeconds * sr));
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        scratchL.assign ((size_t) juce::jmax (64, maxBlockSize), 0.0f);
        scratchR.assign ((size_t) juce::jmax (64, maxBlockSize), 0.0f);
        reverb.setSampleRate (sr);
        wetSmooth.reset (sr, 0.03);
        paramSmooth.reset (sr, 0.05);
        wetSmooth.setCurrentAndTargetValue (wet.load());
        paramSmooth.setCurrentAndTargetValue (param.load());
        onStep = 1.0f / (float) (0.008 * sr);   // fundidos de 8 ms
        writePos = 0;
        resetState();
        applied = getType();
        active = false;
        onGain = 0.0f;
    }

    void FxUnit::resetState() noexcept
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        fbLpL = fbLpR = 0.0f;
        delaySmoothed = 0.0;
        lfoPhase = 0.0;
        apL.fill (0.0f); apR.fill (0.0f);
        phaserFbL = phaserFbR = 0.0f;
        svf1L = svf2L = svf1R = svf2R = 0.0;
        holdL = holdR = 0.0f;
        holdCounter = 0.0;
        gatePhase = 0.0;
        gateGain = 1.0f;
        nz1L = nz2L = nz1R = nz2R = 0.0;
        reverb.reset();
        tailSilence = 0.0;
    }

    void FxUnit::startTrigger() noexcept
    {
        triggerStart   = writePos;
        triggerPos     = (double) writePos;
        triggerElapsed = 0.0;
        triggerSpeed   = applied == Type::backspin ? -3.0 : 1.0;
    }

    float FxUnit::readBuffer (const std::vector<float>& buf, double pos) const noexcept
    {
        // Lectura con interpolación lineal en el buffer circular.
        pos = std::fmod (pos, (double) size);
        if (pos < 0.0) pos += size;
        const auto i0 = (int) pos;
        const auto i1 = i0 + 1 >= size ? 0 : i0 + 1;
        const auto f = (float) (pos - i0);
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }

    void FxUnit::process (float* L, float* R, int n, double bpm) noexcept
    {
        const bool on = enabled.load (std::memory_order_relaxed);
        const auto requested = getType();

        // Apagada y sin cola: no hace nada (cero costo).
        if (! on && ! active)
        {
            wasOn = false;
            applied = requested;
            return;
        }
        if (! active)
        {
            active = true;
            applied = requested;
            resetState();
        }

        if (bpm <= 1.0) bpm = 120.0;
        const auto spb = 60.0 / bpm * sr;                                   // muestras por beat
        const auto beats = kBeats[(size_t) getBeatsIndex()];
        const auto beatLen = juce::jlimit (16.0, (double) size - 4.0, beats * spb);
        wetSmooth.setTargetValue (wet.load (std::memory_order_relaxed));
        paramSmooth.setTargetValue (param.load (std::memory_order_relaxed));

        // Efectos de disparo (roll, brake, backspin): arrancan al encender.
        if (on && ! wasOn)
            startTrigger();
        wasOn = on;

        const bool switching = requested != applied;
        const bool tail = hasTail (applied);
        float tailPeak = 0.0f;   // salida del efecto en este bloque (para saber si la cola terminó)

        for (int i = 0; i < n; ++i)
        {
            // Fundido de encendido / apagado / cambio de efecto.
            const auto targetGain = (on && ! switching) ? 1.0f : 0.0f;
            if (onGain < targetGain)      onGain = std::min (targetGain, onGain + onStep);
            else if (onGain > targetGain) onGain = std::max (targetGain, onGain - onStep);

            const auto w = wetSmooth.getNextValue();
            const auto p = paramSmooth.getNextValue();
            const auto inL = L[i], inR = R[i];
            float fxL = 0.0f, fxR = 0.0f;
            // Efectos con cola: la entrada se corta con onGain, la salida sigue sonando.
            const auto send = tail ? onGain : 1.0f;

            switch (applied)
            {
                case Type::echo:
                case Type::delay:
                {
                    delaySmoothed = delaySmoothed <= 0.0 ? beatLen : delaySmoothed + (beatLen - delaySmoothed) * 0.0005;
                    const auto rp = (double) writePos - delaySmoothed;
                    const auto dL = readBuffer (bufL, rp), dR = readBuffer (bufR, rp);
                    if (applied == Type::echo)
                    {
                        const auto fb = 0.3f + 0.6f * p;
                        fbLpL += 0.35f * (dL - fbLpL);   // pierde agudos en cada repetición
                        fbLpR += 0.35f * (dR - fbLpR);
                        bufL[(size_t) writePos] = inL * send + fbLpL * fb;
                        bufR[(size_t) writePos] = inR * send + fbLpR * fb;
                    }
                    else
                    {
                        const auto fb = 0.75f * p;
                        // Ping-pong: cada repetición cambia de lado.
                        bufL[(size_t) writePos] = 0.5f * (inL + inR) * send + dR * fb;
                        bufR[(size_t) writePos] = dL * fb;
                    }
                    fxL = dL; fxR = dR;
                    break;
                }

                case Type::reverb:
                    // Se procesa por bloque más abajo (juce::Reverb).
                    scratchL[(size_t) i] = inL * send;
                    scratchR[(size_t) i] = inR * send;
                    break;

                case Type::flanger:
                {
                    lfoPhase += 1.0 / beatLen;
                    if (lfoPhase >= 1.0) lfoPhase -= 1.0;
                    const auto lfo = 0.5 + 0.5 * std::sin (kTwoPi * lfoPhase);
                    const auto d = (0.0008 + 0.005 * lfo) * sr;
                    const auto fb = 0.85f * p;
                    const auto dL = readBuffer (bufL, (double) writePos - d);
                    const auto dR = readBuffer (bufR, (double) writePos - d * 1.05);
                    bufL[(size_t) writePos] = inL + dL * fb;
                    bufR[(size_t) writePos] = inR + dR * fb;
                    fxL = 0.6f * (inL + dL);
                    fxR = 0.6f * (inR + dR);
                    break;
                }

                case Type::phaser:
                {
                    lfoPhase += 1.0 / beatLen;
                    if (lfoPhase >= 1.0) lfoPhase -= 1.0;
                    const auto lfo = 0.5 + 0.5 * std::sin (kTwoPi * lfoPhase);
                    const auto f = 200.0 * std::pow (20.0, lfo);   // 200 Hz .. 4 kHz
                    const auto t = std::tan (juce::MathConstants<double>::pi * std::min (f, sr * 0.45) / sr);
                    const auto a = (float) ((t - 1.0) / (t + 1.0));
                    auto run = [a] (std::array<float, 6>& st, float x) noexcept
                    {
                        for (auto& z : st)
                        {
                            const auto y = a * x + z;
                            z = x - a * y;
                            x = y;
                        }
                        return x;
                    };
                    const auto fb = 0.7f * p;
                    phaserFbL = run (apL, inL + phaserFbL * fb);
                    phaserFbR = run (apR, inR + phaserFbR * fb);
                    fxL = 0.6f * (inL + phaserFbL);
                    fxR = 0.6f * (inR + phaserFbR);
                    break;
                }

                case Type::filter:
                {
                    lfoPhase += 1.0 / beatLen;
                    if (lfoPhase >= 1.0) lfoPhase -= 1.0;
                    const auto lfo = 0.5 - 0.5 * std::cos (kTwoPi * lfoPhase);
                    const auto f = 18000.0 * std::pow (2.0, -6.0 * (double) p * lfo);   // baja hasta ~280 Hz
                    const auto g = std::tan (juce::MathConstants<double>::pi * std::min (f, sr * 0.45) / sr);
                    const auto k = 1.0 / 2.5;   // resonante
                    fxL = svf (svf1L, svf2L, inL, g, k, 0);
                    fxR = svf (svf1R, svf2R, inR, g, k, 0);
                    break;
                }

                case Type::bitcrusher:
                {
                    const auto bits = 16.0f - 13.0f * p;          // 16 -> 3 bits
                    const auto steps = std::pow (2.0f, bits - 1.0f);
                    const auto hold = 1.0 + 24.0 * (double) p;     // baja el sample rate hasta 1/25
                    holdCounter += 1.0;
                    if (holdCounter >= hold)
                    {
                        holdCounter -= hold;
                        holdL = std::round (inL * steps) / steps;
                        holdR = std::round (inR * steps) / steps;
                    }
                    fxL = holdL; fxR = holdR;
                    break;
                }

                case Type::gater:
                {
                    gatePhase += 1.0 / beatLen;
                    if (gatePhase >= 1.0) gatePhase -= 1.0;
                    const auto open = gatePhase < 0.5 ? 1.0f : 1.0f - (0.25f + 0.75f * p);
                    gateGain += (open - gateGain) * 0.02f;   // ~1 ms: sin clicks
                    fxL = inL * gateGain;
                    fxR = inR * gateGain;
                    break;
                }

                case Type::roll:
                {
                    // Repite el fragmento que empezó al encender (largo = beats).
                    triggerElapsed += 1.0;
                    const auto loopPos = std::fmod (triggerElapsed, beatLen);
                    fxL = readBuffer (bufL, (double) triggerStart + loopPos);
                    fxR = readBuffer (bufR, (double) triggerStart + loopPos);
                    if (triggerElapsed < beatLen)
                    {
                        bufL[(size_t) writePos] = inL;
                        bufR[(size_t) writePos] = inR;
                        fxL = inL; fxR = inR;
                    }
                    // Mini-fundido en la unión del loop.
                    const auto edge = std::min (loopPos, beatLen - loopPos) / (0.002 * sr);
                    if (triggerElapsed >= beatLen && edge < 1.0)
                    {
                        fxL *= (float) edge;
                        fxR *= (float) edge;
                    }
                    break;
                }

                case Type::brake:
                case Type::backspin:
                {
                    bufL[(size_t) writePos] = inL;
                    bufR[(size_t) writePos] = inR;
                    triggerElapsed += 1.0;
                    if (applied == Type::brake)
                        triggerSpeed = std::max (0.0, 1.0 - triggerElapsed / beatLen);
                    else
                        triggerSpeed = -3.0 * std::max (0.0, 1.0 - triggerElapsed / beatLen);
                    triggerPos += triggerSpeed;
                    // No leer más adelante de lo grabado.
                    if (triggerPos > (double) triggerStart + triggerElapsed)
                        triggerPos = (double) triggerStart + triggerElapsed;
                    const auto level = (float) std::min (1.0, std::abs (triggerSpeed) * 4.0);
                    fxL = readBuffer (bufL, triggerPos) * level;
                    fxR = readBuffer (bufR, triggerPos) * level;
                    break;
                }

                case Type::noise:
                {
                    const auto f = 150.0 * std::pow (2.0, 7.0 * (double) p);   // 150 Hz .. 19 kHz
                    const auto g = std::tan (juce::MathConstants<double>::pi * std::min (f, sr * 0.45) / sr);
                    const auto k = 1.0 / 3.0;
                    const auto nL = noiseRng.nextFloat() * 2.0f - 1.0f;
                    const auto nR = noiseRng.nextFloat() * 2.0f - 1.0f;
                    fxL = inL + 0.35f * svf (nz1L, nz2L, nL, g, k, 1);
                    fxR = inR + 0.35f * svf (nz1R, nz2R, nR, g, k, 1);
                    break;
                }

                case Type::count:
                    break;
            }

            if (tail)
                tailPeak = std::max (tailPeak, std::max (std::abs (fxL), std::abs (fxR)));

            if (applied != Type::reverb)
            {
                if (tail)
                {
                    // Eco / delay: se SUMA al original (el dry nunca baja). Al cambiar
                    // de efecto la cola se apaga con el fundido.
                    const auto g = switching ? onGain : 1.0f;
                    L[i] = inL + fxL * w * g;
                    R[i] = inR + fxR * w * g;
                }
                else
                {
                    const auto mix = w * onGain;
                    L[i] = inL + (fxL - inL) * mix;
                    R[i] = inR + (fxR - inR) * mix;
                }
            }

            if (++writePos >= size)
                writePos = 0;
        }

        if (applied == Type::reverb)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.3f + 0.65f * paramSmooth.getCurrentValue();
            rp.damping = 0.45f;
            rp.wetLevel = 1.0f;
            rp.dryLevel = 0.0f;
            rp.width = 1.0f;
            reverb.setParameters (rp);
            reverb.processStereo (scratchL.data(), scratchR.data(), n);
            const auto w = wetSmooth.getCurrentValue() * (switching ? onGain : 1.0f);
            for (int i = 0; i < n; ++i)
            {
                L[i] += scratchL[(size_t) i] * w * 0.7f;
                R[i] += scratchR[(size_t) i] * w * 0.7f;
            }
        }

        // ¿Terminó? Apagada: sin cola cuando el fundido llegó a 0; con cola, cuando
        // la salida del efecto quedó en silencio ~0,3 s.
        if (switching && onGain <= 0.0f)
        {
            applied = requested;
            resetState();
            if (on)
                startTrigger();
            return;
        }
        if (! on && onGain <= 0.0f)
        {
            if (! tail)
            {
                active = false;
                return;
            }
            if (applied == Type::reverb)
                for (int i = 0; i < n; ++i)
                    tailPeak = std::max (tailPeak, std::max (std::abs (scratchL[(size_t) i]), std::abs (scratchR[(size_t) i])));
            tailSilence = tailPeak < 1.0e-4f ? tailSilence + n / sr : 0.0;
            // Entre un eco y el siguiente hay silencio: esperar más que el tiempo del eco.
            if (tailSilence > beatLen / sr + 0.3)
                active = false;
        }
    }
} // namespace kb::engine::fx
