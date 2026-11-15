#include "app/AnalysisCache.h"

#include <algorithm>

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

    std::optional<AnalysisCache::HotCues> AnalysisCache::findHotCues (const juce::File& file) const
    {
        const auto it = hotCues.find (file.getFullPathName());
        if (it == hotCues.end())
            return std::nullopt;
        return it->second;
    }

    void AnalysisCache::storeHotCues (const juce::File& file, const HotCues& cues)
    {
        const auto path = file.getFullPathName();
        const bool empty = std::all_of (cues.begin(), cues.end(), [] (double c) { return c < 0.0; });
        const auto it = hotCues.find (path);
        if (empty)
        {
            if (it == hotCues.end())
                return;
            hotCues.erase (it);
        }
        else
        {
            if (it != hotCues.end() && it->second == cues)
                return;
            hotCues[path] = cues;
        }
        dirty = true;
    }

    void AnalysisCache::load()
    {
        const auto json = juce::JSON::parse (storage);
        if (const auto* cueList = json.getProperty ("hotCues", {}).getArray())
        {
            for (const auto& v : *cueList)
            {
                HotCues c;
                c.fill (-1.0);
                if (const auto* arr = v.getProperty ("cues", {}).getArray())
                    for (int i = 0; i < juce::jmin (8, arr->size()); ++i)
                        c[(size_t) i] = (double) (*arr)[i];
                const auto path = v.getProperty ("path", {}).toString();
                if (path.isNotEmpty())
                    hotCues[path] = c;
            }
        }
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

        juce::Array<juce::var> cueList;
        for (const auto& [path, c] : hotCues)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("path", path);
            juce::Array<juce::var> arr;
            for (auto x : c)
                arr.add (x);
            o->setProperty ("cues", arr);
            cueList.add (juce::var (o));
        }
        root->setProperty ("hotCues", cueList);

        storage.getParentDirectory().createDirectory();
        const auto ok = storage.replaceWithText (juce::JSON::toString (juce::var (root)));
        if (ok)
            dirty = false;
        return ok;
    }
} // namespace kb::app
