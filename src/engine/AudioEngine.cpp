#include "engine/AudioEngine.h"

#include <algorithm>

namespace kb::engine
{
    void AudioEngine::prepare (const EngineConfig& config)
    {
        jassert (config.sampleRate > 0.0);
        jassert (config.maxBlockSize > 0);

        const auto channels = std::clamp (config.numOutputChannels, 0, kMaxOutputChannels);
        const auto block    = std::max (config.maxBlockSize, 1);

        // Reserva única: process() trabaja siempre dentro de este buffer.
        scratch.setSize (kMaxOutputChannels, block, false, true, false);
        scratch.clear();

        sampleRate.store (config.sampleRate, std::memory_order_relaxed);
        maxBlockSize.store (block, std::memory_order_relaxed);
        numOutputs.store (channels, std::memory_order_relaxed);
        processedBlocks.store (0, std::memory_order_relaxed);
        prepared.store (true, std::memory_order_release);
    }

    void AudioEngine::release()
    {
        prepared.store (false, std::memory_order_release);
        scratch.setSize (0, 0);
    }

    void AudioEngine::process (float* const* outputs, int numOutputChannels, int numSamples) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        if (outputs == nullptr || numSamples <= 0)
            return;

        // Fase 0: silencio. Desde la Fase 1 acá se mezclan los decks.
        for (int ch = 0; ch < numOutputChannels; ++ch)
            if (auto* out = outputs[ch])
                juce::FloatVectorOperations::clear (out, numSamples);

        processedBlocks.fetch_add (1, std::memory_order_relaxed);
    }
} // namespace kb::engine
