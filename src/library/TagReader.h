#pragma once
// =============================================================================
//  TagReader (Fase 6): tags y carátula de un archivo de audio, con TagLib.
//
//  Formatos: MP3 (ID3v1/ID3v2), FLAC, OGG, M4A/AAC (MP4), WAV y AIFF (ID3 o
//  INFO), APE. La carátula sale de la imagen incrustada en el archivo; si no
//  hay, de una imagen de la carpeta (cover / folder / front .jpg / .png).
//
//  Hilos: se puede llamar desde cualquier hilo (cada llamada abre su archivo).
// =============================================================================
#include <juce_graphics/juce_graphics.h>

namespace kb::library
{
    struct TrackTags
    {
        juce::String title, artist, album, genre, comment, keyText;
        int    year = 0;
        double bpm = 0.0;               // BPM del tag (si el archivo lo trae)
        double durationSeconds = 0.0;
        int    bitrateKbps = 0, sampleRate = 0;
        bool   valid = false;           // TagLib pudo abrir el archivo
    };

    class TagReader final
    {
    public:
        static TrackTags read (const juce::File& file);

        /** Bytes de la carátula (JPEG/PNG) incrustada; vacío si no tiene. */
        static juce::MemoryBlock readEmbeddedArtwork (const juce::File& file);
        /** Imagen de la carpeta de la canción (cover.jpg, folder.jpg...), si existe. */
        static juce::File findFolderArtwork (const juce::File& file);

        /** Carátula lista para usar (incrustada o de la carpeta), achicada a 'maxSize'.
            Imagen inválida si no hay. */
        static juce::Image readArtwork (const juce::File& file, int maxSize);

        /** Para pruebas: escribe título, artista, género, BPM y una carátula. */
        static bool writeForTest (const juce::File& file, const TrackTags& tags, const juce::MemoryBlock& jpegOrPng);
    };
} // namespace kb::library
