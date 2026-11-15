#pragma once
// Utilidades compartidas por las pruebas.
#include "engine/AudioEngine.h"
#include "engine/TrackBuffer.h"

#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace kb::test
{
    // --- Contador de allocations (ver AllocationCounter.cpp) -------------------------
    extern thread_local bool countingAllocations;
    extern std::atomic<int>  allocationCount;

    /** Cuenta new/delete en este hilo mientras exista. */
    struct AllocationGuard
    {
        AllocationGuard()  { allocationCount = 0; countingAllocations = true; }
        ~AllocationGuard() { countingAllocations = false; }
        int count() const  { return allocationCount.load(); }
    };

    /** Pista completa con un seno estéreo. */
    inline std::shared_ptr<engine::TrackBuffer> makeSineTrack (double seconds, double freq,
                                                               double sampleRate = 48000.0,
                                                               float amplitude = 0.5f)
    {
        const auto frames = (std::int64_t) std::llround (seconds * sampleRate);
        engine::TrackInfo info;
        info.title = "sine";
        auto t = std::make_shared<engine::TrackBuffer> (info, sampleRate, frames);
        for (int c = 0; c < engine::TrackBuffer::kNumChannels; ++c)
        {
            auto* d = t->getWritePointer (c);
            for (std::int64_t i = 0; i < frames; ++i)
                d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sampleRate);
        }
        t->markComplete (frames);
        return t;
    }

    /** Pista completa con una rampa: el valor de cada frame identifica su posición. */
    inline std::shared_ptr<engine::TrackBuffer> makeRampTrack (std::int64_t frames, double sampleRate = 48000.0)
    {
        engine::TrackInfo info;
        auto t = std::make_shared<engine::TrackBuffer> (info, sampleRate, frames);
        for (int c = 0; c < engine::TrackBuffer::kNumChannels; ++c)
        {
            auto* d = t->getWritePointer (c);
            for (std::int64_t i = 0; i < frames; ++i)
                d[i] = (float) i / (float) frames;
        }
        t->markComplete (frames);
        return t;
    }

    /** Corre el motor 'frames' muestras en bloques y devuelve L y R. */
    struct Rendered
    {
        std::vector<float> left, right;
    };

    inline Rendered run (engine::AudioEngine& engine, int frames, int blockSize = 256)
    {
        Rendered out;
        out.left.resize ((size_t) frames);
        out.right.resize ((size_t) frames);
        for (int pos = 0; pos < frames; pos += blockSize)
        {
            const auto n = std::min (blockSize, frames - pos);
            float* chans[] = { out.left.data() + pos, out.right.data() + pos };
            engine.process (chans, 2, n);
        }
        return out;
    }

    /** Mayor salto entre dos muestras consecutivas (detector de clicks). */
    inline float maxStep (const std::vector<float>& x, size_t from = 1)
    {
        float m = 0.0f;
        for (size_t i = std::max<size_t> (from, 1); i < x.size(); ++i)
            m = std::max (m, std::abs (x[i] - x[i - 1]));
        return m;
    }

    inline float rms (const float* x, size_t n)
    {
        double acc = 0.0;
        for (size_t i = 0; i < n; ++i)
            acc += (double) x[i] * (double) x[i];
        return n > 0 ? (float) std::sqrt (acc / (double) n) : 0.0f;
    }

    /** Motor preparado a 48 kHz, master y volúmenes al máximo, crossfader al deck 1. */
    inline void prepareUnity (engine::AudioEngine& engine, double sr = 48000.0, int block = 256)
    {
        engine.prepare ({ sr, block, 2 });
        engine.setMasterVolume (1.0f);
        engine.setChannelVolume (0, 1.0f);
        engine.setChannelVolume (1, 1.0f);
        engine.setCrossfader (-1.0f);
    }
} // namespace kb::test
