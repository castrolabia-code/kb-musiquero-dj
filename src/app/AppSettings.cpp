#include "app/AppSettings.h"

namespace kb::app
{
    namespace keys
    {
        static constexpr const char* language     = "ui.language";
        static constexpr const char* windowState  = "ui.mainWindowState";
        static constexpr const char* audioDevice  = "audio.deviceState";
        static constexpr const char* settingsVer  = "settings.version";
    }

    // Subir este número si cambia el formato de los ajustes (para migraciones).
    static constexpr int kSettingsVersion = 1;

    static juce::String companyFolder() { return "Castro Labs"; }
    static juce::String productFolder() { return "KB Musiquero DJ"; }

    juce::File AppSettings::getAppDataDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile (companyFolder())
                       .getChildFile (productFolder());
        dir.createDirectory();
        return dir;
    }

    AppSettings::AppSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = "settings";
        options.filenameSuffix      = ".xml";
        options.folderName          = companyFolder() + juce::File::getSeparatorString() + productFolder();
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat       = juce::PropertiesFile::storeAsXML;
        options.commonToAllUsers    = false;
        options.millisecondsBeforeSaving = 1500;
        properties.setStorageParameters (options);

        file().setValue (keys::settingsVer, kSettingsVersion);
    }

    AppSettings::~AppSettings()
    {
        flush();
    }

    juce::PropertiesFile& AppSettings::file() const
    {
        auto* f = properties.getUserSettings();
        jassert (f != nullptr);
        return *f;
    }

    std::optional<Language> AppSettings::getLanguage() const
    {
        return I18n::fromCode (file().getValue (keys::language));
    }

    void AppSettings::setLanguage (Language language)
    {
        file().setValue (keys::language, I18n::toCode (language));
    }

    juce::String AppSettings::getMainWindowState() const
    {
        return file().getValue (keys::windowState);
    }

    void AppSettings::setMainWindowState (const juce::String& state)
    {
        file().setValue (keys::windowState, state);
    }

    std::unique_ptr<juce::XmlElement> AppSettings::getAudioDeviceState() const
    {
        return file().getXmlValue (keys::audioDevice);
    }

    void AppSettings::setAudioDeviceState (const juce::XmlElement* state)
    {
        if (state != nullptr)
            file().setValue (keys::audioDevice, state);
        else
            file().removeValue (keys::audioDevice);
    }

    bool AppSettings::flush()
    {
        return properties.saveIfNeeded();
    }

    juce::File AppSettings::getSettingsFile() const
    {
        return file().getFile();
    }
} // namespace kb::app
