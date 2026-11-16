#pragma once
// =============================================================================
//  ImageResize: achicado de imágenes SIN pixelado (carátulas).
//
//  El renderizador de JUCE interpola de a 2x2 píxeles (bilineal): al achicar
//  más de la mitad de una sola vez se saltea información y la imagen queda
//  "dentada" (aliasing), sobre todo si después gira. Acá se achica en pasos de
//  la mitad (cada paso promedia bien) y recién el último paso va al tamaño
//  exacto. Así el resultado es suave como en un editor de fotos.
//  Solo usa imágenes "software": se puede llamar desde cualquier hilo.
// =============================================================================
#include <juce_graphics/juce_graphics.h>

namespace kb::library
{
    /** Recorte cuadrado al centro de 'src' achicado a size x size, de alta calidad.
        Con 'circle' = true devuelve ARGB con un círculo antialiasado (fuera transparente). */
    inline juce::Image squareResize (const juce::Image& source, int size, bool circle = false)
    {
        if (! source.isValid() || size <= 0)
            return {};

        auto img = juce::SoftwareImageType().convert (source);
        const auto side = juce::jmin (img.getWidth(), img.getHeight());
        juce::Rectangle<int> area ((img.getWidth() - side) / 2, (img.getHeight() - side) / 2, side, side);

        // Pasos de 1/2 mientras sobre más del doble.
        while (area.getWidth() >= size * 2)
        {
            const auto half = juce::jmax (1, area.getWidth() / 2);
            juce::Image next (juce::Image::ARGB, half, half, true, juce::SoftwareImageType());
            {
                juce::Graphics g (next);
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.drawImage (img, 0, 0, half, half, area.getX(), area.getY(), area.getWidth(), area.getHeight());
            }
            img = next;
            area = { 0, 0, half, half };
        }

        juce::Image out (circle ? juce::Image::ARGB : juce::Image::RGB, size, size, true, juce::SoftwareImageType());
        juce::Graphics g (out);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (circle)
        {
            juce::Path p;
            p.addEllipse (0.0f, 0.0f, (float) size, (float) size);
            g.reduceClipRegion (p);
        }
        g.drawImage (img, 0, 0, size, size, area.getX(), area.getY(), area.getWidth(), area.getHeight());
        return out;
    }

    /** Guarda la última versión achicada de una imagen: solo se recalcula si cambia
        la imagen o el tamaño (para dibujar a 60 fps sin costo). Hilo de mensajes. */
    struct ScaledImageCache
    {
        juce::Image get (const juce::Image& source, int size, bool circle = false)
        {
            if (! source.isValid() || size <= 0)
                return {};
            if (source != lastSource || size != lastSize || circle != lastCircle || ! result.isValid())
            {
                result = squareResize (source, size, circle);
                lastSource = source;   // se guarda la referencia: la memoria no se reutiliza
                lastSize = size;
                lastCircle = circle;
            }
            return result;
        }

    private:
        juce::Image result;
        juce::Image lastSource;
        int lastSize = 0;
        bool lastCircle = false;
    };
} // namespace kb::library
