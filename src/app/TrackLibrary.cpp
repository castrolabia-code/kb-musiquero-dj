#include "app/TrackLibrary.h"

#include "analysis/KeyDetector.h"
#include "app/I18n.h"
#include "engine/AudioFileFormats.h"
#include "library/TagReader.h"

#include <algorithm>

namespace kb::app
{
    TrackLibrary::TrackLibrary (AnalysisCache& c, library::LibraryDatabase& d)
        : cache (c), db (d)
    {
        session = db.newSession();

        // Cuando llega un análisis (de la lista o de un deck), refrescar.
        juce::WeakReference<TrackLibrary> weak (this);
        cache.addListener ([weak] (const juce::File& file)
        {
            if (auto* self = weak.get())
            {
                self->pending.erase (file.getFullPathName());
                self->rebuild();
                self->notify();
            }
        });

        // Pistas de la colección sin analizar o sin tags (p. ej. migradas): a la cola.
        std::vector<juce::int64> needTags;
        for (const auto& t : db.collection())
        {
            if (! t.file.existsAsFile())
                continue;
            if (! cache.find (t.file).has_value())
                queueAnalysis (t.file);
            if (! t.tagsRead)
                needTags.push_back (t.id);
        }
        queueTags (needTags);
        rebuild();
    }

    TrackLibrary::~TrackLibrary()
    {
        tagPool.removeAllJobs (true, 3000);
    }

    // =========================================================================
    //  Vista
    // =========================================================================
    void TrackLibrary::setSource (Source s)
    {
        source = s;
        rebuild();
        notify();
    }

    void TrackLibrary::setSearch (const juce::String& text)
    {
        if (text == search)
            return;
        search = text;
        rebuild();
        notify();
    }

    bool TrackLibrary::matches (const library::TrackRecord& t, const juce::StringArray& words) const
    {
        if (words.isEmpty())
            return true;
        juce::String hay;
        hay << t.title << ' ' << t.artist << ' ' << t.album << ' ' << t.genre << ' ' << t.file.getFileName();
        if (t.bpm > 0.0)
            hay << ' ' << juce::roundToInt (t.bpm);
        if (t.key >= 0)
            hay << ' ' << analysis::MusicalKey::camelot (t.key) << ' ' << analysis::MusicalKey::name (t.key);
        hay = hay.toLowerCase();
        for (const auto& w : words)
            if (! hay.contains (w))
                return false;
        return true;
    }

