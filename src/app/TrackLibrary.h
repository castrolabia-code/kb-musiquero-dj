#pragma once
// =============================================================================
//  TrackLibrary: lista de canciones agregadas con el botón "+" (o arrastrando
//  archivos/carpetas sobre la biblioteca). Se guarda entre sesiones en
//  %APPDATA%\...\library.json y analiza el BPM de cada tema en segundo plano.
//
//  Es la versión simple previa a la biblioteca completa con SQLite (Fase 6).
//  Uso exclusivo desde el hilo de mensajes.
// =============================================================================
#include <juce_events/juce_events.h>

#include "analysis/TrackAnalyser.h"
#include "app/AnalysisCache.h"

#include <functional>
#include <set>
#include <vector>

namespace kb::app
{
    struct LibraryItem
    {
        juce::File   file;
        juce::String title;
        juce::String artist;
    };

    class TrackLibrary final
    {
    public:
        TrackLibrary (AnalysisCache& cache, juce::File storageFile);
        ~TrackLibrary();

        /** Agrega archivos de audio (ignora repetidos y no soportados). Devuelve cuántos agregó. */
        int  addFiles (const juce::Array<juce::File>& files);
        /** Busca audio en la carpeta (y subcarpetas) en segundo plano y lo agrega. */
        void addFolder (const juce::File& folder);
        void remove (const std::vector<int>& indices);
        /** Vacía la lista (no borra ningún archivo del disco ni el caché de análisis). */
        void clear();

        int  size() const noexcept { return (int) items.size(); }
        const LibraryItem& operator[] (int i) const { return items[(size_t) i]; }

        std::optional<AnalysisEntry> analysisFor (int i) const;
        bool isAnalysing (int i) const;
        int  getNumPendingAnalyses() const noexcept { return (int) pending.size(); }

        /** Se llama cuando cambia la lista o llega un análisis. */
        std::function<void()> onChanged;

        bool save();

    private:
        void load();
        void queueAnalysis (const juce::File& file);
        void notify();

        AnalysisCache& cache;
        juce::File storage;
        std::vector<LibraryItem> items;
        std::set<juce::String> pending;
        analysis::TrackAnalyser analyser { "KB Library Analysis" };

        JUCE_DECLARE_WEAK_REFERENCEABLE (TrackLibrary)
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackLibrary)
    };
} // namespace kb::app
