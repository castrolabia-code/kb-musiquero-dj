#include "ui/DeckDisplay.h"

#include "library/ImageResize.h"

#include "analysis/KeyDetector.h"
#include "app/I18n.h"
#include "ui/DrawPrimitives.h"
#include "ui/KbLookAndFeel.h"
#include "ui/Palette.h"

#include <cmath>

namespace kb::ui
{
    using juce::Justification;
    using juce::Rectangle;
    using namespace palette;
    using analysis::MusicalKey;
    using engine::TrackBuffer;

    DeckDisplay::DeckDisplay (int d) : deck (d)
    {
        setOpaque (true);
    }

    // =========================================================================
    //  Formatos
    // =========================================================================
    juce::String DeckDisplay::formatTime (double seconds, bool negative)
    {
        seconds = std::max (0.0, seconds);
        const auto tenths = (int) std::floor (seconds * 10.0 + 1.0e-6);
        return juce::String (negative ? "-" : "") + juce::String (tenths / 600).paddedLeft ('0', 2) + ":"
               + juce::String ((tenths / 10) % 60).paddedLeft ('0', 2) + "." + juce::String (tenths % 10);
    }

    juce::String DeckDisplay::formatBpm (double bpm) { return juce::String (bpm, 2); }

    int DeckDisplay::effectiveShift (float tempoPercent, bool keyLock, int keyShift)
    {
        const auto fromTempo = keyLock ? 0 : (int) std::lround (12.0 * std::log2 (1.0 + tempoPercent / 100.0));
        return keyShift + fromTempo;
    }

    juce::Colour DeckDisplay::bandColour (int band) const
    {
        const auto c = deckColour (deck);
        switch (band)
        {
            case TrackBuffer::low:  return c.darker (0.55f);
            case TrackBuffer::mid:  return c;
            default:                return c.interpolatedWith (juce::Colours::white, 0.75f);
        }
    }

    // =========================================================================
    //  Layout
    // =========================================================================
    // Como la referencia: info del deck arriba, vista general de la canción y la
    // waveform ampliada (grande) abajo. Cada deck ocupa media pantalla.
    Rectangle<float> DeckDisplay::infoArea() const
    {
        auto b = getLocalBounds().toFloat();
        return b.withHeight (juce::jmax (36.0f, b.getHeight() * 0.27f));
    }

    Rectangle<float> DeckDisplay::overviewArea() const
    {
        auto b = getLocalBounds().toFloat();
        b.removeFromTop (infoArea().getHeight() + 2.0f);
        return b.withHeight (b.getHeight() * 0.24f);
    }

    Rectangle<float> DeckDisplay::detailArea() const
    {
        auto b = getLocalBounds().toFloat();
        b.removeFromTop (infoArea().getHeight() + 2.0f);
        b.removeFromTop (b.getHeight() * 0.24f + 3.0f);
        return b;
    }

    void DeckDisplay::resized()
    {
        detailImage = {};
        overviewImage = {};
    }

    // =========================================================================
    //  Actualización (60 fps)
    // =========================================================================
    void DeckDisplay::update (const View& v)
    {
        view = v;

        // "Firma" de lo visible: si no cambió, no se repinta.
        const auto* t = v.track.get();
        const auto pxPerSecond = detailArea().getWidth() / zoomSeconds;
        juce::String sig;
        sig << (t != nullptr ? (juce::int64) t->getId() : 0) << "|" << (int) v.loading << (int) v.analysing
            << (int) v.keyLock << "|" << v.keyShift << "|" << juce::String (v.tempo, 2)
            << "|" << (int) std::floor (v.status.positionSeconds * pxPerSecond)
            << "|" << (int) (v.status.positionSeconds * 10.0) << "|" << juce::String (v.status.cueSeconds, 3)
            << "|" << (t != nullptr ? juce::String (t->getBpm(), 3) + juce::String (t->getKey())
                                      + juce::String (t->getDetailBinsReady()) : juce::String())
            << "|" << (int) zoomSeconds << "|" << (int) v.artwork.isValid() << (juce::pointer_sized_int) v.artwork.getPixelData().get() << v.title << "|" << (int) v.status.loopActive << juce::String (v.status.loopInSeconds, 3)
            << juce::String (v.status.loopOutSeconds, 3) << (int) v.status.slipping << (int) (v.status.slipSeconds * 20.0);
        for (auto c : v.status.hotCues)
            sig << "," << juce::String (c, 2);
        if (sig != lastSignature)
        {
            lastSignature = sig;
            repaint();
        }
    }

