// Pinky Lite 2 - non-automatable plugin settings (UI + engine options)
// Stored inside the project/plugin state and mirrored to a per-user file
// so every new instance starts with your favourite look.
// Author: seoulbby
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>

namespace pinky
{
class PinkySettings : public juce::ChangeBroadcaster
{
public:
    PinkySettings();

    // indices
    int  theme() const        { return (int) tree.getProperty ("theme", 0); }      // 0 light, 1 night
    int  accent() const       { return (int) tree.getProperty ("accent", 0); }     // 0..3
    int  uiScale() const      { return (int) tree.getProperty ("uiScale", 1); }    // 0..3
    int  animation() const    { return (int) tree.getProperty ("anim", 2); }       // 0 off, 1 light, 2 full
    int  knobFeel() const     { return (int) tree.getProperty ("feel", 1); }       // 0 fast, 1 normal, 2 fine
    bool tooltips() const     { return (bool) tree.getProperty ("tips", true); }
    bool repeatable() const   { return repeatFlag.load(); }
    bool safety() const       { return safetyFlag.load(); }

    static float scaleFor (int idx) { static constexpr float s[] { 0.8f, 1.0f, 1.25f, 1.5f }; return s[juce::jlimit (0, 3, idx)]; }
    static int   dragPixelsFor (int feel) { static constexpr int d[] { 120, 200, 420 }; return d[juce::jlimit (0, 2, feel)]; }

    void set (const juce::Identifier& key, const juce::var& v);
    void resetToDefaults();
    void loadFrom (const juce::ValueTree& t);

    juce::ValueTree tree { "SETTINGS" };

private:
    void applyFlags();
    void saveGlobal();
    void loadGlobal();
    juce::File globalFile() const;

    std::atomic<bool> repeatFlag { true }, safetyFlag { true };
};
} // namespace pinky
