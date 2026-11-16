#pragma once
// =============================================================================
//  ArtworkCache (Fase 6): carátulas listas para dibujar.
//
//  * request(): busca la carátula en segundo plano (incrustada o de la
//    carpeta), la guarda achicada en disco (artwork/<clave>.jpg) y avisa en el
//    hilo de mensajes. La segunda vez sale del disco (o de la memoria) al toque.
//  * getThumbnail(): versión chica para la lista (se calcula una vez).
//  Uso desde el hilo de mensajes.
// =============================================================================
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace kb::library
{
    class ArtworkCache final
    {
    public:
        static constexpr int kSize = 1024;   // nítida hasta en 4K

        explicit ArtworkCache (juce::File cacheDirectory);
        ~ArtworkCache();

        using Callback = std::function<void (const juce::Image&)>;

        /** Pide la carátula; 'callback' se llama en el hilo de mensajes (imagen inválida = no tiene). */
        void request (const juce::File& audioFile, Callback callback);

        /** Si ya está en memoria: la imagen (inválida si se sabe que no tiene). */
        bool lookup (const juce::File& audioFile, juce::Image& result) const;

        /** Miniatura para la lista; si todavía no está, la pide y devuelve inválida. */
        juce::Image getThumbnail (const juce::File& audioFile, int size);

        /** Se llama cuando llega una carátula nueva (para repintar la lista). */
        std::function<void()> onArtworkArrived;

        /** Cómo se pasa un resultado al hilo de mensajes (por defecto MessageManager::callAsync).
            Las pruebas lo reemplazan para procesar la cola a mano. */
        std::function<void (std::function<void()>)> postToMessageThread;

        /** Clave de disco para un archivo (cambia si el archivo cambia). */
        static juce::String keyFor (const juce::File& audioFile);

    private:
        void finished (const juce::File& file, const juce::Image& image);

        juce::File dir;
        juce::ThreadPool pool { juce::ThreadPoolOptions().withThreadName ("KB Artwork").withNumberOfThreads (1) };
        std::map<juce::String, juce::Image> memory;          // ruta -> imagen (inválida = sin carátula)
        std::map<juce::String, juce::Image> thumbnails;      // ruta|tamaño -> miniatura
        std::map<juce::String, std::vector<Callback>> waiting;
        std::vector<juce::String> order;                     // para descartar las más viejas
        std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArtworkCache)
    };
} // namespace kb::library
