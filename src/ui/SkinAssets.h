#pragma once
// =============================================================================
//  SkinAssets (Fase 7): renders opcionales del skin (sección 6, enfoque híbrido).
//
//  Si existen imágenes en la carpeta "skin" (al lado del ejecutable, o en la
//  carpeta de datos de la app), se usan en lugar del dibujo por código:
//
//    chassis.png   chasis amarillo de toda la ventana
//    platter.png   base del plato (aro y sombra; el vinilo que gira se dibuja encima)
//    mixer.png     panel negro del mixer
//
//  Cada una puede venir en 3 resoluciones: nombre.png (@1x), nombre@2x.png y
//  nombre@4x.png; se elige la más chica que no pixele en la pantalla actual.
//  Sin imágenes, todo se dibuja por código (como hasta ahora). Ver docs/SKIN.md.
// =============================================================================
#include <juce_graphics/juce_graphics.h>

#include <map>

namespace kb::ui
{
    class SkinAssets final
    {
    public:
        static SkinAssets& get();

        /** Imagen para dibujar a 'pixelsNeeded' de ancho físico; inválida si no hay render. */
        juce::Image find (const juce::String& name, float pixelsNeeded);

        /** Carpetas donde se buscan los renders (en orden). */
        juce::Array<juce::File> folders() const;

    private:
        std::map<juce::String, juce::Image> cache;
    };
} // namespace kb::ui
