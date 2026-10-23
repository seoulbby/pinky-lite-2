// Author: seoulbby
#include "PluginEditor.h"
#include <map>
#include <functional>
#include "Skin.h"
#include "Widgets.h"
#include "Visuals.h"
#include "SettingsPage.h"

using namespace pinky;

namespace
{
// ---------------------------------------------------------------- title badge
class TitleBadge : public juce::Component
{
public:
    explicit TitleBadge (const Skin& s) : skin (s) { setInterceptsMouseClicks (false, false); setBufferedToImage (true); }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        juce::GlyphArrangement ga;
        ga.addLineOfText (skin.font (31.0f, true), "Pinky Lite", 0.0f, 0.0f);
        juce::Path text;
        ga.createPath (text);
        auto tb = text.getBounds();
        text.applyTransform (juce::AffineTransform::translation (22.0f - tb.getX(), 36.0f));
        juce::DropShadow (p.accent.withAlpha (p.night ? 0.5f : 0.35f), 10, { 0, 3 }).drawForPath (g, text);
        g.setGradientFill (juce::ColourGradient (p.accent, 22.0f, 8.0f, p.accent2, tb.getWidth() + 22.0f, 40.0f, false));
        g.fillPath (text);

        // "2" badge
        auto badge = juce::Rectangle<float> (tb.getWidth() + 32.0f, 12.0f, 26.0f, 26.0f);
        g.setGradientFill (juce::ColourGradient (p.accent2, badge.getX(), badge.getY(), p.accent, badge.getRight(), badge.getBottom(), false));
        g.fillEllipse (badge);
        g.setColour (juce::Colours::white);
        g.setFont (skin.font (15.0f, true));
        g.drawText ("2", badge, juce::Justification::centred, false);

        // credit line with a tiny heart
        g.setColour (p.textDim);
        g.setFont (skin.font (11.5f, true));
        g.drawText ("by seoulbby", juce::Rectangle<float> (24.0f, 44.0f, 100.0f, 16.0f), juce::Justification::centredLeft, false);
        juce::Path heart;
        const float hx = 98.0f, hy = 52.0f, s = 4.2f;
        heart.startNewSubPath (hx, hy + s * 1.1f);
        heart.cubicTo (hx - s * 2.2f, hy - s * 0.2f, hx - s * 1.0f, hy - s * 1.8f, hx, hy - s * 0.5f);
        heart.cubicTo (hx + s * 1.0f, hy - s * 1.8f, hx + s * 2.2f, hy - s * 0.2f, hx, hy + s * 1.1f);
        g.setColour (p.accent);
        g.fillPath (heart);
    }
    const Skin& skin;
};

// ---------------------------------------------------------------- preset bar
class PresetBar : public juce::Component, public Animated
{
public:
    PresetBar (const Skin& s, PinkyProcessor& p) : skin (s), proc (p) {}

    void tick (float dt) override
    {
        const int cur = proc.getCurrentProgram();
        bool ch = cur != shown;
        shown = cur;
        for (int i = 0; i < 3; ++i)
        {
            const float n = skin.approach (hov[i], hovered == i ? 1.0f : 0.0f, dt, 15.0f);
            if (std::abs (n - hov[i]) > 0.004f) ch = true;
            hov[i] = n;
        }
        if (ch) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (p.panel);
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (p.accent.withAlpha (0.3f));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.2f);

        auto left = b.removeFromLeft (b.getHeight()), right = b.removeFromRight (b.getHeight());
        auto arrow = [&] (juce::Rectangle<float> r, int idx, bool isLeft)
        {
            g.setColour (p.accent.withAlpha (0.15f * hov[idx]));
            g.fillEllipse (r.reduced (3.0f));
            g.setColour (p.accent.interpolatedWith (p.text, 0.0f));
            juce::Path a;
            const float cx = r.getCentreX(), cy = r.getCentreY(), d = isLeft ? -1.0f : 1.0f;
            a.startNewSubPath (cx - 3.0f * d, cy - 5.0f); a.lineTo (cx + 3.0f * d, cy); a.lineTo (cx - 3.0f * d, cy + 5.0f);
            g.strokePath (a, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        arrow (left, 0, true);
        arrow (right, 2, false);

        g.setColour (hov[1] > 0.3f ? p.accent : p.text);
        g.setFont (skin.font (13.5f, true));
        g.drawText (proc.getProgramName (shown), b, juce::Justification::centred, false);
        g.setColour (p.textDim);
        g.setFont (skin.font (9.5f, true));
        g.drawText (juce::String (shown + 1) + "/" + juce::String (proc.getNumPrograms()), b.reduced (10.0f, 0.0f), juce::Justification::centredRight, false);
    }

    void mouseMove (const juce::MouseEvent& e) override { hovered = zone (e.x); }
    void mouseExit (const juce::MouseEvent&) override   { hovered = -1; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = proc.getNumPrograms(), z = zone (e.x);
        if (z == 0) proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n);
        else if (z == 2) proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n);
        else
        {
            juce::PopupMenu m;
            for (int i = 0; i < n; ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                             [this] (int r) { if (r > 0) proc.setCurrentProgram (r - 1); });
        }
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + (w.deltaY > 0 ? n - 1 : 1)) % n);
    }
    const Skin& skin;
