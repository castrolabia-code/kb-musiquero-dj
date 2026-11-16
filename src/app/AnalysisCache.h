#pragma once
// =============================================================================
//  AnalysisCache: resultados de análisis por archivo (BPM, grilla, duración,
//  tonalidad) y hot cues. Desde la Fase 6 se guardan en la base SQLite de la
//  biblioteca (library.db); esta clase es la fachada que usa el resto de la app.
//
//  Si el archivo cambia (tamaño o fecha), su análisis deja de valer y se vuelve
//  a analizar. Uso exclusivo desde el hilo de mensajes.
// =============================================================================
#include <juce_core/juce_core.h>

#include "library/LibraryDatabase.h"

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace kb::app
{
    struct AnalysisEntry
    {
        double bpm              = 0.0;
        double firstBeatSeconds = 0.0;
        double durationSeconds  = 0.0;
        int    key              = -1;   // 0..23, -1 desconocida
        juce::String formatName;
    };

    class AnalysisCache final
    {
    public:
        explicit AnalysisCache (library::LibraryDatabase& database);

        std::optional<AnalysisEntry> find (const juce::File& file) const;
        void store (const juce::File& file, const AnalysisEntry& entry);

        /** Hot cues de una pista (8 posiciones en segundos; < 0 = vacío). */
        using HotCues = library::HotCues;
        std::optional<HotCues> findHotCues (const juce::File& file) const;
        void storeHotCues (const juce::File& file, const HotCues& cues);

        /** Se llama cada vez que se guarda un resultado nuevo. */
        void addListener (std::function<void (const juce::File&)> callback) { listeners.push_back (std::move (callback)); }

        /** Compatibilidad: la base guarda al instante (no hace falta "save"). */
        bool save() { return true; }

        /** Migración de las versiones 0.3-0.8 (analysis.json y library.json) a la base.
            Los JSON quedan renombrados como *.migrado. Devuelve cuántas pistas importó. */
        static int importLegacy (library::LibraryDatabase& db, const juce::File& analysisJson, const juce::File& libraryJson);

        library::LibraryDatabase& getDatabase() noexcept { return db; }

    private:
        library::LibraryDatabase& db;
        std::vector<std::function<void (const juce::File&)>> listeners;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisCache)
    };
} // namespace kb::app
