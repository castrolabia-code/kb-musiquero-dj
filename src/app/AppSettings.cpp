#include "app/AppSettings.h"

namespace kb::app
{
    namespace keys
    {
        static constexpr const char* language     = "ui.language";
        static constexpr const char* windowState  = "ui.mainWindowState";
        static constexpr const char* audioDevice  = "audio.deviceState";
        static constexpr const char* settingsVer  = "settings.version";
        static constexpr const char* xfCurve      = "mixer.crossfaderCurve";
        static constexpr const char* xfReverse    = "mixer.crossfaderReverse";
        static constexpr const char* masterPair   = "routing.masterOutputPair";
        static constexpr const char* split        = "routing.split";
        static constexpr const char* autoMixSecs  = "mixer.autoMixSeconds";
        static constexpr const char* sampler      = "sampler.slots";
        static constexpr const char* fxState      = "fx.units";
        static constexpr const char* libColumns   = "library.columns";
        static constexpr const char* largeLibrary = "ui.largeLibrary";
        static constexpr const char* useOpenGL    = "ui.openGL";
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

    int  AppSettings::getCrossfaderCurve() const        { return juce::jlimit (0, 2, file().getIntValue (keys::xfCurve, 0)); }
    void AppSettings::setCrossfaderCurve (int c)        { file().setValue (keys::xfCurve, juce::jlimit (0, 2, c)); }
    bool AppSettings::getCrossfaderReverse() const      { return file().getBoolValue (keys::xfReverse, false); }
    void AppSettings::setCrossfaderReverse (bool r)     { file().setValue (keys::xfReverse, r); }
    int  AppSettings::getMasterOutputPair() const       { return juce::jlimit (0, 1, file().getIntValue (keys::masterPair, 0)); }
    void AppSettings::setMasterOutputPair (int p)       { file().setValue (keys::masterPair, juce::jlimit (0, 1, p)); }
    bool AppSettings::getSplit() const                  { return file().getBoolValue (keys::split, false); }
    void AppSettings::setSplit (bool s)                 { file().setValue (keys::split, s); }

    double AppSettings::getAutoMixSeconds() const       { return juce::jlimit (2.0, 60.0, file().getDoubleValue (keys::autoMixSecs, 10.0)); }
    void AppSettings::setAutoMixSeconds (double s)      { file().setValue (keys::autoMixSecs, s); }

    juce::String AppSettings::getSamplerState() const       { return file().getValue (keys::sampler, {}); }
    void AppSettings::setSamplerState (const juce::String& j) { file().setValue (keys::sampler, j); }

    juce::String AppSettings::getFxState() const       { return file().getValue (keys::fxState, {}); }
    void AppSettings::setFxState (const juce::String& j) { file().setValue (keys::fxState, j); }

    juce::String AppSettings::getLibraryColumns() const       { return file().getValue (keys::libColumns, {}); }
    void AppSettings::setLibraryColumns (const juce::String& s) { file().setValue (keys::libColumns, s); }

    bool AppSettings::getLargeLibrary() const   { return file().getBoolValue (keys::largeLibrary, false); }
    void AppSettings::setLargeLibrary (bool on) { file().setValue (keys::largeLibrary, on); }
   #if JUCE_WINDOWS
    bool AppSettings::getUseOpenGL() const      { return file().getBoolValue (keys::useOpenGL, true); }
   #else
    bool AppSettings::getUseOpenGL() const      { return file().getBoolValue (keys::useOpenGL, false); }
   #endif
    void AppSettings::setUseOpenGL (bool on)    { file().setValue (keys::useOpenGL, on); }

    bool AppSettings::flush()
    {
        return properties.saveIfNeeded();
    }

    juce::File AppSettings::getSettingsFile() const
    {
        return file().getFile();
    }
} // namespace kb::app
