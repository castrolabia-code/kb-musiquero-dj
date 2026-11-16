#include "app/AnalysisCache.h"

#include <algorithm>

namespace kb::app
{
    AnalysisCache::AnalysisCache (library::LibraryDatabase& d) : db (d) {}

    std::optional<AnalysisEntry> AnalysisCache::find (const juce::File& file) const
    {
        const auto t = db.findByPath (file);
        if (! t.has_value() || ! t->analysed)
            return std::nullopt;

        // El archivo cambió desde el análisis: no vale.
        if (t->fileSize != file.getSize() || t->modified != file.getLastModificationTime().toMilliseconds())
            return std::nullopt;

        AnalysisEntry e;
        e.bpm = t->bpm;
        e.firstBeatSeconds = t->firstBeat;
        e.durationSeconds = t->duration;
        e.key = t->key;
        e.formatName = t->format;
        return e;
    }

    void AnalysisCache::store (const juce::File& file, const AnalysisEntry& e)
    {
        const auto id = db.ensureTrack (file, false);
        if (id == 0)
            return;
        db.setAnalysis (id, e.bpm, e.firstBeatSeconds, e.key, e.durationSeconds, e.formatName,
                        file.getSize(), file.getLastModificationTime().toMilliseconds());

        const auto toNotify = listeners;
        for (auto& l : toNotify)
            l (file);
    }

    std::optional<AnalysisCache::HotCues> AnalysisCache::findHotCues (const juce::File& file) const
    {
        const auto t = db.findByPath (file);
        if (! t.has_value())
            return std::nullopt;
        const auto cues = db.hotCues (t->id);
        if (std::all_of (cues.begin(), cues.end(), [] (double c) { return c < 0.0; }))
            return std::nullopt;
        return cues;
    }

    void AnalysisCache::storeHotCues (const juce::File& file, const HotCues& cues)
    {
        const auto id = db.ensureTrack (file, false);
        if (id != 0)
            db.setHotCues (id, cues);
    }

    int AnalysisCache::importLegacy (library::LibraryDatabase& db, const juce::File& analysisJson, const juce::File& libraryJson)
    {
        int imported = 0;
        db.begin();
        if (analysisJson.existsAsFile())
        {
            const auto json = juce::JSON::parse (analysisJson);
            if (const auto* list = json.getProperty ("entries", {}).getArray())
            {
                for (const auto& v : *list)
                {
                    const auto path = v.getProperty ("path", {}).toString();
                    if (path.isEmpty() || ! v.hasProperty ("key"))
                        continue;
                    const auto id = db.ensureTrack (juce::File (path), false);
                    db.setAnalysis (id, (double) v.getProperty ("bpm", 0.0), (double) v.getProperty ("firstBeat", 0.0),
                                    (int) v.getProperty ("key", -1), (double) v.getProperty ("duration", 0.0),
                                    v.getProperty ("format", {}).toString(), (juce::int64) v.getProperty ("size", 0),
                                    (juce::int64) v.getProperty ("modified", 0));
                }
            }
            if (const auto* cues = json.getProperty ("hotCues", {}).getArray())
            {
                for (const auto& v : *cues)
                {
                    const auto path = v.getProperty ("path", {}).toString();
                    if (path.isEmpty())
                        continue;
                    HotCues c;
                    c.fill (-1.0);
                    if (const auto* arr = v.getProperty ("cues", {}).getArray())
                        for (int i = 0; i < juce::jmin (8, arr->size()); ++i)
                            c[(size_t) i] = (double) (*arr)[i];
                    db.setHotCues (db.ensureTrack (juce::File (path), false), c);
                }
            }
        }
        if (libraryJson.existsAsFile())
        {
            const auto json = juce::JSON::parse (libraryJson);
            if (const auto* files = json.getProperty ("files", {}).getArray())
                for (const auto& f : *files)
                    if (f.toString().isNotEmpty() && db.ensureTrack (juce::File (f.toString()), true) != 0)
                        ++imported;
        }
        db.commit();

        for (const auto& f : { analysisJson, libraryJson })
            if (f.existsAsFile())
                f.moveFileTo (f.withFileExtension (f.getFileExtension() + ".migrado"));
        return imported;
    }
} // namespace kb::app
