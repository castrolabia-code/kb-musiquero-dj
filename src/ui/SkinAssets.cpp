#include "ui/SkinAssets.h"

namespace kb::ui
{
    SkinAssets& SkinAssets::get()
    {
        static SkinAssets instance;
        return instance;
    }

    juce::Array<juce::File> SkinAssets::folders() const
    {
        juce::Array<juce::File> out;
        out.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getChildFile ("skin"));
        out.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                     .getChildFile ("Castro Labs").getChildFile ("KB Musiquero DJ").getChildFile ("skin"));
        return out;
    }

    juce::Image SkinAssets::find (const juce::String& name, float pixelsNeeded)
    {
        // De la más chica a la más grande: la primera que alcance (o la más grande que haya).
        juce::Image best;
        for (const auto* suffix : { "", "@2x", "@4x" })
        {
            const auto key = name + suffix;
            auto it = cache.find (key);
            if (it == cache.end())
            {
                juce::Image img;
                for (const auto& dir : folders())
                {
                    const auto f = dir.getChildFile (key + ".png");
                    if (f.existsAsFile())
                    {
                        img = juce::ImageFileFormat::loadFrom (f);
                        break;
                    }
                }
                it = cache.emplace (key, img).first;
            }
            if (it->second.isValid())
            {
                best = it->second;
                if ((float) best.getWidth() >= pixelsNeeded)
                    break;
            }
        }
        return best;
    }
} // namespace kb::ui
