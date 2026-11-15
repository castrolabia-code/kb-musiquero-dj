#include "app/AnalysisCache.h"

namespace kb::app
{
    AnalysisCache::AnalysisCache (juce::File f) : storage (std::move (f))
    {
        load();
    }

    AnalysisCache::~AnalysisCache()
    {
        save();
    }

    std::optional<AnalysisEntry> AnalysisCache::find (const juce::File& file) const
    {
        const auto it = entries.find (file.getFullPathName());
        if (it == entries.end())
            return std::nullopt;

        // El archivo cambió desde el análisis: no vale.
        if (it->second.fileSize != file.getSize()
            || it->second.modified != file.getLastModificationTime().toMilliseconds())
            return std::nullopt;

        return it->second.entry;
    }

    void AnalysisCache::store (const juce::File& file, const AnalysisEntry& entry)
    {
        Stored s;
        s.entry    = entry;
        s.fileSize = file.getSize();
        s.modified = file.getLastModificationTime().toMilliseconds();
        entries[file.getFullPathName()] = s;
        dirty = true;

        const auto toNotify = listeners;
        for (auto& l : toNotify)
            l (file);
    }

    void AnalysisCache::load()
    {
        const auto json = juce::JSON::parse (storage);
        const auto* list = json.getProperty ("entries", {}).getArray();
        if (list == nullptr)
            return;

        for (const auto& v : *list)
        {
            Stored s;
            s.entry.bpm              = (double) v.getProperty ("bpm", 0.0);
            s.entry.firstBeatSeconds = (double) v.getProperty ("firstBeat", 0.0);
            s.entry.durationSeconds  = (double) v.getProperty ("duration", 0.0);
            s.entry.formatName       = v.getProperty ("format", {}).toString();
            // Las entradas de antes de la detección de tonalidad se vuelven a analizar.
            if (! v.hasProperty ("key"))
                continue;
            s.entry.key              = (int) v.getProperty ("key", -1);
            s.fileSize               = (juce::int64) v.getProperty ("size", 0);
            s.modified               = (juce::int64) v.getProperty ("modified", 0);
            const auto path = v.getProperty ("path", {}).toString();
            if (path.isNotEmpty())
                entries[path] = s;
        }
    }

    bool AnalysisCache::save()
    {
        if (! dirty)
            return true;

        juce::Array<juce::var> list;
        for (const auto& [path, s] : entries)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("path", path);
            o->setProperty ("bpm", s.entry.bpm);
            o->setProperty ("firstBeat", s.entry.firstBeatSeconds);
            o->setProperty ("duration", s.entry.durationSeconds);
            o->setProperty ("format", s.entry.formatName);
            o->setProperty ("key", s.entry.key);
            o->setProperty ("size", s.fileSize);
            o->setProperty ("modified", s.modified);
            list.add (juce::var (o));
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("version", 2);
        root->setProperty ("entries", list);

        storage.getParentDirectory().createDirectory();
        const auto ok = storage.replaceWithText (juce::JSON::toString (juce::var (root)));
        if (ok)
            dirty = false;
        return ok;
    }
} // namespace kb::app
