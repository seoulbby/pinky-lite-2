// Pinky Lite 2 - editor
// Author: seoulbby
#pragma once
#include "PluginProcessor.h"

class PinkyEditor : public juce::AudioProcessorEditor,
                    private juce::Timer,
                    private juce::ChangeListener
{
public:
    explicit PinkyEditor (PinkyProcessor&);
    ~PinkyEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    void setSettingsOpen (bool open);

    // test helper: run the animation clock without a message loop
    void advanceForTest (float seconds, int steps);

private:
    struct Content;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applySettings (bool resizeWindow);

    PinkyProcessor& proc;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltipWindow { this, 650 };
    double lastTick = 0.0;
    int lastScaleIdx = -1;
};
