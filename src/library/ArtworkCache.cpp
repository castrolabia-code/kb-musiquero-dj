#include "library/ArtworkCache.h"

#include "library/TagReader.h"

namespace kb::library
{
    static constexpr size_t kMaxInMemory = 48;

    ArtworkCache::ArtworkCache (juce::File d) : dir (std::move (d))
    {
        dir.createDirectory();
    }

    ArtworkCache::~ArtworkCache()
    {
        alive->store (false);
        pool.removeAllJobs (true, 5000);
    }

    juce::String ArtworkCache::keyFor (const juce::File& f)
    {
        const auto id = f.getFullPathName() + "|" + juce::String (f.getSize()) + "|"
                      + juce::String (f.getLastModificationTime().toMilliseconds());
        // Dos hashes de 64 bits distintos (texto y texto invertido): colisión prácticamente imposible.
        auto reversed = juce::String();
        for (auto i = id.length(); --i >= 0;)
            reversed += id[i];
        return juce::String::toHexString ((juce::int64) id.hashCode64()) + juce::String::toHexString ((juce::int64) reversed.hashCode64());
    }

    bool ArtworkCache::lookup (const juce::File& file, juce::Image& result) const
    {
        const auto it = memory.find (file.getFullPathName());
        if (it == memory.end())
            return false;
        result = it->second;
        return true;
    }

    void ArtworkCache::request (const juce::File& file, Callback callback)
    {
        juce::Image known;
        if (lookup (file, known))
        {
            if (callback) callback (known);
            return;
        }

        const auto path = file.getFullPathName();
        auto& list = waiting[path];
        const bool alreadyPending = ! list.empty();
        list.push_back (std::move (callback));
        if (alreadyPending)
            return;

        auto flag = alive;
        const auto cacheDir = dir;
        pool.addJob ([this, flag, file, cacheDir]
        {
            if (! flag->load())
                return;
            const auto key = keyFor (file);
            const auto jpg  = cacheDir.getChildFile (key + ".jpg");
            const auto none = cacheDir.getChildFile (key + ".none");

            juce::Image img;
            if (jpg.existsAsFile())
                img = juce::ImageFileFormat::loadFrom (jpg);
            else if (! none.existsAsFile())
            {
                img = TagReader::readArtwork (file, kSize);
                if (img.isValid())
                {
                    juce::JPEGImageFormat fmt;
                    fmt.setQuality (0.88f);
                    juce::FileOutputStream out (jpg);
                    if (out.openedOk())
                        fmt.writeImageToStream (img, out);
                }
                else
                {
                    none.create();   // recordar que no tiene (no volver a buscar)
                }
            }

            auto deliver = [this, flag, file, img]
            {
                if (flag->load())
                    finished (file, img);
            };
            if (postToMessageThread)
                postToMessageThread (std::move (deliver));
            else
                juce::MessageManager::callAsync (std::move (deliver));
        });
    }

    void ArtworkCache::finished (const juce::File& file, const juce::Image& image)
    {
        const auto path = file.getFullPathName();
        memory[path] = image;
        order.push_back (path);
        while (order.size() > kMaxInMemory)
        {
            memory.erase (order.front());
            order.erase (order.begin());
        }

        auto callbacks = std::move (waiting[path]);
        waiting.erase (path);
        for (auto& cb : callbacks)
            if (cb) cb (image);
        if (onArtworkArrived)
            onArtworkArrived();
    }

    juce::Image ArtworkCache::getThumbnail (const juce::File& file, int size)
    {
        const auto key = file.getFullPathName() + "|" + juce::String (size);
        if (const auto it = thumbnails.find (key); it != thumbnails.end())
            return it->second;

        juce::Image full;
        if (! lookup (file, full))
        {
            request (file, nullptr);
            return {};
        }
        auto thumb = full.isValid() ? full.rescaled (size, size, juce::Graphics::mediumResamplingQuality) : juce::Image();
        if (thumbnails.size() > 400)
            thumbnails.clear();
        thumbnails[key] = thumb;
        return thumb;
    }
} // namespace kb::library
