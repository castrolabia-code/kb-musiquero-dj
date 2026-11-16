#pragma once
// =============================================================================
//  PlatformFiles (Fase 10): acceso a archivos igual en escritorio y en Android.
//
//  * resolve(): convierte lo que devuelve un FileChooser en archivos locales.
//    En escritorio son rutas comunes. En Android el selector del sistema
//    devuelve enlaces content://: se copian (en segundo plano) a la carpeta
//    de la app "Importadas" para que TagLib y los decodificadores los lean.
//  * requestMusicAccess() / deviceMusicFolders(): permiso de Android para
//    leer la música del dispositivo y las carpetas donde suele estar.
//  Todo se llama desde el hilo de mensajes; los callbacks también llegan ahí.
// =============================================================================
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace kb::app::platform
{
    /** true en Android (sin rutas de archivo directas desde el selector). */
    constexpr bool isMobile() noexcept
    {
       #if JUCE_ANDROID || JUCE_IOS
        return true;
       #else
        return false;
       #endif
    }

    /** Archivos locales para los resultados del selector (los content:// se copian). */
    void resolve (const juce::Array<juce::URL>& urls, std::function<void (juce::Array<juce::File>)> done);

    /** Pide (si hace falta) permiso para leer la música del dispositivo. */
    void requestMusicAccess (std::function<void (bool granted)> done);

    /** Carpetas de música del dispositivo que existen (Android: Music, Download, ...). */
    juce::Array<juce::File> deviceMusicFolders();

    /** Carpeta donde se guardan las canciones importadas desde el selector. */
    juce::File importedFolder();
} // namespace kb::app::platform
