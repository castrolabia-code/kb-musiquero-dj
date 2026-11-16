#include "app/CrashReporter.h"

namespace kb::app::crash
{
    namespace
    {
        juce::File folder;
        juce::String header;

        void onCrash (void*)
        {
            // Dentro de un cierre de emergencia: lo mínimo y sin depender de la UI.
            const auto f = folder.getChildFile ("crash-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S") + ".txt");
            juce::String report;
            report << header << "\n"
                   << "Hora: " << juce::Time::getCurrentTime().toString (true, true) << "\n\n"
                   << "Pila de llamadas:\n" << juce::SystemStats::getStackBacktrace() << "\n";
            const auto log = folder.getChildFile ("kb-musiquero-dj.log");
            if (log.existsAsFile())
                report << "\nÚltimas líneas del log:\n" << log.loadFileAsString().getLastCharacters (8000);
            f.replaceWithText (report);
        }
    }

    void install (const juce::File& logsFolder)
    {
        folder = logsFolder;
        folder.createDirectory();
        header << "KB - MUSIQUERO DJ " << KB_VERSION_STRING << "\n"
               << juce::SystemStats::getOperatingSystemName() << " · " << juce::SystemStats::getCpuModel()
               << " · " << juce::SystemStats::getMemorySizeInMegabytes() << " MB";
        juce::SystemStats::setApplicationCrashHandler (onCrash);
    }

    juce::Array<juce::File> newReports (const juce::File& logsFolder, juce::int64 lastSeenMs)
    {
        juce::Array<juce::File> out;
        for (const auto& f : logsFolder.findChildFiles (juce::File::findFiles, false, "crash-*.txt"))
            if (f.getLastModificationTime().toMilliseconds() > lastSeenMs)
                out.add (f);
        return out;
    }

    juce::String supportEmail()
    {
        return {};   // completar con el correo de soporte de Castro Labs antes de vender
    }
} // namespace kb::app::crash
