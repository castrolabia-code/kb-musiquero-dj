#include "library/TagReader.h"

#include "engine/TrackLoader.h"

#include <fileref.h>
#include <tag.h>
#include <tpropertymap.h>
#include <tvariant.h>

namespace kb::library
{
    namespace
    {
        TagLib::FileName nameOf (const juce::File& f)
        {
           #if JUCE_WINDOWS
            return f.getFullPathName().toWideCharPointer();
           #else
            return f.getFullPathName().toRawUTF8();
           #endif
        }

        juce::String str (const TagLib::String& s)
        {
            return juce::String::fromUTF8 (s.toCString (true)).trim();
        }

        juce::String firstProperty (const TagLib::PropertyMap& props, const char* key)
        {
            const auto it = props.find (key);
            if (it == props.end() || it->second.isEmpty())
                return {};
            return str (it->second.front());
        }
    }

    TrackTags TagReader::read (const juce::File& file)
    {
        TrackTags t;
        TagLib::FileRef ref (nameOf (file), true, TagLib::AudioProperties::Fast);
        if (ref.isNull())
            return t;
        t.valid = true;

        if (auto* tag = ref.tag())
        {
            t.title   = str (tag->title());
            t.artist  = str (tag->artist());
            t.album   = str (tag->album());
            t.genre   = str (tag->genre());
            t.comment = str (tag->comment());
            t.year    = (int) tag->year();
        }
        const auto props = ref.properties();
        t.bpm     = firstProperty (props, "BPM").getDoubleValue();
        t.keyText = firstProperty (props, "INITIALKEY");
        // Sin tags: "Artista - Título.mp3" (igual que el cargador de pistas).
        if (t.title.isEmpty())
        {
            juce::String a, ti;
            engine::TrackLoader::parseFileName (file, a, ti);
            t.title = ti;
            if (t.artist.isEmpty())
                t.artist = a;
        }

        if (auto* ap = ref.audioProperties())
        {
            t.durationSeconds = ap->lengthInMilliseconds() / 1000.0;
            t.bitrateKbps     = ap->bitrate();
            t.sampleRate      = ap->sampleRate();
        }
        return t;
    }

    juce::MemoryBlock TagReader::readEmbeddedArtwork (const juce::File& file)
    {
        TagLib::FileRef ref (nameOf (file), false);
        if (ref.isNull())
            return {};

        // TagLib 2: "PICTURE" funciona igual en ID3v2, FLAC, MP4, Ogg y APE.
        const auto pictures = ref.complexProperties ("PICTURE");
        const TagLib::VariantMap* best = nullptr;
        for (const auto& p : pictures)
        {
            const auto type = p.value ("pictureType").toString();
            if (best == nullptr || type == "Front Cover")
                best = &p;
            if (type == "Front Cover")
                break;
        }
        if (best == nullptr)
            return {};
        const auto data = best->value ("data").toByteVector();
        return juce::MemoryBlock (data.data(), data.size());
    }

    juce::File TagReader::findFolderArtwork (const juce::File& file)
    {
        const auto dir = file.getParentDirectory();
        for (const auto* name : { "cover", "folder", "front", "album", "artwork" })
            for (const auto* ext : { ".jpg", ".jpeg", ".png" })
            {
                // Búsqueda sin distinguir mayúsculas (Cover.JPG, Folder.jpg...).
                const auto wanted = juce::String (name) + ext;
                for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*" + juce::String (ext)))
                    if (f.getFileName().equalsIgnoreCase (wanted))
                        return f;
            }
        return {};
    }

    juce::Image TagReader::readArtwork (const juce::File& file, int maxSize)
    {
        juce::Image img;
        const auto data = readEmbeddedArtwork (file);
        if (data.getSize() > 0)
            img = juce::ImageFileFormat::loadFrom (data.getData(), data.getSize());
        if (! img.isValid())
        {
            const auto folder = findFolderArtwork (file);
            if (folder.existsAsFile())
                img = juce::ImageFileFormat::loadFrom (folder);
        }
        if (! img.isValid())
            return {};

        // Recorte cuadrado al centro y achicado de calidad.
        const auto side = juce::jmin (img.getWidth(), img.getHeight());
        img = img.getClippedImage ({ (img.getWidth() - side) / 2, (img.getHeight() - side) / 2, side, side });
        if (side > maxSize)
            img = img.rescaled (maxSize, maxSize, juce::Graphics::highResamplingQuality);
        return img.convertedToFormat (juce::Image::RGB);
    }

    bool TagReader::writeForTest (const juce::File& file, const TrackTags& tags, const juce::MemoryBlock& image)
    {
        TagLib::FileRef ref (nameOf (file), false);
        if (ref.isNull() || ref.tag() == nullptr)
            return false;
        ref.tag()->setTitle (TagLib::String (tags.title.toRawUTF8(), TagLib::String::UTF8));
        ref.tag()->setArtist (TagLib::String (tags.artist.toRawUTF8(), TagLib::String::UTF8));
        ref.tag()->setGenre (TagLib::String (tags.genre.toRawUTF8(), TagLib::String::UTF8));
        auto props = ref.properties();
        if (tags.bpm > 0.0)
            props.replace ("BPM", TagLib::StringList (TagLib::String (juce::String (juce::roundToInt (tags.bpm)).toRawUTF8())));
        if (tags.keyText.isNotEmpty())
            props.replace ("INITIALKEY", TagLib::StringList (TagLib::String (tags.keyText.toRawUTF8())));
        ref.setProperties (props);
        if (image.getSize() > 0)
        {
            TagLib::VariantMap pic;
            pic.insert ("data", TagLib::ByteVector ((const char*) image.getData(), (unsigned int) image.getSize()));
            pic.insert ("pictureType", TagLib::String ("Front Cover"));
            pic.insert ("mimeType", TagLib::String ("image/png"));
            ref.setComplexProperties ("PICTURE", { pic });
        }
        return ref.save();
    }
} // namespace kb::library
