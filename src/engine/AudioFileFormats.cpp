#include "engine/AudioFileFormats.h"

#if JUCE_WINDOWS
 #include "engine/formats/MediaFoundationReader.h"
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <objbase.h>
#endif

namespace kb::engine::formats
{
    namespace
    {
        bool isMediaFoundationExtension (const juce::String& ext)
        {
            return ext == "m4a" || ext == "aac" || ext == "mp4";
        }
    }

    juce::StringArray supportedExtensions()
    {
        juce::StringArray exts { "mp3", "wav", "aif", "aiff", "flac", "ogg" };
       #if JUCE_WINDOWS || JUCE_MAC || JUCE_IOS
        exts.addArray ({ "m4a", "aac", "mp4" });
       #endif
        return exts;
    }

    juce::String fileWildcard()
    {
        juce::StringArray parts;
        for (const auto& e : supportedExtensions())
            parts.add ("*." + e);
        return parts.joinIntoString (";");
    }

    bool isSupportedFile (const juce::File& file)
    {
        return supportedExtensions().contains (file.getFileExtension().trimCharactersAtStart (".").toLowerCase());
    }

    std::unique_ptr<juce::AudioFormatReader> createReader (const juce::File& file)
    {
        if (! file.existsAsFile())
            return nullptr;

        const auto ext = file.getFileExtension().trimCharactersAtStart (".").toLowerCase();

       #if JUCE_WINDOWS
        if (isMediaFoundationExtension (ext))
            return MediaFoundationReader::create (file);
       #else
        juce::ignoreUnused (isMediaFoundationExtension (ext));
       #endif

        juce::AudioFormatManager manager;
        manager.registerBasicFormats();   // WAV, AIFF, FLAC, Ogg, MP3 (+CoreAudio en Mac)
        return std::unique_ptr<juce::AudioFormatReader> (manager.createReaderFor (file));
    }

   #if JUCE_WINDOWS
    // Solo se balancea CoUninitialize si CoInitializeEx tuvo éxito en este hilo.
    static thread_local bool comInitialisedHere = false;
   #endif

    void initialiseDecodingThread()
    {
       #if JUCE_WINDOWS
        // MTA para Media Foundation. Si el hilo ya tenía COM en otro modo
        // (RPC_E_CHANGED_MODE) seguimos igual: MF funciona en ambos modelos.
        const auto hr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
        comInitialisedHere = SUCCEEDED (hr);
       #endif
    }

    void shutdownDecodingThread()
    {
       #if JUCE_WINDOWS
        if (comInitialisedHere)
        {
            CoUninitialize();
            comInitialisedHere = false;
        }
       #endif
    }
} // namespace kb::engine::formats
