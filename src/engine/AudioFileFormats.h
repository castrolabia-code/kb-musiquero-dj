#pragma once
// =============================================================================
//  Formatos de audio soportados y creación de lectores.
//
//    MP3, WAV, AIFF, FLAC, OGG -> decodificadores incluidos en JUCE.
//    M4A / AAC / MP4           -> Windows: Media Foundation (MediaFoundationReader).
//                                 macOS: CoreAudio (vía JUCE). Android: Fase 10.
// =============================================================================
#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace kb::engine::formats
{
    /** Extensiones soportadas en esta plataforma, en minúsculas y sin punto. */
    juce::StringArray supportedExtensions();

    /** "*.mp3;*.wav;..." para diálogos de archivo. */
    juce::String fileWildcard();

    bool isSupportedFile (const juce::File& file);

    /** Crea un lector para el archivo, o nullptr si no se puede abrir/decodificar.
        Llamar desde un hilo de fondo (puede tardar). */
    std::unique_ptr<juce::AudioFormatReader> createReader (const juce::File& file);

    /** Preparar/liberar el hilo actual para decodificar (en Windows inicializa COM,
        que necesita Media Foundation). Se llaman al empezar/terminar cada trabajo. */
    void initialiseDecodingThread();
    void shutdownDecodingThread();
} // namespace kb::engine::formats
