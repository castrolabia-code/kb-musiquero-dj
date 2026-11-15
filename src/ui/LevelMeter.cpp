#include "ui/LevelMeter.h"

#include "ui/Palette.h"

#include <array>

namespace kb::ui
{
    using namespace palette;

    namespace
    {
        // Puntos de la escala: (dBFS, posición). Lineal por tramos.
        constexpr std::array<std::pair<float, float>, 10> kScale { {
            { -60.0f, 0.00f }, { -48.0f, 0.08f }, { -36.0f, 0.20f }, { -24.0f, 0.36f }, { -18.0f, 0.46f },
            { -12.0f, 0.60f }, {  -9.0f, 0.68f }, {  -6.0f, 0.78f }, {  -3.0f, 0.89f }, {   0.0f, 1.00f } } };

        float positionToDb (float pos) noexcept
        {
            pos = juce::jlimit (0.0f, 1.0f, pos);
            for (size_t i = 1; i < kScale.size(); ++i)
            {
                if (pos <= kScale[i].second)
                {
                    const auto [d0, p0] = kScale[i - 1];
                    const auto [d1, p1] = kScale[i];
                    return d0 + (d1 - d0) * (pos - p0) / (p1 - p0);
                }
            }
            return 0.0f;
        }
    }

    float LevelMeter::dbToPosition (float db) noexcept
    {
        if (db <= kScale.front().first) return 0.0f;
        if (db >= 0.0f)                 return 1.0f;
        for (size_t i = 1; i < kScale.size(); ++i)
        {
            if (db <= kScale[i].first)
            {
                const auto [d0, p0] = kScale[i - 1];
                const auto [d1, p1] = kScale[i];
                return p0 + (p1 - p0) * (db - d0) / (d1 - d0);
            }
        }
        return 1.0f;
    }

    const std::vector<float>& LevelMeter::scaleMarks()
    {
        static const std::vector<float> marks { -48.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f };
        return marks;
    }

    LevelMeter::LevelMeter (bool v, int n)
        : vertical (v), numSegments (juce::jmax (4, n))
    {
        setOpaque (false);
        segmentDb.resize ((size_t) numSegments);
        for (int i = 0; i < numSegments; ++i)
            segmentDb[(size_t) i] = positionToDb ((float) (i + 1) / (float) numSegments);
    }

    juce::Colour LevelMeter::segmentColour (float dbTop) const noexcept
    {
        if (dbTop <= -12.0f) return meterGreen;
        if (dbTop <= -6.0f)  return meterYellow;
        if (dbTop <= -3.0f)  return meterOrange;
        return meterRed;
    }

    void LevelMeter::push (const engine::MeterSource::Reading& r, float dt)
    {
        ballistics.update (r.peak, r.rms, r.clipped, dt);

        // Índice del último segmento encendido para cada capa: un segmento se
        // enciende cuando el nivel supera su borde inferior.
        auto lastLit = [this] (float db)
        {
            int k = -1;
            for (int i = 0; i < numSegments; ++i)
            {
                const auto bottom = i == 0 ? engine::MeterBallistics::kFloorDb + 0.5f : segmentDb[(size_t) i - 1];
                if (db > bottom)
                    k = i;
            }
            return k;
        };

        const auto rms  = lastLit (ballistics.getRmsDb());
        const auto peak = lastLit (ballistics.getPeakDb());
        const auto hold = lastLit (ballistics.getHoldDb());
        const auto clip = ballistics.isClipLit();

        // Solo se repinta si cambió algún LED.
        if (rms != litRms || peak != litPeak || hold != litHold || clip != clipLit)
        {
            litRms = rms; litPeak = peak; litHold = hold; clipLit = clip;
            repaint();
        }
    }

    void LevelMeter::resized() {}

    void LevelMeter::mouseDown (const juce::MouseEvent&)
    {
        ballistics.resetClip();
        clipLit = false;
        repaint();
    }

    void LevelMeter::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (slot);
        g.fillRoundedRectangle (b, 2.0f);

        const auto pad = juce::jmax (1.0f, juce::jmin (b.getWidth(), b.getHeight()) * 0.12f);
        auto inner = b.reduced (pad);

        // LED de clip en la punta (arriba o a la derecha).
        const auto clipSize = vertical ? inner.getWidth() * 0.9f : inner.getHeight() * 1.6f;
        auto clipRect = vertical ? inner.removeFromTop (clipSize) : inner.removeFromRight (clipSize);
        if (vertical) inner.removeFromTop (pad); else inner.removeFromRight (pad);

        const auto len = vertical ? inner.getHeight() : inner.getWidth();
        const auto seg = len / (float) numSegments;
        const auto gap = juce::jmax (1.0f, seg * 0.22f);

        for (int i = 0; i < numSegments; ++i)
        {
            juce::Rectangle<float> r = vertical
                ? juce::Rectangle<float> (inner.getX(), inner.getBottom() - seg * (float) (i + 1) + gap * 0.5f, inner.getWidth(), seg - gap)
                : juce::Rectangle<float> (inner.getX() + seg * (float) i + gap * 0.5f, inner.getY(), seg - gap, inner.getHeight());

            const auto c = segmentColour (segmentDb[(size_t) i]);
            float alpha = 0.13f;                          // apagado
            if (i <= litPeak) alpha = 0.55f;               // entre RMS y pico
            if (i <= litRms || i == litHold) alpha = 1.0f; // RMS y retención

            if (alpha >= 0.99f)
            {
                g.setColour (c.withAlpha (0.25f));        // glow
                g.fillRoundedRectangle (r.expanded (gap * 0.9f), gap);
            }
            g.setColour (c.withAlpha (alpha));
            g.fillRect (r);
        }

        g.setColour (clipLit ? meterRed : meterRed.withAlpha (0.15f));
        if (clipLit)
        {
            g.setColour (meterRed.withAlpha (0.35f));
            g.fillRoundedRectangle (clipRect.expanded (pad), pad * 2.0f);
            g.setColour (meterRed);
        }
        g.fillRoundedRectangle (clipRect, 1.5f);
    }
} // namespace kb::ui
