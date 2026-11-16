#include "app/Keymap.h"

namespace kb::app
{
    juce::String Keymap::keyText (const juce::KeyPress& k)
    {
        // Letras siempre en minúscula (la misma tecla con o sin Bloq Mayús).
        return juce::KeyPress (juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getKeyCode()), k.getModifiers()
                                   .withoutFlags (juce::ModifierKeys::allMouseButtonModifiers), 0).getTextDescription();
    }

    void Keymap::resetToDefaults()
    {
        keys.clear();
        auto add = [this] (int code, const juce::String& target, juce::ModifierKeys mods = {})
        {
            keys[keyText (juce::KeyPress (code, mods, 0))] = target;
        };
        add ('q', "deck1.playPause");  add ('w', "deck2.playPause");
        add ('a', "deck1.cue");        add ('s', "deck2.cue");
        add ('z', "deck1.sync");       add ('x', "deck2.sync");
        add ('e', "deck1.keyLock");    add ('r', "deck2.keyLock");
        add ('1', "deck1.pad1"); add ('2', "deck1.pad2"); add ('3', "deck1.pad3"); add ('4', "deck1.pad4");
        add ('7', "deck2.pad1"); add ('8', "deck2.pad2"); add ('9', "deck2.pad3"); add ('0', "deck2.pad4");
        add ('b', "library.largeView", juce::ModifierKeys::commandModifier);
    }

    juce::String Keymap::targetFor (const juce::KeyPress& key) const
    {
        const auto it = keys.find (keyText (key));
        return it != keys.end() ? it->second : juce::String();
    }

    juce::KeyPress Keymap::keyFor (const juce::String& target) const
    {
        for (const auto& [k, t] : keys)
            if (t == target)
                return juce::KeyPress::createFromDescription (k);
        return {};
    }

    void Keymap::assign (const juce::KeyPress& key, const juce::String& target)
    {
        removeTarget (target);
        keys[keyText (key)] = target;
    }

    void Keymap::removeTarget (const juce::String& target)
    {
        for (auto it = keys.begin(); it != keys.end();)
            it = it->second == target ? keys.erase (it) : std::next (it);
    }

    juce::String Keymap::toJson() const
    {
        auto* o = new juce::DynamicObject();
        for (const auto& [k, t] : keys)
            o->setProperty (k, t);
        return juce::JSON::toString (juce::var (o), true);
    }

    bool Keymap::fromJson (const juce::String& json)
    {
        const auto v = juce::JSON::parse (json);
        const auto* o = v.getDynamicObject();
        if (o == nullptr)
            return false;
        keys.clear();
        for (const auto& p : o->getProperties())
            keys[p.name.toString()] = p.value.toString();
        return true;
    }
} // namespace kb::app
