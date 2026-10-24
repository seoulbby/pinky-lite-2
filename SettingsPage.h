// Pinky Lite 2 - in-plugin settings page
// Author: seoulbby
#pragma once
#include "Widgets.h"
#include "Visuals.h"

namespace pinky
{
class SettingsPage : public juce::Component, public Animated
{
public:
    static constexpr int panelW = 600, panelH = 440;

    SettingsPage (const Skin& s, PinkyProcessor& p, std::function<void()> closeCb)
        : skin (s), proc (p), onClose (std::move (closeCb))
    {
        auto& st = proc.settings;
        setVisible (false);
        setAlpha (0.0f);

        auto pills = [&] (juce::StringArray names, int initial, const char* key, int cols = -1)
        {
            auto sel = std::make_unique<PillSelector> (skin, names, cols < 0 ? names.size() : cols, initial,
                                                       [this, key] (int i) { proc.settings.set (key, i); });
            addAndMakeVisible (*sel);
            auto* raw = sel.get();
            owned.push_back (std::move (sel));
            animated.push_back (raw);
            return raw;
        };
        auto toggle = [&] (bool initial, const char* key)
        {
            auto t = std::make_unique<ToggleSwitch> (skin, initial, [this, key] (bool b) { proc.settings.set (key, b); });
            addAndMakeVisible (*t);
            auto* raw = t.get();
            toggles.push_back (std::move (t));
            animated.push_back (raw);
            return raw;
        };

        themePills  = pills ({ "Light", "Night" }, st.theme(), "theme");
        swatches    = std::make_unique<SwatchRow> (skin, st.accent(), [this] (int i) { proc.settings.set ("accent", i); });
        addAndMakeVisible (*swatches); animated.push_back (swatches.get());
        scalePills  = pills ({ "80%", "100%", "125%", "150%" }, st.uiScale(), "uiScale");
        animPills   = pills ({ "Off", "Light", "Full" }, st.animation(), "anim");
        feelPills   = pills ({ "Fast", "Normal", "Fine" }, st.knobFeel(), "feel");
        tipsToggle  = toggle (st.tooltips(), "tips");

        repeatToggle = toggle (st.repeatable(), "repeat");
        safeToggle   = toggle (st.safety(), "safety");

        closeBtn = std::make_unique<PinkButton> (skin, "Done", [this] { if (onClose) onClose(); }, true);
        addAndMakeVisible (*closeBtn); animated.push_back (closeBtn.get());
        resetBtn = std::make_unique<PinkButton> (skin, "Reset settings", [this] { proc.settings.resetToDefaults(); }, false);
        addAndMakeVisible (*resetBtn); animated.push_back (resetBtn.get());
    }

    void syncFromSettings()
    {
        auto& st = proc.settings;
        themePills->setIndex (st.theme());
        swatches->setIndex (st.accent());
        scalePills->setIndex (st.uiScale());
        animPills->setIndex (st.animation());
        feelPills->setIndex (st.knobFeel());
        tipsToggle->setState (st.tooltips());
        repeatToggle->setState (st.repeatable());
        safeToggle->setState (st.safety());
        repaint();
    }

    void setOpen (bool o)
    {
        open = o;
        if (o) { setVisible (true); toFront (false); }
    }
    bool isOpen() const { return open; }

    void tick (float dt) override
    {
        const float n = skin.approach (amt, open ? 1.0f : 0.0f, dt, 13.0f);
        if (std::abs (n - amt) > 0.0005f || (open && amt < 1.0f) || (! open && amt > 0.0f))
        {
            amt = n;
            if (! open && amt < 0.01f) { amt = 0.0f; setVisible (false); }
            setAlpha (amt);
            setTransform (juce::AffineTransform::translation (0.0f, (1.0f - amt) * 26.0f));
        }
        if (! isVisible()) return;
        for (auto* a : animated) a->tick (dt);
    }

    juce::Rectangle<int> panel() const { return juce::Rectangle<int> (panelW, panelH).withCentre (getLocalBounds().getCentre()); }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        g.fillAll ((p.night ? juce::Colour (0xff0b0410) : juce::Colour (0xff5c2146)).withAlpha (0.42f));

        auto r = panel().toFloat();
        juce::Path pth; pth.addRoundedRectangle (r, 26.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 30, { 0, 12 }).drawForPath (g, pth);
        g.setColour (p.night ? juce::Colour (0xff25102e) : juce::Colour (0xfffff6fb));
        g.fillPath (pth);
        g.setGradientFill (juce::ColourGradient (p.accent2.withAlpha (p.night ? 0.12f : 0.28f), r.getX(), r.getY(), p.accent.withAlpha (0.0f), r.getRight(), r.getBottom(), false));
        g.fillPath (pth);
        g.setColour (p.cardEdge);
        g.strokePath (pth, juce::PathStrokeType (1.4f));

