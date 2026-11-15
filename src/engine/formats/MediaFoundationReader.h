#pragma once
// =============================================================================
//  MediaFoundationReader (solo Windows): decodifica M4A / AAC / MP4 de audio
//  con Windows Media Foundation (componente del sistema operativo; no se
//  redistribuye nada). Entrega float 32 a la frecuencia original del archivo.
//
//  Pensado para lectura SECUENCIAL (así decodifica TrackLoader). Los saltos
//  hacia atrás o adelante funcionan, pero con la precisión del seek de MF.
//
//  Requisito: el hilo que lo usa debe tener COM inicializado
//  (formats::initialiseDecodingThread()).
// =============================================================================
#include <juce_audio_formats/juce_audio_formats.h>

#if JUCE_WINDOWS

#include <memory>

namespace kb::engine::formats
{
    class MediaFoundationReader final : public juce::AudioFormatReader
    {
    public:
        /** nullptr si el archivo no se puede abrir o no tiene audio. */
        static std::unique_ptr<juce::AudioFormatReader> create (const juce::File& file);

        ~MediaFoundationReader() override;

        bool readSamples (int* const* destChannels, int numDestChannels,
                          int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                          int numSamples) override;

    private:
        struct Impl;
        explicit MediaFoundationReader (std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaFoundationReader)
    };
} // namespace kb::engine::formats

#endif // JUCE_WINDOWS
