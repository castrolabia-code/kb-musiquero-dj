#pragma once
// =============================================================================
//  TrackBuffer: una pista decodificada en memoria (float 32, estéreo), ya
//  convertida al sample rate del motor.
//
//  Se llena de forma PROGRESIVA desde el hilo de carga: el audio puede
//  reproducir todo lo que ya está decodificado (getDecodedFrames) mientras el
//  resto se sigue cargando ("carga instantánea").
//
//  Concurrencia:
//    * Un solo escritor (el hilo de carga) escribe muestras y luego publica el
//      nuevo total con publishDecoded() (release).
//    * Los lectores (audio, UI) leen solo frames < getDecodedFrames() (acquire),
//      que ya no cambian nunca más: no hay carreras de datos.
//    * La memoria la libera el hilo de mensajes, nunca el de audio.
// =============================================================================
#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdint>
#include <memory>

namespace kb::engine
{
    struct TrackInfo
    {
        juce::File   file;
        juce::String title;
        juce::String artist;
        juce::String formatName;
        double       sourceSampleRate = 0.0;
        int          sourceChannels   = 0;
    };

    class TrackBuffer final
    {
    public:
        static constexpr int kNumChannels  = 2;
        static constexpr int kOverviewBins = 4096;

        /** Reserva toda la memoria. Puede lanzar std::bad_alloc. */
        TrackBuffer (TrackInfo info, double sampleRate, std::int64_t capacityFrames);

        const TrackInfo& getInfo() const noexcept       { return info; }
        std::uint64_t    getId() const noexcept         { return id; }
        double           getSampleRate() const noexcept { return sampleRate; }
        std::int64_t     getCapacityFrames() const noexcept { return capacity; }

        /** Frames listos para reproducir (crece mientras carga). */
        std::int64_t getDecodedFrames() const noexcept { return decoded.load (std::memory_order_acquire); }

        /** Largo de la pista: estimado mientras carga, exacto al terminar. */
        std::int64_t getLengthFrames() const noexcept  { return length.load (std::memory_order_acquire); }
        double       getLengthSeconds() const noexcept { return (double) getLengthFrames() / sampleRate; }

        bool  isComplete() const noexcept { return complete.load (std::memory_order_acquire); }
        float getLoadProgress() const noexcept;

        const float* getReadPointer (int channel) const noexcept { return channels[(size_t) channel].get(); }

        // --- Vista general (picos por bloque, para la UI) -------------------------
        /** Cantidad de frames que representa cada bin de la vista general. */
        double getFramesPerOverviewBin() const noexcept { return framesPerBin; }
        /** Pico (0..1) de un bin. Válido para bins totalmente decodificados. */
        float  getOverviewPeak (int bin) const noexcept;
        int    getOverviewBinsReady() const noexcept;

        // --- Tempo y grilla (los escribe el análisis; los lee cualquier hilo) ---------
        void   setBeatInfo (double bpm, double firstBeatSeconds) noexcept
        {
            firstBeat.store (firstBeatSeconds, std::memory_order_relaxed);
            beatsPerMinute.store (bpm, std::memory_order_release);
        }
        /** 0 mientras no se analizó (o si no se pudo detectar). */
        double getBpm() const noexcept              { return beatsPerMinute.load (std::memory_order_acquire); }
        double getFirstBeatSeconds() const noexcept { return firstBeat.load (std::memory_order_relaxed); }

        // --- Cancelación (la pista se reemplazó antes de terminar de cargar) -------
        void cancelLoading() noexcept        { cancelled.store (true, std::memory_order_relaxed); }
        bool isLoadingCancelled() const noexcept { return cancelled.load (std::memory_order_relaxed); }

        // --- Solo para el hilo de carga ---------------------------------------------
        float* getWritePointer (int channel) noexcept { return channels[(size_t) channel].get(); }
        void   setEstimatedLength (std::int64_t frames) noexcept;
        /** Publica que los frames [0, totalDecoded) están escritos y son definitivos. */
        void   publishDecoded (std::int64_t totalDecoded) noexcept;
        void   markComplete (std::int64_t finalLength) noexcept;

    private:
        void updateOverview (std::int64_t from, std::int64_t to) noexcept;

        TrackInfo     info;
        std::uint64_t id;
        double        sampleRate;
        std::int64_t  capacity;
        double        framesPerBin;

        std::unique_ptr<float[]> channels[kNumChannels];
        std::unique_ptr<float[]> overview;   // kOverviewBins

        std::atomic<std::int64_t> decoded   { 0 };
        std::atomic<std::int64_t> length    { 0 };
        std::atomic<bool>         complete  { false };
        std::atomic<bool>         cancelled { false };
        std::atomic<double>       beatsPerMinute { 0.0 };
        std::atomic<double>       firstBeat      { 0.0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackBuffer)
    };
} // namespace kb::engine
