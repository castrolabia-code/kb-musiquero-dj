#include "app/TrackLibrary.h"

#include "engine/AudioFileFormats.h"
#include "engine/TrackLoader.h"

#include <algorithm>

namespace kb::app
{
    TrackLibrary::TrackLibrary (AnalysisCache& c, juce::File f)
        : cache (c), storage (std::move (f))
    {
        load();

        // Cuando el cache recibe un análisis (de la lista o de un deck), refrescar.
        juce::WeakReference<TrackLibrary> weak (this);
        cache.addListener ([weak] (const juce::File& file)
        {
            if (auto* self = weak.get())
            {
                self->pending.erase (file.getFullPathName());
                self->notify();
            }
        });
    }

    TrackLibrary::~TrackLibrary()
    {
        save();
    }

    int TrackLibrary::addFiles (const juce::Array<juce::File>& files)
    {
        int added = 0;
        for (const auto& f : files)
        {
            if (! f.existsAsFile() || ! engine::formats::isSupportedFile (f))
                continue;
            const auto already = std::any_of (items.begin(), items.end(), [&f] (const auto& it) { return it.file == f; });
            if (already)
                continue;

            LibraryItem item;
            item.file = f;
            engine::TrackLoader::parseFileName (f, item.artist, item.title);
            items.push_back (item);
            ++added;

            if (! cache.find (f).has_value())
                queueAnalysis (f);
        }

        if (added > 0)
        {
            save();
            notify();
        }
        return added;
    }

    void TrackLibrary::addFolder (const juce::File& folder)
    {
        juce::WeakReference<TrackLibrary> weak (this);
        // La búsqueda puede tardar en carpetas grandes: hilo aparte.
        juce::Thread::launch ([weak, folder]
        {
            juce::Array<juce::File> found;
            for (const auto& entry : juce::RangedDirectoryIterator (folder, true, engine::formats::fileWildcard(),
                                                                    juce::File::findFiles))
            {
                found.add (entry.getFile());
                if (found.size() >= 20000)
                    break;
            }
            found.sort();

            juce::MessageManager::callAsync ([weak, found]
            {
                if (auto* self = weak.get())
                    self->addFiles (found);
            });
        });
    }

    void TrackLibrary::remove (const std::vector<int>& indices)
    {
        auto sorted = indices;
        std::sort (sorted.rbegin(), sorted.rend());
        for (auto i : sorted)
            if (juce::isPositiveAndBelow (i, (int) items.size()))
                items.erase (items.begin() + i);
        save();
        notify();
    }

    void TrackLibrary::clear()
    {
        items.clear();
        save();
        notify();
    }

    std::optional<AnalysisEntry> TrackLibrary::analysisFor (int i) const
    {
        return juce::isPositiveAndBelow (i, (int) items.size()) ? cache.find (items[(size_t) i].file) : std::nullopt;
    }

    bool TrackLibrary::isAnalysing (int i) const
    {
        return juce::isPositiveAndBelow (i, (int) items.size())
               && pending.count (items[(size_t) i].file.getFullPathName()) > 0;
    }

    void TrackLibrary::queueAnalysis (const juce::File& file)
    {
        if (! pending.insert (file.getFullPathName()).second)
            return;

        juce::WeakReference<TrackLibrary> weak (this);
        analyser.analyseFile (file, [weak, file] (analysis::FileAnalysis r)
        {
            juce::MessageManager::callAsync ([weak, file, r]
            {
                if (auto* self = weak.get())
                {
                    AnalysisEntry e;
                    e.bpm              = r.beat.bpm;
                    e.firstBeatSeconds = r.beat.firstBeatSeconds;
                    e.durationSeconds  = r.durationSeconds;
                    e.formatName       = r.formatName;
                    e.key              = r.key.key;
                    self->pending.erase (file.getFullPathName());
                    self->cache.store (file, e);   // notifica a la lista
                }
            });
        });
    }

    void TrackLibrary::notify()
    {
        if (onChanged)
            onChanged();
    }

    void TrackLibrary::load()
    {
        const auto json = juce::JSON::parse (storage);
        if (const auto* list = json.getProperty ("files", {}).getArray())
        {
            for (const auto& v : *list)
            {
                const juce::File f (v.toString());
                if (! f.existsAsFile())
                    continue;   // el archivo ya no está
                LibraryItem item;
                item.file = f;
                engine::TrackLoader::parseFileName (f, item.artist, item.title);
                items.push_back (item);
                if (! cache.find (f).has_value())
                    queueAnalysis (f);
            }
        }
    }

    bool TrackLibrary::save()
    {
        juce::Array<juce::var> list;
        for (const auto& it : items)
            list.add (it.file.getFullPathName());
        auto* root = new juce::DynamicObject();
        root->setProperty ("version", 1);
        root->setProperty ("files", list);
        storage.getParentDirectory().createDirectory();
        return storage.replaceWithText (juce::JSON::toString (juce::var (root)));
    }
} // namespace kb::app