    void DeckDisplay::setDropHighlight (bool h)
    {
        if (dropHighlight != h)
        {
            dropHighlight = h;
            repaint();
        }
    }

    void DeckDisplay::refresh() { lastSignature.clear(); repaint(); }

    juce::String DeckDisplay::getTooltip()
    {
        const auto p = getMouseXYRelative().toFloat();
        if (detailArea().contains (p))   return tr ("tip.waveformDetail");
        if (overviewArea().contains (p)) return tr ("tip.waveform");
        return tr ("tip.deckInfo");
    }

    // =========================================================================
    //  Pintado
    // =========================================================================
    void DeckDisplay::paint (juce::Graphics& g)
    {
        g.fillAll (panelDark);
        const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();

        paintInfo (g, infoArea());
        paintDetail (g, detailArea(), scale);
        paintOverview (g, overviewArea(), scale);

        if (dropHighlight)
        {
            g.setColour (cyan.withAlpha (0.15f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
            g.setColour (cyan);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 2.0f);
            draw::text (g, tr ("wave.dropHere", { { "n", juce::String (deck + 1) } }), detailArea(),
                        juce::jmin (18.0f * KbLookAndFeel::uiScale(), detailArea().getHeight() * 0.35f), cyan);
        }
    }

    void DeckDisplay::paintInfo (juce::Graphics& g, Rectangle<float> info)
    {
        // Una fila: [carátula] Título / artista ........ BPM · KEY · tempo · tiempo
        const auto colour = deckColour (deck);
        const auto h = info.getHeight();
        info = info.reduced (h * 0.04f, h * 0.06f);

        auto art = info.removeFromLeft (info.getHeight());
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.55f), art.getX(), art.getY(),
                                                 panelEdge, art.getRight(), art.getBottom(), false));
        g.fillRoundedRectangle (art, 3.0f);
        if (view.artwork.isValid())
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (art, 3.0f);
            g.reduceClipRegion (clip);
            g.setOpacity (1.0f);
            // Achicada a los píxeles reales de la pantalla (nítida, sin dientes).
            const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
            const auto thumb = artThumb.get (view.artwork, juce::roundToInt (art.getWidth() * scale));
            g.drawImage (thumb, art);
        }
        else
        {
            draw::text (g, juce::String (deck + 1), art, art.getHeight() * 0.42f, textPrimary);
        }
        info.removeFromLeft (h * 0.15f);

        const auto H = info.getHeight();
        const auto* t = view.track.get();
        const auto len = t != nullptr ? t->getLengthSeconds() : 0.0;

        // Derecha: BPM grande + tonalidad / tiempos + tempo.
        auto right = info.removeFromRight (juce::jmin (info.getWidth() * 0.55f, H * 7.4f));
        auto bpmCol = right.removeFromLeft (right.getWidth() * 0.44f);
        juce::String bpmText = "--";
        if (t != nullptr && t->getBpm() > 0.0)
            bpmText = formatBpm (t->getBpm() * (1.0 + view.tempo / 100.0));
        else if (view.analysing)
            bpmText = "...";
        auto bpmRow = bpmCol.removeFromTop (H * 0.58f);
        draw::text (g, tr ("wave.bpm"), bpmRow.removeFromLeft (H * 0.8f), H * 0.26f, colour, Justification::centredLeft);
        draw::text (g, bpmText, bpmRow, H * 0.4f, textPrimary, Justification::centredLeft);

        juce::String keyText = view.analysing ? juce::String ("...") : juce::String ("--");
        if (t != nullptr && t->getKey() >= 0)
        {
            const auto shift = effectiveShift (view.tempo, view.keyLock, view.keyShift);
            const auto k = MusicalKey::transpose (t->getKey(), shift);
            keyText = juce::String (MusicalKey::camelot (k)) + " " + juce::String (MusicalKey::name (k));
            if (shift != 0)
                keyText << " (" << (shift > 0 ? "+" : "") << shift << ")";
        }
        draw::text (g, tr ("wave.key"), bpmCol.removeFromLeft (H * 0.8f), H * 0.24f, colour, Justification::centredLeft);
        draw::text (g, keyText, bpmCol, H * 0.27f, textPrimary, Justification::centredLeft);

        auto timeRow = right.removeFromTop (H * 0.58f);
        draw::text (g, formatTime (view.status.positionSeconds), timeRow, H * 0.33f, textPrimary, Justification::centredRight);
        auto tempoRow = right;
        draw::text (g, formatTime (len - view.status.positionSeconds, true), tempoRow.removeFromRight (tempoRow.getWidth() * 0.5f),
                    H * 0.27f, colour, Justification::centredRight);
        if (view.keyLock)
        {
            const auto badge = tempoRow.removeFromRight (H * 0.85f).reduced (H * 0.04f, H * 0.06f);
            g.setColour (orange);
            g.fillRoundedRectangle (badge, 3.0f);
            draw::text (g, tr ("deck.keyLockShort"), badge, badge.getHeight() * 0.5f, panelDark);
        }
        draw::text (g, (view.tempo >= 0.0f ? "+" : "") + juce::String (view.tempo, 2) + "%", tempoRow,
                    H * 0.24f, textSecondary, Justification::centredRight);

        // Izquierda: título y artista.
        info.removeFromRight (H * 0.2f);
        auto titleRow = info.removeFromTop (H * 0.58f);
        if (t != nullptr)
        {
            draw::text (g, view.title.isNotEmpty() ? view.title : t->getInfo().title, titleRow, H * 0.36f, textPrimary, Justification::centredLeft, true);
            draw::text (g, view.title.isNotEmpty() ? view.artist : t->getInfo().artist, info, H * 0.27f, textSecondary, Justification::centredLeft, false);
        }
        else
        {
            draw::text (g, tr (view.loading ? "wave.loadingShort" : "wave.empty"), titleRow, H * 0.32f,
                        textSecondary, Justification::centredLeft, false);
        }
    }

    // =========================================================================
    //  Waveform ampliada
    // =========================================================================
    void DeckDisplay::renderDetailCache (Rectangle<float> area, float scale, double startSeconds)
    {
        const auto* t = view.track.get();
        const auto viewW = juce::jmax (1, juce::roundToInt (area.getWidth() * scale));
        const auto w = viewW * 3;
        const auto h = juce::jmax (1, juce::roundToInt (area.getHeight() * scale));

        detailImage = juce::Image (juce::Image::ARGB, w, h, true);
        detailStart = startSeconds;
        detailZoom  = zoomSeconds;
        detailScale = scale;
        detailWidth = viewW;
        detailTrack = t != nullptr ? t->getId() : 0;
        detailBins  = t != nullptr ? t->getDetailBinsReady() : -1;

        if (t == nullptr)
            return;

        juce::Graphics g (detailImage);
        const auto mid = (float) h * 0.5f;
        const auto secondsPerPx = (zoomSeconds * 3.0) / (double) w;
        const auto binsPerSecond = (double) TrackBuffer::kDetailBinsPerSecond;
        const auto ready = detailBins;
        const float gains[TrackBuffer::numBands] = { 1.6f, 2.4f, 3.2f };

        for (int x = 0; x < w; ++x)
        {
            const auto t0 = startSeconds + x * secondsPerPx;
            if (t0 < 0.0)
                continue;
            const auto b0 = (int) (t0 * binsPerSecond);
            const auto b1 = juce::jmax (b0 + 1, (int) ((t0 + secondsPerPx) * binsPerSecond));
            if (b0 >= ready)
                break;

            for (int band = 0; band < TrackBuffer::numBands; ++band)
            {
                float peak = 0.0f;
                for (int b = b0; b < juce::jmin (b1, ready); ++b)
                    peak = juce::jmax (peak, t->getDetailBand (band, b));
                const auto barH = juce::jlimit (0.0f, mid, peak * gains[band] * mid);
                if (barH < 0.5f)
                    continue;
                g.setColour (bandColour (band).withAlpha (band == TrackBuffer::high ? 0.85f : 1.0f));
                g.fillRect ((float) x, mid - barH, 1.0f, barH * 2.0f);
            }
        }
    }

    void DeckDisplay::paintDetail (juce::Graphics& g, Rectangle<float> area, float scale)
    {
        g.setColour (slot);
        g.fillRoundedRectangle (area, 4.0f);

        const auto* t = view.track.get();
        if (t == nullptr)
        {
           #if JUCE_ANDROID || JUCE_IOS
            const char* hint = "wave.emptyHintTouch";   // pantallas táctiles: no hay "arrastrar"
           #else
            const char* hint = "wave.emptyHint";
           #endif
            draw::text (g, tr (view.loading ? "wave.loadingShort" : hint), area,
                        juce::jmin (16.0f * KbLookAndFeel::uiScale(), area.getHeight() * 0.3f), textSecondary.withAlpha (0.8f), Justification::centred, false);
            return;
        }

        // La ventana muestra "zoomSeconds" de tiempo REAL: con más tempo entra más
        // pista. Así, con SYNC, las grillas de los dos decks quedan alineadas.
        const auto rate = juce::jlimit (0.5, 1.5, 1.0 + view.tempo / 100.0);
        const auto span = zoomSeconds * rate;          // segundos de pista visibles
        const auto pos = view.status.positionSeconds;
        const auto windowStart = pos - span * 0.5;
        const auto viewW = juce::jmax (1, juce::roundToInt (area.getWidth() * scale));

        // La caché cubre 3 x zoom segundos de pista (alcanza hasta +50 % de tempo).
        const auto needsRender = detailImage.isNull() || detailTrack != t->getId()
                                 || ! juce::exactlyEqual (detailScale, scale) || detailWidth != viewW
                                 || ! juce::exactlyEqual (detailZoom, zoomSeconds)
                                 || detailBins != t->getDetailBinsReady()
                                 || windowStart < detailStart || windowStart + span > detailStart + 3.0 * zoomSeconds;
        if (needsRender)
            renderDetailCache (area, scale, pos - zoomSeconds * 1.5);

        // Copiar (y escalar según el tempo) la porción visible de la imagen cacheada.
        const auto pxPerTrackSecond = detailImage.getWidth() / (3.0 * zoomSeconds);
        const auto srcX = (int) std::lround ((windowStart - detailStart) * pxPerTrackSecond);
        const auto srcW = juce::jmax (1, (int) std::lround (span * pxPerTrackSecond));
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (area.toNearestInt());
            const auto dest = area.toNearestInt();
            g.setOpacity (1.0f);
            g.drawImage (detailImage, dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(),
                         srcX, 0, srcW, detailImage.getHeight());
        }

        const auto pxPerSecond = area.getWidth() / (float) span;
        auto xFor = [&] (double seconds) { return area.getX() + (float) ((seconds - windowStart) * pxPerSecond); };

        // Grilla de beats.
        if (t->getBpm() > 0.0)
        {
            const auto beat = 60.0 / t->getBpm();
            const auto first = t->getFirstBeatSeconds();
            const auto k0 = (long) std::ceil ((windowStart - first) / beat);
            const auto k1 = (long) std::floor ((windowStart + span - first) / beat);
            for (auto k = k0; k <= k1; ++k)
            {
                const auto x = xFor (first + (double) k * beat);
                const auto bar = (k % 4) == 0;
                g.setColour (juce::Colours::white.withAlpha (bar ? 0.45f : 0.18f));
                g.fillRect (x - 0.5f, area.getY(), 1.0f, area.getHeight() * (bar ? 1.0f : 0.25f));
                if (! bar)
                    g.fillRect (x - 0.5f, area.getBottom() - area.getHeight() * 0.25f, 1.0f, area.getHeight() * 0.25f);
            }
        }

        // Pasado (a la izquierda del playhead) atenuado.
        g.setColour (panelDark.withAlpha (0.35f));
        g.fillRect (Rectangle<float>::leftTopRightBottom (area.getX(), area.getY(), area.getCentreX(), area.getBottom()));

        // Loop: zona sombreada (verde si está activo, gris si solo está guardado).
        const auto& st = view.status;
        if (st.loopInSeconds >= 0.0 && st.loopOutSeconds > st.loopInSeconds)
        {
            const auto x0 = juce::jmax (area.getX(), xFor (st.loopInSeconds));
            const auto x1 = juce::jmin (area.getRight(), xFor (st.loopOutSeconds));
            if (x1 > x0)
            {
                const auto c = st.loopActive ? play : textSecondary;
                g.setColour (c.withAlpha (st.loopActive ? 0.22f : 0.10f));
                g.fillRect (Rectangle<float>::leftTopRightBottom (x0, area.getY(), x1, area.getBottom()));
                g.setColour (c.withAlpha (0.9f));
                for (auto x : { xFor (st.loopInSeconds), xFor (st.loopOutSeconds) })
                    if (x >= area.getX() && x <= area.getRight())
                        g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight());
            }
        }
        else if (st.loopInSeconds >= 0.0)
        {
            const auto x = xFor (st.loopInSeconds);   // IN marcado, esperando OUT
            if (x >= area.getX() && x <= area.getRight())
            {
                g.setColour (play);
                g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight());
            }
        }

        // Hot cues: línea y etiqueta con su número, en su color.
        for (int i = 0; i < 8; ++i)
        {
            const auto sec = st.hotCues[(size_t) i];
            if (sec < 0.0)
                continue;
            const auto x = xFor (sec);
            if (x < area.getX() - 20.0f || x > area.getRight())
                continue;
            const auto c = palette::hotCueColour (i);
            g.setColour (c);
            g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight());
            const auto tagH = juce::jmax (12.0f, area.getHeight() * 0.22f);
            const Rectangle<float> tag (x, area.getBottom() - tagH, tagH * 0.95f, tagH);
            g.fillRoundedRectangle (tag, 2.0f);
            draw::text (g, juce::String (i + 1), tag, tagH * 0.8f, panelDark);
        }

        // Slip: dónde estaría la canción (línea punteada).
        if (st.slipping)
        {
            const auto x = xFor (st.slipSeconds);
            if (x >= area.getX() && x <= area.getRight())
            {
                g.setColour (cyan.withAlpha (0.9f));
                for (auto y = area.getY(); y < area.getBottom(); y += 6.0f)
                    g.fillRect (x - 1.0f, y, 2.0f, 3.0f);
            }
        }

        // Cue.
        const auto xCue = xFor (view.status.cueSeconds);
        if (xCue >= area.getX() && xCue <= area.getRight())
        {
            g.setColour (orange);
            g.fillRect (xCue - 1.0f, area.getY(), 2.0f, area.getHeight());
            juce::Path tri;
            const auto s = juce::jmax (5.0f, area.getHeight() * 0.12f);
            tri.addTriangle (xCue - s, area.getY(), xCue + s, area.getY(), xCue, area.getY() + s * 1.2f);
            g.fillPath (tri);
        }

        // Playhead al centro.
        g.setColour (textPrimary);
        g.fillRect (Rectangle<float> (2.0f, area.getHeight()).withCentre (area.getCentre()));

        if (! t->isComplete())
            draw::text (g, tr ("wave.loading", { { "percent", juce::String ((int) (t->getLoadProgress() * 100.0f)) } }),
                        area.reduced (8.0f, 4.0f), juce::jmin (14.0f * KbLookAndFeel::uiScale(), area.getHeight() * 0.25f), textSecondary,
                        Justification::topRight, false);
    }

    // =========================================================================
    //  Vista general
    // =========================================================================
    void DeckDisplay::renderOverviewCache (Rectangle<float> area, float scale)
    {
        const auto* t = view.track.get();
        const auto w = juce::jmax (1, juce::roundToInt (area.getWidth() * scale));
        const auto h = juce::jmax (1, juce::roundToInt (area.getHeight() * scale));
        overviewImage = juce::Image (juce::Image::ARGB, w, h, true);
        overviewScale = scale;
        overviewWidth = w;
        overviewTrack = t != nullptr ? t->getId() : 0;
        overviewBins  = t != nullptr ? t->getOverviewBinsReady() : -1;
        if (t == nullptr)
            return;

        juce::Graphics g (overviewImage);
        const auto mid = (float) h * 0.5f;
        const auto lengthFrames = (double) t->getLengthFrames();
        const auto framesPerBin = t->getFramesPerOverviewBin();
        const auto ready = overviewBins;
        const float gains[TrackBuffer::numBands] = { 1.6f, 2.4f, 3.2f };
        if (lengthFrames <= 0.0)
            return;

        for (int x = 0; x < w; ++x)
        {
            const auto b0 = (int) (lengthFrames * x / w / framesPerBin);
            const auto b1 = juce::jmax (b0 + 1, (int) std::ceil (lengthFrames * (x + 1) / w / framesPerBin));
            if (b0 >= ready)
                break;
            for (int band = 0; band < TrackBuffer::numBands; ++band)
            {
                float peak = 0.0f;
                for (int b = b0; b < juce::jmin (b1, ready); ++b)
                    peak = juce::jmax (peak, t->getOverviewBand (band, b));
                const auto barH = juce::jlimit (0.0f, mid, peak * gains[band] * mid);
                if (barH < 0.5f)
                    continue;
                g.setColour (bandColour (band));
                g.fillRect ((float) x, mid - barH, 1.0f, barH * 2.0f);
            }
        }
    }

    void DeckDisplay::paintOverview (juce::Graphics& g, Rectangle<float> area, float scale)
    {
        g.setColour (slot);
        g.fillRoundedRectangle (area, 3.0f);

        const auto* t = view.track.get();
        if (t == nullptr)
            return;

        const auto w = juce::jmax (1, juce::roundToInt (area.getWidth() * scale));
        if (overviewImage.isNull() || overviewTrack != t->getId() || overviewBins != t->getOverviewBinsReady()
            || ! juce::exactlyEqual (overviewScale, scale) || overviewWidth != w)
            renderOverviewCache (area, scale);
        g.setOpacity (1.0f);
        g.drawImage (overviewImage, area, juce::RectanglePlacement::stretchToFit);

        const auto len = t->getLengthSeconds();
        if (len <= 0.0)
            return;
        const auto xPlay = area.getX() + area.getWidth() * (float) juce::jlimit (0.0, 1.0, view.status.positionSeconds / len);
        const auto xCue  = area.getX() + area.getWidth() * (float) juce::jlimit (0.0, 1.0, view.status.cueSeconds / len);

        g.setColour (panelDark.withAlpha (0.55f));
        g.fillRect (Rectangle<float>::leftTopRightBottom (area.getX(), area.getY(), xPlay, area.getBottom()));

        // Ventana de la vista ampliada.
        const auto zx = area.getWidth() * (float) (zoomSeconds * (1.0 + view.tempo / 100.0) / len);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRect (Rectangle<float> (zx, area.getHeight()).withCentre ({ xPlay, area.getCentreY() }).getIntersection (area));

        auto xOf = [&] (double sec) { return area.getX() + area.getWidth() * (float) juce::jlimit (0.0, 1.0, sec / len); };
        const auto& st = view.status;
        if (st.loopInSeconds >= 0.0 && st.loopOutSeconds > st.loopInSeconds)
        {
            g.setColour ((st.loopActive ? play : textSecondary).withAlpha (0.45f));
            g.fillRect (Rectangle<float>::leftTopRightBottom (xOf (st.loopInSeconds), area.getY(),
                                                              juce::jmax (xOf (st.loopInSeconds) + 2.0f, xOf (st.loopOutSeconds)), area.getBottom()));
        }
        for (int i = 0; i < 8; ++i)
            if (st.hotCues[(size_t) i] >= 0.0)
            {
                g.setColour (palette::hotCueColour (i));
                const auto x = xOf (st.hotCues[(size_t) i]);
                g.fillRect (x - 1.0f, area.getY(), 2.0f, area.getHeight());
                g.fillRect (x - 1.0f, area.getY(), 5.0f, juce::jmax (3.0f, area.getHeight() * 0.2f));
            }

        g.setColour (orange);
        g.fillRect (xCue - 1.0f, area.getY(), 2.0f, area.getHeight());
        g.setColour (textPrimary);
        g.fillRect (Rectangle<float> (2.0f, area.getHeight()).withCentre ({ xPlay, area.getCentreY() }));
    }

    // =========================================================================
    //  Mouse
    // =========================================================================
    void DeckDisplay::mouseDown (const juce::MouseEvent& e) { seekFromMouse (e); }
    void DeckDisplay::mouseDrag (const juce::MouseEvent& e) { seekFromMouse (e); }

    void DeckDisplay::seekFromMouse (const juce::MouseEvent& e)
    {
        const auto area = overviewArea();
        if (view.track == nullptr || ! onSeek || ! area.contains (e.mouseDownPosition))
            return;
        const auto fraction = juce::jlimit (0.0f, 1.0f, (e.position.x - area.getX()) / area.getWidth());
        onSeek (fraction * view.track->getLengthSeconds());
    }

    void DeckDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (! detailArea().contains (e.position))
            return;
        // Rueda hacia arriba = acercar (menos segundos en pantalla).
        const auto factor = wheel.deltaY > 0.0f ? 0.8 : 1.25;
        zoomSeconds = juce::jlimit (2.0, 32.0, zoomSeconds * factor);
        refresh();
    }
} // namespace kb::ui
