#include "engine/ChannelStrip.h"

namespace kb::engine
{
    static constexpr double kSmoothingSeconds = 0.02;

    float ChannelStrip::trimKnobToGain (float p) noexcept
    {
        const auto db = 24.0f * (juce::jlimit (0.0f, 1.0f, p) - 0.5f);   // -12 .. +12 dB
        return juce::Decibels::decibelsToGain (db);
    }

    void ChannelStrip::prepare (double sampleRate) noexcept
    {
        for (auto& e : eq)
            e.prepare (sampleRate);
        filter.prepare (sampleRate);

        for (auto* s : { &trimGain, &lowGain, &midGain, &highGain })
        {
            s->reset (sampleRate, kSmoothingSeconds);
            s->setCurrentAndTargetValue (1.0f);
        }
    }

    void ChannelStrip::process (float* left, float* right, int n, const Params& p) noexcept
    {
        trimGain.setTargetValue (trimKnobToGain (p.trim));
        lowGain.setTargetValue  (dsp::eqKnobToGain (p.low));
        midGain.setTargetValue  (dsp::eqKnobToGain (p.mid));
        highGain.setTargetValue (dsp::eqKnobToGain (p.high));

        for (int i = 0; i < n; ++i)
        {
            const auto t  = (double) trimGain.getNextValue();
            const auto gl = (double) lowGain.getNextValue();
            const auto gm = (double) midGain.getNextValue();
            const auto gh = (double) highGain.getNextValue();

            left[i]  = (float) eq[0].process ((double) left[i]  * t, gl, gm, gh);
            right[i] = (float) eq[1].process ((double) right[i] * t, gl, gm, gh);
        }

        filter.setPosition (p.filter);
        filter.process (left, right, n);

        meter.addBlock (left, right, n);
    }
} // namespace kb::engine