private:
    int zone (int x) const { return x < getHeight() ? 0 : (x > getWidth() - getHeight() ? 2 : 1); }
    PinkyProcessor& proc;
    int shown = -1, hovered = -1;
    float hov[3] {};
};

const char* tooltipFor (const juce::String& id)
{
    static const std::map<juce::String, const char*> t = {
        { id::inGain, "Input gain into the distortion." },
        { id::drive, "How hard the signal hits the shaper." },
        { id::shape, "Character of the selected mode (knee, fold depth, asymmetry...)." },
        { id::bias, "Offsets the signal for asymmetric, even-harmonic grit." },
        { id::tone, "Tilt EQ after the distortion: left = darker, right = brighter." },
        { id::xover, "Where the two distortion bands split." },
        { id::lowAmt, "How much drive the low band gets." },
        { id::highAmt, "How much drive the high band gets." },
        { id::bits, "Bit-crusher: fewer bits = crunchier." },
        { id::rate, "Sample-rate reducer: higher = more aliasing." },
        { id::cutoff, "Filter cutoff frequency." },
        { id::reso, "Filter resonance." },
        { id::lfoRate, "LFO speed in Hz (used when Sync is Free)." },
        { id::lfoDrive, "How much the LFO moves the drive. Negative inverts." },
        { id::lfoCut, "How much the LFO moves the filter cutoff." },
        { id::lfoCrush, "How much the LFO moves the crusher." },
        { id::width, "Stereo width: 0 = mono, 100 = original, 200 = extra wide." },
        { id::mix, "Dry / wet balance." },
        { id::outGain, "Output level." } };
    auto it = t.find (id);
    return it != t.end() ? it->second : "";
}
} // namespace

// ============================================================================ Content
struct PinkyEditor::Content : public juce::Component
{
    explicit Content (PinkyProcessor& p)
        : proc (p)
    {
        setOpaque (true);
        auto& a = proc.apvts;

        bg = &add<BackgroundLayer> (skin);
        bg->setBounds (0, 0, Layout::W, Layout::H);
        add<TitleBadge> (skin).setBounds (0, 4, 330, 66);

        presetBar = &add<PresetBar> (skin, proc);
        presetBar->setBounds (292, 18, 280, 36);

        inBar = &add<LevelBar> (skin, proc.inPeak, "IN");
        inBar->setBounds (598, 20, 150, 14);
        outBar = &add<LevelBar> (skin, proc.outPeak, "OUT");
        outBar->setBounds (598, 40, 150, 14);

        gear = &add<GearButton> (skin, [this] { if (toggleSettingsCb) toggleSettingsCb(); });
        gear->setBounds (836, 15, 40, 40);

        // ---- Distortion card
        auto& modePills = add<PillSelector> (skin, a, id::mode, juce::StringArray (pinky::modeNames, (int) pinky::NumModes), 8);
        modePills.setBounds (30, 108, 546, 28);
        shaperView = &add<ShaperView> (skin, proc);
        shaperView->setBounds (30, 146, 196, 184);
        knob (id::drive, "Drive", 236, 146, 130, 184, 98, false);
        knob (id::shape, "Shape", 366, 176, 68, 130, 58, false);
        knob (id::bias, "Bias", 436, 176, 68, 130, 58, true);
        knob (id::tone, "Tone", 506, 176, 68, 130, 58, true);

        // ---- Bands card
        add<BandView> (skin, proc).setBounds (612, 108, 256, 96);
        knob (id::xover, "Xover", 610, 212, 86, 120, 60, false);
        knob (id::lowAmt, "Low", 696, 212, 86, 120, 60, false);
        knob (id::highAmt, "High", 782, 212, 86, 120, 60, false);

        // ---- Crush card
        add<CrushView> (skin, proc).setBounds (30, 380, 160, 92);
        knob (id::bits, "Bits", 30, 480, 80, 122, 58, false);
        knob (id::rate, "Rate", 108, 480, 80, 122, 58, false);

        // ---- Filter card
        add<PillSelector> (skin, a, id::fType, juce::StringArray { "Off", "LP", "HP", "BP" }, 4).setBounds (226, 378, 196, 26);
        add<FilterView> (skin, proc).setBounds (226, 412, 196, 84);
        knob (id::cutoff, "Cutoff", 226, 500, 98, 104, 52, false);
        knob (id::reso, "Reso", 324, 500, 98, 104, 52, false);

        // ---- Motion card
        add<StepSelector> (skin, a, id::lfoSync).setBounds (598, 354, 128, 22);
        add<LfoView> (skin, proc).setBounds (458, 382, 268, 74);
        auto& shapePills = add<PillSelector> (skin, a, id::lfoShape, juce::StringArray (pinky::lfoShapeNames, (int) pinky::NumLfoShapes), 6);
        shapePills.setBounds (458, 462, 268, 28);
        shapePills.drawIcon = [] (juce::Graphics& g, juce::Rectangle<float> r, int i, juce::Colour c) { drawLfoIcon (g, r, i, c); };
        knob (id::lfoRate, "Rate", 458, 494, 67, 110, 48, false);
        knob (id::lfoDrive, "Drive", 525, 494, 67, 110, 48, true);
        knob (id::lfoCut, "Cutoff", 592, 494, 67, 110, 48, true);
        knob (id::lfoCrush, "Crush", 659, 494, 67, 110, 48, true);

        // ---- Output card
        knob (id::inGain, "Input", 762, 380, 58, 108, 46, true);
        knob (id::outGain, "Output", 818, 380, 58, 108, 46, true);
        knob (id::width, "Width", 762, 492, 58, 108, 46, false);
        knob (id::mix, "Mix", 818, 492, 58, 108, 46, false);

        settingsPage = std::make_unique<SettingsPage> (skin, proc, [this] { if (toggleSettingsCb) toggleSettingsCb(); });
        addChildComponent (*settingsPage);
        settingsPage->setBounds (0, 0, Layout::W, Layout::H);
        animated.push_back (settingsPage.get());
    }

