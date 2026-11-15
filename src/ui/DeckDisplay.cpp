#include "ui/DeckDisplay.h"

#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/Palette.h"

#include <cmath>

namespace kb::ui
{
    using juce::Justification;
    using juce::Rectangle;
    using namespace palette;

    DeckDisplay::DeckDisplay (int d) : deck (d)
    {
        setOpaque (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    juce::String DeckDisplay::formatTime (double seconds, bool negative)
    {
        seconds = std::max (0.0, seconds);
        const auto tenths = (int) std::floor (seconds * 10.0 + 1.0e-6);
        const auto mins = tenths / 600;
        const auto secs = (tenths / 10) % 60;
        const auto t    = tenths % 10;
        return juce::String (negative ? "-" : "") + juce::String (mins).paddedLeft ('0', 2) + ":"
               + juce::String (secs).paddedLeft ('0', 2) + "." + juce::String (t);
    }

    void DeckDisplay::update (std::shared_ptr<engine::TrackBuffer> t, const engine::Deck::Status& st, bool isLoading)
    {
        bool changed = t != track || isLoading != loading || st.playing != status.playing
                       || std::abs (st.cueSeconds - status.cueSeconds) > 1.0e-3;

        track   = std::move (t);
        status  = st;
        loading = isLoading;

        // Solo cuenta como cambio lo que se ve: décimas de segundo, un píxel de
        // playhead o un 1 % más de carga.
        const auto tenths = (int) std::floor (st.positionSeconds * 10.0);
        const auto len = track != nullptr ? track->getLengthSeconds() : 0.0;
        const auto px  = len > 0.0 ? (int) (waveArea().getWidth() * st.positionSeconds / len) : -1;
        const auto progress = track != nullptr ? std::floor (track->getLoadProgress() * 100.0f) : -1.0f;

        changed = changed || tenths != lastTenths || px != lastPlayheadPx || ! juce::exactlyEqual (progress, lastProgress);
        lastTenths = tenths;
        lastPlayheadPx = px;
        lastProgress = progress;

        if (changed)
            repaint();
    }

    void DeckDisplay::setDropHighlight (bool h)
    {
        if (dropHighlight != h)
        {
            dropHighlight = h;
            repaint();
        }
    }

    void DeckDisplay::refresh()
    {
        repaint();
    }

    void DeckDisplay::resized()
    {
        overviewScale = 0.0f;   // fuerza regenerar la vista con el nuevo tamaño
    }

    Rectangle<float> DeckDisplay::waveArea() const
    {
        auto row = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
        const auto h = row.getHeight();
        row.removeFromLeft (juce::jmax (h * 4.2f, row.getWidth() * 0.22f) + 4.0f);
        return row;
    }

    // =========================================================================
    //  Vista general cacheada en una imagen (a la resolución física)
    // =========================================================================
    void DeckDisplay::renderOverview (float scale)
    {
        const auto area = waveArea();
        const auto w = juce::jmax (1, juce::roundToInt (area.getWidth() * scale));
        const auto h = juce::jmax (1, juce::roundToInt (area.getHeight() * scale));

        overview = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics g (overview);

        const auto colour = deckColour (deck);
        const auto mid = (float) h * 0.5f;
        g.setColour (colour.withAlpha (0.25f));
        g.fillRect (0.0f, mid - 0.5f * scale, (float) w, scale);

        if (track == nullptr)
            return;

        const auto lengthFrames = (double) track->getLengthFrames();
        const auto binsReady = track->getOverviewBinsReady();
        const auto framesPerBin = track->getFramesPerOverviewBin();
        if (lengthFrames <= 0.0)
            return;

        // Cada columna de píxeles toma el pico de los bins que cubre.
        for (int x = 0; x < w; ++x)
        {
            const auto f0 = lengthFrames * x / w;
            const auto f1 = lengthFrames * (x + 1) / w;
            const auto b0 = (int) (f0 / framesPerBin);
            const auto b1 = juce::jmax (b0 + 1, (int) std::ceil (f1 / framesPerBin));
            if (b0 >= binsReady)
                break;

            float peak = 0.0f;
            for (int b = b0; b < juce::jmin (b1, binsReady); ++b)
                peak = juce::jmax (peak, track->getOverviewPeak (b));

            const auto barH = juce::jlimit (0.0f, mid, peak * mid * 0.95f);
            g.setColour (colour.withAlpha (0.55f + 0.45f * peak));
            g.fillRect ((float) x, mid - barH, 1.0f, barH * 2.0f + 0.5f);
        }

        overviewTrackId = track->getId();
        overviewBins    = binsReady;
        overviewScale   = scale;
    }

    // =========================================================================
    //  Pintado
    // =========================================================================
    void DeckDisplay::paint (juce::Graphics& g)
    {
        g.fillAll (panelDark);

        auto row = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
        const auto colour = deckColour (deck);
        const auto h = row.getHeight();

        // --- Info del deck ------------------------------------------------------------
        auto info = row.removeFromLeft (juce::jmax (h * 4.2f, row.getWidth() * 0.22f));
        g.setColour (panel);
        g.fillRoundedRectangle (info, 4.0f);
        info = info.reduced (h * 0.1f);

        auto art = info.removeFromLeft (info.getHeight());
        g.setColour (panelEdge);
        g.fillRoundedRectangle (art, 3.0f);
        draw::text (g, juce::String (deck + 1), art, art.getHeight() * 0.5f, colour);   // carátula: Fase 6
        info.removeFromLeft (h * 0.12f);

        const auto lineH = info.getHeight() / 4.0f;
        const auto len = track != nullptr ? track->getLengthSeconds() : 0.0;

        if (track != nullptr)
        {
            const auto& ti = track->getInfo();
            draw::text (g, ti.title, info.removeFromTop (lineH), lineH * 0.78f, textPrimary, Justification::centredLeft, true);
            draw::text (g, ti.artist, info.removeFromTop (lineH), lineH * 0.66f, textSecondary, Justification::centredLeft, false);
        }
        else
        {
            draw::text (g, tr (loading ? "wave.loadingShort" : "wave.empty"), info.removeFromTop (lineH), lineH * 0.7f,
                        textPrimary, Justification::centredLeft, false);
            info.removeFromTop (lineH);
        }

        // BPM / KEY llegan en la Fase 3.
        draw::text (g, tr ("wave.bpm") + " --.-    " + tr ("wave.key") + " --    +0.00%",
                    info.removeFromTop (lineH), lineH * 0.6f, colour, Justification::centredLeft);

        const auto timeLine = info.removeFromTop (lineH);
        draw::text (g, formatTime (status.positionSeconds) + "   " + formatTime (len - status.positionSeconds, true),
                    timeLine, lineH * 0.72f, textPrimary, Justification::centredLeft);

        // --- Vista de la pista ------------------------------------------------------------
        row.removeFromLeft (4.0f);
        const auto area = row;
        g.setColour (slot);
        g.fillRoundedRectangle (area, 4.0f);

        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const auto needsRender = overview.isNull() || ! juce::exactlyEqual (scale, overviewScale)
                                 || (track != nullptr && (track->getId() != overviewTrackId || track->getOverviewBinsReady() != overviewBins))
                                 || (track == nullptr && overviewTrackId != 0);
        if (needsRender)
        {
            renderOverview (scale);
            if (track == nullptr)
                overviewTrackId = 0;
        }
        g.drawImage (overview, area, juce::RectanglePlacement::stretchToFit);

        if (track != nullptr && len > 0.0)
        {
            const auto xPlay = area.getX() + area.getWidth() * (float) juce::jlimit (0.0, 1.0, status.positionSeconds / len);
            const auto xCue  = area.getX() + area.getWidth() * (float) juce::jlimit (0.0, 1.0, status.cueSeconds / len);

            // Parte ya reproducida, atenuada.
            g.setColour (panelDark.withAlpha (0.55f));
            g.fillRect (Rectangle<float>::leftTopRightBottom (area.getX(), area.getY(), xPlay, area.getBottom()));

            // Marca de cue (triángulo naranja arriba).
            juce::Path tri;
            const auto s = juce::jmax (5.0f, area.getHeight() * 0.12f);
            tri.addTriangle (xCue - s, area.getY(), xCue + s, area.getY(), xCue, area.getY() + s * 1.2f);
            g.setColour (orange);
            g.fillPath (tri);

            // Playhead
            g.setColour (textPrimary);
            g.fillRect (Rectangle<float> (2.0f, area.getHeight()).withCentre ({ xPlay, area.getCentreY() }));

            if (! track->isComplete())
                draw::text (g, tr ("wave.loading", { { "percent", juce::String ((int) (track->getLoadProgress() * 100.0f)) } }),
                            area.reduced (8.0f, 4.0f), juce::jmin (14.0f, area.getHeight() * 0.22f), textSecondary,
                            Justification::topRight, false);
        }
        else
        {
            draw::text (g, tr (loading ? "wave.loadingShort" : "wave.emptyHint"), area, juce::jmin (15.0f, area.getHeight() * 0.25f),
                        textSecondary.withAlpha (0.8f), Justification::centred, false);
        }

        if (dropHighlight)
        {
            g.setColour (cyan.withAlpha (0.15f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
            g.setColour (cyan);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 2.0f);
            draw::text (g, tr ("wave.dropHere", { { "n", juce::String (deck + 1) } }), area,
                        juce::jmin (18.0f, area.getHeight() * 0.3f), cyan);
        }
    }

    // =========================================================================
    //  Click para saltar
    // =========================================================================
    void DeckDisplay::mouseDown (const juce::MouseEvent& e) { seekFromMouse (e); }
    void DeckDisplay::mouseDrag (const juce::MouseEvent& e) { seekFromMouse (e); }

    void DeckDisplay::seekFromMouse (const juce::MouseEvent& e)
    {
        const auto area = waveArea();
        if (track == nullptr || ! onSeek || e.mouseDownPosition.x < area.getX())
            return;

        const auto fraction = juce::jlimit (0.0f, 1.0f, (e.position.x - area.getX()) / area.getWidth());
        onSeek (fraction * track->getLengthSeconds());
    }
} // namespace kb::ui
