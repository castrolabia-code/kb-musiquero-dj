#include "app/PlatformFiles.h"

#include "app/AppSettings.h"

namespace kb::app::platform
{
    juce::File importedFolder()
    {
        return AppSettings::getAppDataDirectory().getChildFile ("Importadas");
    }

    namespace
    {
        // Nombre de archivo legible para un content:// (si el sistema lo informa).
        juce::String displayName (const juce::URL& url)
        {
           #if JUCE_ANDROID
            const auto name = juce::AndroidDocument::fromDocument (url).getInfo().getName();
            if (name.isNotEmpty())
                return juce::File::createLegalFileName (name);
           #endif
            const auto last = juce::URL::removeEscapeChars (url.toString (false)).fromLastOccurrenceOf ("/", false, false);
            return juce::File::createLegalFileName (last.isNotEmpty() ? last : juce::String ("cancion"));
        }

        juce::File copyToImported (const juce::URL& url)
        {
            std::unique_ptr<juce::InputStream> in;
           #if JUCE_ANDROID
            in = juce::AndroidDocument::fromDocument (url).createInputStream();
           #endif
            if (in == nullptr)
                in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
            if (in == nullptr)
                return {};

            auto dir = importedFolder();
            dir.createDirectory();
            const auto name = displayName (url);
            auto target = dir.getChildFile (name);
            // Si ya se importó (mismo nombre y tamaño), se reutiliza.
            const auto size = in->getTotalLength();
            if (target.existsAsFile() && size > 0 && target.getSize() == size)
                return target;
            target = target.getNonexistentSibling (false);

            juce::FileOutputStream out (target);
            if (! out.openedOk() || out.writeFromInputStream (*in, -1) <= 0)
            {
                target.deleteFile();
                return {};
            }
            out.flush();
            return target;
        }
    }

    void resolve (const juce::Array<juce::URL>& urls, std::function<void (juce::Array<juce::File>)> done)
    {
        juce::Array<juce::File> local;
        juce::Array<juce::URL> remote;
        for (const auto& u : urls)
        {
            if (u.isLocalFile() && u.getLocalFile().exists())
                local.add (u.getLocalFile());
            else if (! u.isEmpty())
                remote.add (u);
        }

        if (remote.isEmpty())
        {
            if (done) done (local);
            return;
        }

        // Copiar puede tardar (archivos grandes, nube): hilo aparte.
        juce::Thread::launch ([local, remote, done]
        {
            auto files = local;
            for (const auto& u : remote)
            {
                const auto f = copyToImported (u);
                if (f.existsAsFile())
                    files.add (f);
            }
            juce::MessageManager::callAsync ([files, done] { if (done) done (files); });
        });
    }

    void requestMusicAccess (std::function<void (bool)> done)
    {
       #if JUCE_ANDROID
        juce::RuntimePermissions::request (juce::RuntimePermissions::readMediaAudio,
                                           [done] (bool granted) { if (done) done (granted); });
       #else
        if (done) done (true);
       #endif
    }

    juce::Array<juce::File> deviceMusicFolders()
    {
        juce::Array<juce::File> result;
       #if JUCE_ANDROID
        const juce::File root ("/storage/emulated/0");
        for (const auto* name : { "Music", "Download", "Downloads", "Documents", "Audio" })
        {
            const auto dir = root.getChildFile (name);
            if (dir.isDirectory())
                result.add (dir);
        }
        // Tarjeta SD / pendrive: /storage/XXXX-XXXX/Music
        for (const auto& entry : juce::RangedDirectoryIterator (juce::File ("/storage"), false, "*", juce::File::findDirectories))
        {
            const auto vol = entry.getFile();
            if (vol.getFileName() == "emulated" || vol.getFileName() == "self")
                continue;
            if (vol.getChildFile ("Music").isDirectory())
                result.add (vol.getChildFile ("Music"));
        }
        if (importedFolder().isDirectory())
            result.add (importedFolder());
       #else
        const auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (music.isDirectory())
            result.add (music);
       #endif
        return result;
    }
} // namespace kb::app::platform