        g.setColour (p.accent);
        g.fillPath (fourPointStar (r.getX() + 34.0f, r.getY() + 36.0f, 8.0f));
        g.setColour (p.text);
        g.setFont (skin.font (21.0f, true));
        g.drawText ("Settings", juce::Rectangle<float> (r.getX() + 52.0f, r.getY() + 20.0f, 200.0f, 32.0f), juce::Justification::centredLeft, false);

        auto section = [&] (const juce::String& t, float y)
        {
            g.setColour (p.accent);
            g.setFont (skin.font (11.0f, true));
            g.drawText (t, juce::Rectangle<float> (r.getX() + 30.0f, y, 300.0f, 16.0f), juce::Justification::centredLeft, false);
            g.setColour (p.accent.withAlpha (0.25f));
            g.drawLine (r.getX() + 30.0f + 90.0f, y + 8.0f, r.getRight() - 30.0f, y + 8.0f, 1.0f);
        };
        auto row = [&] (const juce::String& t, const juce::String& hint, float y)
        {
            g.setColour (p.text);
            g.setFont (skin.font (13.0f, true));
            g.drawText (t, juce::Rectangle<float> (r.getX() + 30.0f, y, 170.0f, 20.0f), juce::Justification::centredLeft, false);
            g.setColour (p.textDim);
            g.setFont (skin.font (10.0f, false));
            g.drawText (hint, juce::Rectangle<float> (r.getX() + 30.0f, y + 17.0f, 190.0f, 14.0f), juce::Justification::centredLeft, false);
        };

        const float ox = r.getX(), oy = r.getY();
        section ("APPEARANCE", oy + 68.0f);
        row ("Theme", "light day / night mode", oy + 94.0f);
        row ("Accent colour", "pick your pink", oy + 134.0f);
        row ("Window size", "or drag the corner", oy + 174.0f);
        row ("Animations", "full = glow + sparkles", oy + 214.0f);
        row ("Knob feel", "mouse drag sensitivity", oy + 254.0f);
        row ("Tooltips", "hover hints", oy + 294.0f);
        section ("ENGINE", oy + 336.0f);
        row ("Repeatable glitches", "same pattern every loop", oy + 360.0f);
        row ("Safety limiter", "soft-clips above 0 dBFS", oy + 400.0f);
        juce::ignoreUnused (ox);

        g.setColour (p.textDim);
        g.setFont (skin.font (10.5f, false));
        g.drawText ("Pinky Lite 2  v" + juce::String (JucePlugin_VersionString) + "   -   made with love by seoulbby   -   built with JUCE",
                    juce::Rectangle<float> (r.getX(), r.getBottom() - 24.0f, r.getWidth(), 16.0f), juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = panel();
        const int cx = r.getX() + 236, cw = r.getWidth() - 236 - 30, oy = r.getY();
        themePills->setBounds (cx, oy + 94, 200, 28);
        swatches->setBounds (cx + 2, oy + 134, 4 * 26 + 3 * 10, 26);
        scalePills->setBounds (cx, oy + 174, 300, 28);
        animPills->setBounds (cx, oy + 214, 240, 28);
        feelPills->setBounds (cx, oy + 254, 240, 28);
        tipsToggle->setBounds (cx, oy + 296, 48, 26);
        repeatToggle->setBounds (cx, oy + 362, 48, 26);
        safeToggle->setBounds (cx, oy + 402, 48, 26);
        closeBtn->setBounds (r.getRight() - 30 - 96, oy + 20, 96, 30);
        resetBtn->setBounds (r.getRight() - 30 - 130, oy + 380, 130, 28);
        juce::ignoreUnused (cw);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panel().contains (e.getPosition()) && onClose) onClose();
    }

    const Skin& skin;

private:
    PinkyProcessor& proc;
    std::function<void()> onClose;
    bool open = false;
    float amt = 0.0f;

    std::vector<std::unique_ptr<PillSelector>> owned;
    std::vector<std::unique_ptr<ToggleSwitch>> toggles;
    std::vector<Animated*> animated;
    PillSelector *themePills = nullptr, *scalePills = nullptr, *animPills = nullptr, *feelPills = nullptr;
    ToggleSwitch *tipsToggle = nullptr, *repeatToggle = nullptr, *safeToggle = nullptr;
    std::unique_ptr<SwatchRow> swatches;
    std::unique_ptr<PinkButton> closeBtn, resetBtn;
};
} // namespace pinky
