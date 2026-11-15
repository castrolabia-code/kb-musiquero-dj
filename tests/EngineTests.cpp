// Pruebas del motor de audio (Fase 0: contrato básico de prepare/process).
#include <catch2/catch_test_macros.hpp>

#include "engine/AudioEngine.h"

#include <vector>

using kb::engine::AudioEngine;
using kb::engine::EngineConfig;

namespace
{
    // Buffers de salida "sucios" para comprobar que el motor los limpia.
    struct Outputs
    {
        Outputs (int channels, int samples)
            : data (static_cast<size_t> (channels), std::vector<float> (static_cast<size_t> (samples), 0.123f))
        {
            for (auto& ch : data)
                ptrs.push_back (ch.data());
        }

        bool allZero() const
        {
            for (const auto& ch : data)
                for (auto s : ch)
                    if (! juce::exactlyEqual (s, 0.0f))
                        return false;
            return true;
        }

        std::vector<std::vector<float>> data;
        std::vector<float*> ptrs;
    };
}

TEST_CASE ("AudioEngine: prepare guarda la configuración", "[engine]")
{
    AudioEngine engine;
    REQUIRE_FALSE (engine.isPrepared());

    engine.prepare ({ 44100.0, 256, 4 });
    CHECK (engine.isPrepared());
    CHECK (juce::exactlyEqual (engine.getSampleRate(), 44100.0));
    CHECK (engine.getMaxBlockSize() == 256);
    CHECK (engine.getNumOutputs() == 4);

    engine.release();
    CHECK_FALSE (engine.isPrepared());
}

TEST_CASE ("AudioEngine: limita los canales al máximo soportado", "[engine]")
{
    AudioEngine engine;
    engine.prepare ({ 48000.0, 128, 16 });
    CHECK (engine.getNumOutputs() == AudioEngine::kMaxOutputChannels);
}

TEST_CASE ("AudioEngine: en la Fase 0 genera silencio en todas las salidas", "[engine]")
{
    AudioEngine engine;
    engine.prepare ({ 48000.0, 512, 4 });

    Outputs out (4, 512);
    engine.process (out.ptrs.data(), 4, 512);
    CHECK (out.allZero());
    CHECK (engine.getProcessedBlocks() == 1);
}

TEST_CASE ("AudioEngine: process es robusto ante entradas atípicas", "[engine]")
{
    AudioEngine engine;
    engine.prepare ({ 48000.0, 64, 2 });

    SECTION ("bloque más grande que maxBlockSize")
    {
        Outputs out (2, 1024);
        engine.process (out.ptrs.data(), 2, 1024);
        CHECK (out.allZero());
    }

    SECTION ("punteros nulos y bloques vacíos")
    {
        engine.process (nullptr, 2, 64);
        Outputs out (2, 64);
        engine.process (out.ptrs.data(), 2, 0);
        float* withNull[] = { out.ptrs[0], nullptr };
        engine.process (withNull, 2, 64);
        CHECK (juce::exactlyEqual (out.data[0][0], 0.0f));
    }

    SECTION ("sin prepare previo no falla")
    {
        AudioEngine fresh;
        Outputs out (2, 32);
        fresh.process (out.ptrs.data(), 2, 32);
        CHECK (out.allZero());
    }
}
