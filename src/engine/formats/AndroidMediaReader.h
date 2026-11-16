#pragma once
// =============================================================================
//  AndroidMediaReader (solo Android, Fase 10): decodifica M4A / AAC / MP4 de
//  audio con los decodificadores del sistema (NDK: AMediaExtractor +
//  AMediaCodec). No se redistribuye ningún códec. Entrega float 32 a la
//  frecuencia original del archivo.
//
//  Igual que MediaFoundationReader: pensado para lectura SECUENCIAL (así
//  decodifica TrackLoader); los saltos funcionan con la precisión del seek.
// =============================================================================
#include <juce_audio_formats/juce_audio_formats.h>

#if JUCE_ANDROID

#include <memory>

namespace kb::engine::formats
{
    class AndroidMediaReader final : public juce::AudioFormatReader
    {
    public:
        /** nullptr si el archivo no se puede abrir o no tiene audio. */
        static std::unique_ptr<juce::AudioFormatReader> create (const juce::File& file);

        ~AndroidMediaReader() override;

        bool readSamples (int* const* destChannels, int numDestChannels,
                          int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                          int numSamples) override;

    private:
        struct Impl;
        explicit AndroidMediaReader (std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AndroidMediaReader)
    };
} // namespace kb::engine::formats

#endif // JUCE_ANDROID
