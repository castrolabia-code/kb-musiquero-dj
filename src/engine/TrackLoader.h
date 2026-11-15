#pragma once
// =============================================================================
//  TrackLoader: carga y decodifica pistas en hilos de fondo.
//
//  * Un hilo por deck (pool de 2): cargar en el deck 2 no espera al deck 1.
//  * La pista se entrega "lista para reproducir" en cuanto se reservó la
//    memoria (onReady); la decodificación continúa y se publica de a bloques.
//  * Conversión al sample rate del motor con r8brain (calidad 24 bits, fase
//    lineal, con filtro anti-aliasing) en el mismo hilo de carga.
//  * Una nueva carga en el mismo slot cancela la anterior.
//
//  Nada de esto corre en el hilo de audio.
// =============================================================================
#include "engine/TrackBuffer.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <memory>

namespace kb::engine
{
    enum class LoadError
    {
        none,
        fileNotFound,
        unsupportedFormat,
        unreadable,
        empty,
        tooLong,
        outOfMemory,
        cancelled
    };

    struct LoadResult
    {
        std::shared_ptr<TrackBuffer> track;
        LoadError error = LoadError::none;
    };

    class TrackLoader final
    {
    public:
        /** Límite de memoria por pista (~46 min estéreo a 48 kHz). */
        static constexpr std::int64_t kMaxBytesPerTrack = std::int64_t (1) << 30;

        /** Llamado desde el hilo de carga apenas la pista se puede reproducir. */
        using ReadyCallback = std::function<void (std::shared_ptr<TrackBuffer>)>;
        /** Llamado desde el hilo de carga si falla (nunca para 'cancelled'). */
        using ErrorCallback = std::function<void (LoadError)>;

        explicit TrackLoader (int numSlots = 2);
        ~TrackLoader();

        /** Encola la carga en un slot (0 = deck 1, 1 = deck 2). Hilo de mensajes. */
        void loadAsync (int slot, const juce::File& file, double targetSampleRate,
                        ReadyCallback onReady, ErrorCallback onError);

        /** Cancela la carga en curso de un slot (si la hay). Hilo de mensajes. */
        void cancel (int slot);

        /** Carga completa y sincrónica (pruebas y herramientas). */
        static LoadResult loadBlocking (const juce::File& file, double targetSampleRate);

        /** "Artista - Título.mp3" -> { artista, título }. Si no hay " - ", todo es título. */
        static void parseFileName (const juce::File& file, juce::String& artist, juce::String& title);

    private:
        class Job;

        /** Núcleo de la decodificación (corre en el hilo que la llama). */
        static LoadError decode (const juce::File& file, double targetSampleRate,
                                 const std::atomic<bool>& cancelFlag,
                                 const ReadyCallback& onReady,
                                 std::shared_ptr<TrackBuffer>& outTrack);

        juce::ThreadPool pool;
        std::vector<std::shared_ptr<std::atomic<bool>>> slotCancelFlags;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLoader)
    };
} // namespace kb::engine