    template <class T, class... Args>
    T& add (Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto* raw = c.get();
        addAndMakeVisible (*raw);
        if (auto* an = dynamic_cast<Animated*> (raw)) animated.push_back (an);
        owned.push_back (std::move (c));
        return *raw;
    }

    KnobBox& knob (const char* pid, const char* label, int x, int y, int w, int h, float dia, bool bipolar)
    {
        auto& k = add<KnobBox> (skin, proc.apvts, pid, label, dia, bipolar);
        k.setBounds (x, y, w, h);
        k.setComponentID (pid);
        k.setTooltip (tooltipFor (pid));
        knobs.push_back (&k);
        return k;
    }

    void paint (juce::Graphics& g) override { g.fillAll (skin.pal.bgMid); }

    void refreshSkin()
    {
        bg->skinChanged();
        for (auto* k : knobs) { k->applySkin(); k->setTooltip (proc.settings.tooltips() ? tooltipFor (k->getComponentID()) : ""); }
        for (auto& c : owned) c->repaint();
        settingsPage->repaint();
        repaint();
    }

    void tickAll (float dt)
    {
        for (auto* an : animated) an->tick (dt);
    }

    PinkyProcessor& proc;
    Skin skin;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Animated*> animated;
    std::vector<KnobBox*> knobs;
    BackgroundLayer* bg = nullptr;
    PresetBar* presetBar = nullptr;
    LevelBar *inBar = nullptr, *outBar = nullptr;
    GearButton* gear = nullptr;
    ShaperView* shaperView = nullptr;
    std::unique_ptr<SettingsPage> settingsPage;
    std::function<void()> toggleSettingsCb;
};

// ============================================================================ Editor
PinkyEditor::PinkyEditor (PinkyProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    content = std::make_unique<Content> (proc);
    content->toggleSettingsCb = [this] { setSettingsOpen (! content->settingsPage->isOpen()); };
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (Layout::W * 7 / 10, Layout::H * 7 / 10, Layout::W * 3 / 2, Layout::H * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) Layout::W / (double) Layout::H);

    proc.settings.addChangeListener (this);
    applySettings (true);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

PinkyEditor::~PinkyEditor()
{
    proc.settings.removeChangeListener (this);
    stopTimer();
}

void PinkyEditor::resized()
{
    const float s = (float) getWidth() / (float) Layout::W;
    content->setBounds (0, 0, Layout::W, Layout::H);
    content->setTransform (juce::AffineTransform::scale (s));
}

void PinkyEditor::setSettingsOpen (bool open)
{
    content->settingsPage->setOpen (open);
    content->gear->setOpen (open);
}

void PinkyEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content->settingsPage->syncFromSettings();
    applySettings (true);
}

void PinkyEditor::applySettings (bool resizeWindow)
{
    auto& st = proc.settings;
    auto& skin = content->skin;
    skin.pal = makePalette (st.theme() == 1, st.accent());
    skin.anim = st.animation();
    skin.knobPixels = (float) pinky::PinkySettings::dragPixelsFor (st.knobFeel());
    content->refreshSkin();

    if (resizeWindow)
    {
        const float sc = pinky::PinkySettings::scaleFor (st.uiScale());
        const int w = juce::roundToInt (Layout::W * sc), h = juce::roundToInt (Layout::H * sc);
        if (getWidth() == 0 || lastScaleIdx != st.uiScale()) setSize (w, h);
        lastScaleIdx = st.uiScale();
    }

    startTimerHz (skin.anim == 2 ? 60 : 30);
}

void PinkyEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = juce::jlimit (0.0f, 0.1f, (float) ((now - lastTick) * 0.001));
    lastTick = now;
    content->tickAll (dt);
}

void PinkyEditor::advanceForTest (float seconds, int steps)
{
    for (int i = 0; i < steps; ++i) content->tickAll (seconds / (float) steps);
}
