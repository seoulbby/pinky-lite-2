// Author: seoulbby
#include "Settings.h"

namespace pinky
{
static const char* kKeys[] = { "theme", "accent", "uiScale", "anim", "feel", "tips", "repeat", "safety" };

static void fillDefaults (juce::ValueTree& t)
{
    t.setProperty ("theme", 0, nullptr);
    t.setProperty ("accent", 0, nullptr);
    t.setProperty ("uiScale", 1, nullptr);
    t.setProperty ("anim", 2, nullptr);
    t.setProperty ("feel", 1, nullptr);
    t.setProperty ("tips", true, nullptr);
    t.setProperty ("repeat", true, nullptr);
    t.setProperty ("safety", true, nullptr);
}

PinkySettings::PinkySettings()
{
    fillDefaults (tree);
    loadGlobal();
    applyFlags();
}

juce::File PinkySettings::globalFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("seoulbby").getChildFile ("PinkyLite2").getChildFile ("settings.xml");
}

void PinkySettings::loadGlobal()
{
    auto f = globalFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (f))
        for (auto* k : kKeys)
            if (xml->hasAttribute (k))
                tree.setProperty (k, xml->getIntAttribute (k), nullptr);
}

void PinkySettings::saveGlobal()
{
    juce::XmlElement xml ("PinkyLite2Settings");
    for (auto* k : kKeys)
        xml.setAttribute (k, (int) tree.getProperty (k));

    auto f = globalFile();
    f.getParentDirectory().createDirectory();
    xml.writeTo (f);
}

void PinkySettings::applyFlags()
{
    repeatFlag = (bool) tree.getProperty ("repeat", true);
    safetyFlag   = (bool) tree.getProperty ("safety", true);
}

void PinkySettings::set (const juce::Identifier& key, const juce::var& v)
{
    tree.setProperty (key, v, nullptr);
    applyFlags();
    saveGlobal();
    sendChangeMessage();
}

void PinkySettings::resetToDefaults()
{
    fillDefaults (tree);
    applyFlags();
    saveGlobal();
    sendChangeMessage();
}

void PinkySettings::loadFrom (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    for (auto* k : kKeys)
        if (t.hasProperty (k))
            tree.setProperty (k, t.getProperty (k), nullptr);
    applyFlags();
    sendChangeMessage();
}
} // namespace pinky
