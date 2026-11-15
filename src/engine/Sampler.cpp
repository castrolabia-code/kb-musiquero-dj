#include "engine/Sampler.h"

#include "engine/Deck.h"

#include <algorithm>
#include <cmath>

namespace kb::engine
{
    Sampler::Sampler()
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            volume[(size_t) i].store (0.8f);
            looping[(size_t) i].store (false);
            progress[(size_t) i].store (-1.0f);
        }
    }

    void Sampler::prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        fadeStep = 1.0f / std::max (16.0f, (float) (0.002 * sampleRate));
        for (auto& v : voices)
            v.ratio = v.track != nullptr ? v.track->getSampleRate() / sampleRate : 1.0;
    }

    void Sampler::retire (TrackBuffer* t) noexcept
    {
        if (t != nullptr && numRetired < (int) retired.size())
            retired[(size_t) numRetired++] = t;
    }

    void Sampler::dropRetired() noexcept
    {
        if (numRetired == 0)
            return;
        for (int i = 1; i < numRetired; ++i)
            retired[(size_t) (i - 1)] = retired[(size_t) i];
        retired[(size_t) --numRetired] = nullptr;
    }

    void Sampler::load (int slot, TrackBuffer* track) noexcept
    {
        if (! juce::isPositiveAndBelow (slot, kNumSlots))
            return;
        auto& v = voices[(size_t) slot];
        if (v.track == track)
            return;
        retire (v.track);
        v = {};
        v.track = track;
        v.ratio = track != nullptr ? track->getSampleRate() / sampleRate : 1.0;
        progress[(size_t) slot].store (-1.0f, std::memory_order_relaxed);
    }

    void Sampler::trigger (int slot) noexcept
    {
        if (! juce::isPositiveAndBelow (slot, kNumSlots))
            return;
        auto& v = voices[(size_t) slot];
        if (v.track == nullptr)
            return;

        if (v.playing && ! v.stopping)
        {
            if (looping[(size_t) slot].load (std::memory_order_relaxed))
                v.stopping = true;    // loop: tocar de nuevo lo apaga
            else
                v.restart = true;     // one-shot: reinicia (con fundido)
            return;
        }
        v.playing  = true;
        v.stopping = false;
        v.restart  = false;
        v.pos      = 0.0;
        v.fade     = 0.0f;
        v.gain     = volume[(size_t) slot].load (std::memory_order_relaxed);
    }

    void Sampler::stop (int slot) noexcept
    {
        if (juce::isPositiveAndBelow (slot, kNumSlots) && voices[(size_t) slot].playing)
            voices[(size_t) slot].stopping = true;
    }

    void Sampler::render (float* left, float* right, int n) noexcept
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            auto& v = voices[(size_t) s];
            if (! v.playing || v.track == nullptr)
                continue;

            const auto decoded  = v.track->getDecodedFrames();
            const auto complete = v.track->isComplete();
            const auto length   = (double) v.track->getLengthFrames();
            const auto loop     = looping[(size_t) s].load (std::memory_order_relaxed);
            const auto target   = volume[(size_t) s].load (std::memory_order_relaxed);
            const auto gainStep = (target - v.gain) / (float) n;

            for (int i = 0; i < n && v.playing; ++i)
            {
                const bool fadingOut = v.stopping || v.restart;
                if (fadingOut)
                {
                    v.fade -= fadeStep;
                    if (v.fade <= 0.0f)
                    {
                        v.fade = 0.0f;
                        if (v.restart) { v.restart = false; v.pos = 0.0; }
                        else           { v.playing = v.stopping = false; break; }
                    }
                }
                else if (v.fade < 1.0f)
                {
                    v.fade = std::min (1.0f, v.fade + fadeStep);
                }

                if (v.pos + 2.0 >= (double) decoded && ! complete)
                    continue;   // todavía decodificando: silencio

                if (complete && v.pos >= length)
                {
                    if (loop && length > 0.0)
                        v.pos = std::fmod (v.pos, length);
                    else
                    {
                        v.playing = false;
                        break;
                    }
                }

                float l, r;
                Deck::readFrame (*v.track, decoded, v.pos, l, r);
                v.gain += gainStep;
                const auto g = v.fade * v.gain;
                left[i]  += l * g;
                right[i] += r * g;
                v.pos += v.ratio;
            }
            v.gain = target;
            progress[(size_t) s].store (v.playing && length > 0.0 ? (float) (v.pos / length) : -1.0f, std::memory_order_relaxed);
        }
    }

    void Sampler::setVolume (int slot, float g) noexcept
    {
        if (juce::isPositiveAndBelow (slot, kNumSlots))
            volume[(size_t) slot].store (juce::jlimit (0.0f, 1.0f, g), std::memory_order_relaxed);
    }

    float Sampler::getVolume (int slot) const noexcept
    {
        return juce::isPositiveAndBelow (slot, kNumSlots) ? volume[(size_t) slot].load (std::memory_order_relaxed) : 0.0f;
    }

    void Sampler::setLooping (int slot, bool l) noexcept
    {
        if (juce::isPositiveAndBelow (slot, kNumSlots))
            looping[(size_t) slot].store (l, std::memory_order_relaxed);
    }

    bool Sampler::isLooping (int slot) const noexcept
    {
        return juce::isPositiveAndBelow (slot, kNumSlots) && looping[(size_t) slot].load (std::memory_order_relaxed);
    }

    bool Sampler::isPlaying (int slot) const noexcept
    {
        return getProgress (slot) >= 0.0f;
    }

    float Sampler::getProgress (int slot) const noexcept
    {
        return juce::isPositiveAndBelow (slot, kNumSlots) ? progress[(size_t) slot].load (std::memory_order_relaxed) : -1.0f;
    }
} // namespace kb::engine
