#pragma once
// =============================================================================
//  TrackAnalyser: cola de análisis en segundo plano (un hilo, prioridad baja).
//
//    * analyseTrack(): para una pista ya cargada en un deck. Espera a que
//      termine de decodificarse, la analiza y guarda BPM, grilla y tonalidad.
//    * analyseFile():  para la lista de canciones. Decodifica el archivo a
//      22 kHz (rápido y liviano), analiza y devuelve el resultado.
//
//  Los callbacks se llaman desde el hilo de análisis (la app los pasa al de
//  mensajes con MessageManager::callAsync).
// =============================================================================
#include "analysis/BpmDetector.h"
#include "analysis/KeyDetector.h"
#include "engine/TrackBuffer.h"

#include <juce_core/juce_core.h>

#include <functional>
#include <memory>

namespace kb::analysis
{
    struct FileAnalysis
    {
        BeatInfo beat;
        KeyInfo  key;
        double   durationSeconds = 0.0;
        juce::String formatName;
        bool     ok = false;
    };

    class TrackAnalyser final
    {
    public:
        explicit TrackAnalyser (juce::String threadName = "KB Analysis");
        ~TrackAnalyser();

        /** Calcula BPM, grilla y tonalidad y los guarda en la pista. */
        void analyseTrack (std::shared_ptr<engine::TrackBuffer> track, std::function<void (BeatInfo, KeyInfo)> onDone);
        void analyseFile (const juce::File& file, std::function<void (FileAnalysis)> onDone);

        /** Análisis sincrónico de un archivo (pruebas y herramientas). */
        static FileAnalysis analyseFileBlocking (const juce::File& file, const std::atomic<bool>* cancel = nullptr);

        int getNumPendingJobs() const { return pool.getNumJobs(); }

    private:
        juce::ThreadPool pool;
        std::shared_ptr<std::atomic<bool>> shuttingDown = std::make_shared<std::atomic<bool>> (false);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackAnalyser)
    };
} // namespace kb::analysis
