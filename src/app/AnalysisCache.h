#pragma once
// =============================================================================
//  AnalysisCache: resultados de análisis por archivo (BPM, grilla, duración),
//  guardados en %APPDATA%\Castro Labs\KB Musiquero DJ\analysis.json.
//
//  Si el archivo cambia (tamaño o fecha), su entrada deja de valer y se vuelve
//  a analizar. En la Fase 6 esto pasa a la base de datos SQLite.
//  Uso exclusivo desde el hilo de mensajes.
// =============================================================================
#include <juce_core/juce_core.h>

#include <functional>
#include <map>
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
        explicit AnalysisCache (juce::File storageFile);
        ~AnalysisCache();

        std::optional<AnalysisEntry> find (const juce::File& file) const;
        void store (const juce::File& file, const AnalysisEntry& entry);

        /** Se llama cada vez que se guarda un resultado nuevo. */
        void addListener (std::function<void (const juce::File&)> callback) { listeners.push_back (std::move (callback)); }

        bool save();
        int  size() const noexcept { return (int) entries.size(); }

    private:
        struct Stored
        {
            AnalysisEntry entry;
            juce::int64 fileSize = 0;
            juce::int64 modified = 0;
        };

        void load();

        juce::File storage;
        std::map<juce::String, Stored> entries;
        std::vector<std::function<void (const juce::File&)>> listeners;
        bool dirty = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisCache)
    };
} // namespace kb::app