    void TrackLibrary::rebuild()
    {
        std::vector<library::TrackRecord> all;
        switch (source.kind)
        {
            case SourceKind::collection:  all = db.collection(); break;
            case SourceKind::history:     all = db.tracksById (db.historyTracks (session)); break;
            case SourceKind::preparation: all = db.tracksById (db.playlistTracks (preparationId())); break;
            case SourceKind::playlist:    all = db.tracksById (db.playlistTracks (source.playlistId)); break;
        }

        auto words = juce::StringArray::fromTokens (search.toLowerCase(), " ", "\"");
        words.removeEmptyStrings();
        rows.clear();
        sourceIndices.clear();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            if (matches (all[(size_t) i], words))
            {
                rows.push_back (all[(size_t) i]);
                sourceIndices.push_back (i);
            }
        }
    }

    void TrackLibrary::notify()
    {
        if (onChanged)
            onChanged();
    }

    bool TrackLibrary::isAnalysing (int row) const
    {
        return juce::isPositiveAndBelow (row, size()) && pending.count (rows[(size_t) row].file.getFullPathName()) > 0;
    }

    // =========================================================================
    //  Colección
    // =========================================================================
    int TrackLibrary::addFiles (const juce::Array<juce::File>& files)
    {
        std::vector<juce::int64> ids;
        const auto before = db.collectionSize();
        db.begin();
        for (const auto& f : files)
        {
            if (! f.existsAsFile() || ! engine::formats::isSupportedFile (f))
                continue;
            const auto id = db.ensureTrack (f, true);
            if (id == 0)
                continue;
            ids.push_back (id);
        }
        db.commit();

        // Agregadas estando en una playlist: también van a esa playlist.
        if (source.kind == SourceKind::playlist || source.kind == SourceKind::preparation)
            db.addToPlaylist (source.kind == SourceKind::playlist ? source.playlistId : preparationId(), ids);

        std::vector<juce::int64> needTags;
        for (auto id : ids)
        {
            const auto t = db.findById (id);
            if (! t.has_value())
                continue;
            if (! t->tagsRead)
                needTags.push_back (id);
            if (! cache.find (t->file).has_value())
                queueAnalysis (t->file);
        }
        queueTags (needTags);

        rebuild();
        notify();
        return db.collectionSize() - before;
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
                if (found.size() >= 50000)
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

    void TrackLibrary::queueTags (const std::vector<juce::int64>& ids)
    {
        if (ids.empty())
            return;
        // Leer tags en segundo plano y guardarlos en lotes (hilo de mensajes).
        std::vector<std::pair<juce::int64, juce::File>> work;
        for (const auto& t : db.tracksById (ids))
            work.emplace_back (t.id, t.file);

        juce::WeakReference<TrackLibrary> weak (this);
        tagPool.addJob ([weak, work]
        {
            std::vector<std::pair<juce::int64, library::TrackTags>> results;
            auto flush = [&weak, &results]
            {
                juce::MessageManager::callAsync ([weak, batch = std::move (results)]
                {
                    if (auto* self = weak.get())
                    {
                        self->db.begin();
                        for (const auto& [id, tags] : batch)
                            self->db.setTags (id, tags.title, tags.artist, tags.album, tags.genre, tags.year,
                                              tags.bpm, tags.durationSeconds);
                        self->db.commit();
                        self->rebuild();
                        self->notify();
                    }
                });
                results.clear();
            };
            for (const auto& [id, file] : work)
            {
                if (weak.get() == nullptr)
                    return;
                results.emplace_back (id, library::TagReader::read (file));
                if (results.size() >= 50)
                    flush();
            }
            if (! results.empty())
                flush();
        });
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

    void TrackLibrary::remove (const std::vector<int>& rowIndices)
    {
        if (source.kind == SourceKind::playlist || source.kind == SourceKind::preparation)
        {
            std::vector<int> positions;
            for (auto r : rowIndices)
                if (juce::isPositiveAndBelow (r, size()))
                    positions.push_back (sourceIndices[(size_t) r]);
            db.removeFromPlaylist (source.kind == SourceKind::playlist ? source.playlistId : preparationId(), positions);
        }
        else if (source.kind == SourceKind::collection)
        {
            std::vector<juce::int64> ids;
            for (auto r : rowIndices)
                if (juce::isPositiveAndBelow (r, size()))
                    ids.push_back (rows[(size_t) r].id);
            db.setInCollection (ids, false);
        }
        rebuild();
        notify();
    }

    void TrackLibrary::clear()
    {
        if (source.kind == SourceKind::playlist || source.kind == SourceKind::preparation)
        {
            std::vector<int> all;
            for (int i = 0; i < (int) db.playlistTracks (source.kind == SourceKind::playlist ? source.playlistId : preparationId()).size(); ++i)
                all.push_back (i);
            db.removeFromPlaylist (source.kind == SourceKind::playlist ? source.playlistId : preparationId(), all);
        }
        else
        {
            db.clearCollection();
        }
        rebuild();
        notify();
    }

    void TrackLibrary::setRating (int row, int rating)
    {
        if (! juce::isPositiveAndBelow (row, size()))
            return;
        db.setRating (rows[(size_t) row].id, rating);
        rows[(size_t) row].rating = juce::jlimit (0, 5, rating);
        notify();
    }

    // =========================================================================
    //  Playlists e historial
    // =========================================================================
    juce::int64 TrackLibrary::createPlaylist (const juce::String& name)
    {
        const auto id = db.createPlaylist (name);
        notify();
        return id;
    }

    void TrackLibrary::renamePlaylist (juce::int64 id, const juce::String& name)
    {
        db.renamePlaylist (id, name);
        notify();
    }

    void TrackLibrary::deletePlaylist (juce::int64 id)
    {
        db.deletePlaylist (id);
        if (source.kind == SourceKind::playlist && source.playlistId == id)
            source = {};
        rebuild();
        notify();
    }

    void TrackLibrary::addToPlaylist (juce::int64 playlistId, const std::vector<juce::int64>& trackIds)
    {
        db.addToPlaylist (playlistId, trackIds);
        rebuild();
        notify();
    }

    juce::int64 TrackLibrary::preparationId()
    {
        return db.preparationPlaylist (tr ("library.preparation"));
    }

    void TrackLibrary::markPlayed (const juce::File& file)
    {
        const auto id = db.ensureTrack (file, false);
        if (id == 0)
            return;
        db.markPlayed (id, session);
        if (source.kind == SourceKind::history)
            rebuild();
        notify();
    }
} // namespace kb::app
