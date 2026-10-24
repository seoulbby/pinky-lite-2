// Pinky Lite 2 - custom animated widgets
// Author: seoulbby
#pragma once
#include "Skin.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace pinky
{
using APVTS = juce::AudioProcessorValueTreeState;

// ============================================================================ KnobBox
class KnobBox : public juce::Slider, public Animated
{
public:
    KnobBox (const Skin& s, APVTS& apvts, const juce::String& paramID, const juce::String& labelText,
             float diameter, bool bipolarArc = false)
        : skin (s), label (labelText), dia (diameter), bipolar (bipolarArc)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setScrollWheelEnabled (true);
        setMouseDragSensitivity ((int) skin.knobPixels);
        attachment = std::make_unique<APVTS::SliderAttachment> (apvts, paramID, *this);
        if (auto* p = apvts.getParameter (paramID))
            setDoubleClickReturnValue (true, (double) p->convertFrom0to1 (p->getDefaultValue()));
        dispN = (float) valueToProportionOfLength (getValue());
        setBufferedToImage (true);
        setRepaintsOnMouseActivity (false);
    }

    void applySkin()
    {
        setMouseDragSensitivity ((int) skin.knobPixels);
        repaint();
    }

    void tick (float dt) override
    {
        const float target = (float) valueToProportionOfLength (getValue());
        const float hoverT = (isMouseOverOrDragging() ? 1.0f : 0.0f);
        const float pressT = isMouseButtonDown() ? 1.0f : 0.0f;
        const float nN = skin.approach (dispN, target, dt, 16.0f);
        const float nH = skin.approach (hover, hoverT, dt, 14.0f);
        const float nP = skin.approach (press, pressT, dt, 20.0f);
        const bool changed = std::abs (nN - dispN) > 0.0004f || std::abs (nH - hover) > 0.004f || std::abs (nP - press) > 0.004f;
        dispN = nN; hover = nH; press = nP;
        if (changed) repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override { juce::Slider::mouseDown (e); repaint(); }
    void mouseUp (const juce::MouseEvent& e) override   { juce::Slider::mouseUp (e);   repaint(); }
    void valueChanged() override                        { repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        const float W = (float) getWidth(), H = (float) getHeight();
        const float extraTop = juce::jmax (0.0f, (H - (dia + 46.0f)) * 0.5f);
        const float cx = W * 0.5f, cy = 5.0f + extraTop + dia * 0.5f;
        const float R = dia * 0.5f * (1.0f + 0.03f * hover - 0.02f * press);

        // hover halo
        if (hover > 0.01f)
        {
            juce::ColourGradient halo (p.accent.withAlpha (0.30f * hover), cx, cy, p.accent.withAlpha (0.0f), cx, cy, true);
            halo.addColour (0.62, p.accent.withAlpha (0.18f * hover));
            g.setGradientFill (halo);
            g.fillEllipse (cx - R - 12, cy - R - 12, (R + 12) * 2, (R + 12) * 2);
        }

        // body
        juce::Path disc; disc.addEllipse (cx - R, cy - R, R * 2, R * 2);
        juce::DropShadow (p.shadow, 7, { 0, 3 }).drawForPath (g, disc);
        juce::ColourGradient body (p.night ? juce::Colour (0xff4a2650) : juce::Colours::white, cx - R * 0.5f, cy - R * 0.7f,
                                   p.night ? juce::Colour (0xff2a1233) : juce::Colour (0xffffd3e8), cx + R * 0.6f, cy + R, false);
        g.setGradientFill (body);
        g.fillPath (disc);
        g.setColour (p.cardEdge.withAlpha (p.night ? 0.25f : 0.95f));
        g.drawEllipse (cx - R, cy - R, R * 2, R * 2, 1.2f);

        // arcs
        const float a0 = -2.356f, a1 = 2.356f;
        const float ar = R - 4.5f;
        const float thick = juce::jmax (3.5f, dia * 0.085f);
        juce::Path track; track.addCentredArc (cx, cy, ar, ar, 0.0f, a0, a1, true);
        g.setColour (p.track);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float aVal = a0 + dispN * (a1 - a0);
        const float aFrom = bipolar ? 0.0f : a0;
        if (std::abs (aVal - aFrom) > 0.002f)
        {
            juce::Path val; val.addCentredArc (cx, cy, ar, ar, 0.0f, juce::jmin (aFrom, aVal), juce::jmax (aFrom, aVal), true);
            juce::ColourGradient grad (p.accent2, cx - ar, cy + ar, p.accent, cx + ar, cy - ar, false);
            if (skin.anim == 2)
            {
                g.setColour (p.accent.withAlpha (0.22f + 0.15f * hover));
                g.strokePath (val, juce::PathStrokeType (thick + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setGradientFill (grad);
            g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // pointer
        const float pr = ar - thick * 0.5f - 3.0f;
        const float sx = std::sin (aVal), sy = -std::cos (aVal);
        const float inner = R * 0.18f;
        juce::Path ptr; ptr.startNewSubPath (cx + sx * inner, cy + sy * inner); ptr.lineTo (cx + sx * pr, cy + sy * pr);
        g.setColour (p.accent);
        g.strokePath (ptr, juce::PathStrokeType (juce::jmax (2.5f, dia * 0.06f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (p.accent.withAlpha (0.18f));
        g.fillEllipse (cx - inner, cy - inner, inner * 2, inner * 2);

        // texts
        const float ly = cy + dia * 0.5f + 7.0f;
        g.setColour (p.text);
        g.setFont (skin.font (juce::jlimit (9.5f, 12.0f, W * 0.15f), true));
        g.drawText (label.toUpperCase(), juce::Rectangle<float> (0, ly, W, 14), juce::Justification::centred, false);

        g.setColour (hover > 0.3f || isMouseButtonDown() ? p.accent : p.textDim);
        g.setFont (skin.font (juce::jlimit (9.5f, 11.5f, W * 0.15f), false));
        g.drawText (getTextFromValue (getValue()), juce::Rectangle<float> (0, ly + 14, W, 15), juce::Justification::centred, false);
    }

    const Skin& skin;

private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::String label;
    float dia;
    bool bipolar;
    float dispN = 0, hover = 0, press = 0;
};

// ============================================================================ PillSelector
class PillSelector : public juce::Component, public Animated
{
public:
    // bound to a choice parameter
    PillSelector (const Skin& s, APVTS& apvts, const juce::String& paramID, juce::StringArray labelsIn, int cols)
        : skin (s), labels (std::move (labelsIn)), columns (cols)
    {
        auto* param = apvts.getParameter (paramID);
        jassert (param != nullptr);
        att = std::make_unique<juce::ParameterAttachment> (*dynamic_cast<juce::RangedAudioParameter*> (param),
                                                           [this] (float v) { setIndex (juce::roundToInt (v)); });
        att->sendInitialUpdate();
        hover.assign ((size_t) labels.size(), 0.0f);
        setRepaintsOnMouseActivity (false);
        snapHighlight = true;
    }

    // free-standing (settings page)
    PillSelector (const Skin& s, juce::StringArray labelsIn, int cols, int initial, std::function<void (int)> cb)
        : skin (s), labels (std::move (labelsIn)), columns (cols), onSelect (std::move (cb))
    {
        hover.assign ((size_t) labels.size(), 0.0f);
        setIndex (initial);
        snapHighlight = true;
    }

    std::function<void (juce::Graphics&, juce::Rectangle<float>, int, juce::Colour)> drawIcon;
    int getIndex() const { return selected; }

    void setIndex (int i)
    {
        selected = juce::jlimit (0, juce::jmax (0, labels.size() - 1), i);
        repaint();
    }

    juce::Rectangle<float> cellFor (int i) const
    {
        const int rows = (labels.size() + columns - 1) / columns;
        const float cw = (float) getWidth() / (float) columns, ch = (float) getHeight() / (float) rows;
        return juce::Rectangle<float> ((float) (i % columns) * cw, (float) (i / columns) * ch, cw, ch).reduced (2.0f);
    }

    void tick (float dt) override
    {
        auto target = cellFor (selected);
        bool changed = false;
        if (snapHighlight && ! hl.isEmpty()) snapHighlight = false;
        if (hl.isEmpty() || snapHighlight) { hl = target; changed = true; snapHighlight = false; }
        else
        {
            juce::Rectangle<float> n (skin.approach (hl.getX(), target.getX(), dt, 18.0f), skin.approach (hl.getY(), target.getY(), dt, 18.0f),
                                      skin.approach (hl.getWidth(), target.getWidth(), dt, 18.0f), skin.approach (hl.getHeight(), target.getHeight(), dt, 18.0f));
            if (std::abs (n.getX() - hl.getX()) > 0.02f || std::abs (n.getY() - hl.getY()) > 0.02f) changed = true;
            hl = n;
        }
        for (int i = 0; i < labels.size(); ++i)
        {
            const float t = (i == hovered) ? 1.0f : 0.0f;
            const float n = skin.approach (hover[(size_t) i], t, dt, 16.0f);
            if (std::abs (n - hover[(size_t) i]) > 0.004f) changed = true;
            hover[(size_t) i] = n;
        }
        if (changed) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        const auto selTarget = cellFor (selected);
        juce::ignoreUnused (selTarget);

        for (int i = 0; i < labels.size(); ++i)
        {
            auto c = cellFor (i);
            g.setColour (p.accent.withAlpha (p.night ? 0.10f : 0.09f + 0.08f * hover[(size_t) i]));
            g.fillRoundedRectangle (c, c.getHeight() * 0.5f);
        }

        if (! hl.isEmpty())
        {
            juce::Path pill; pill.addRoundedRectangle (hl, hl.getHeight() * 0.5f);
            if (skin.anim == 2) juce::DropShadow (p.accent.withAlpha (0.45f), 6, { 0, 2 }).drawForPath (g, pill);
            g.setGradientFill (juce::ColourGradient (p.accent, hl.getX(), hl.getY(), p.accent2, hl.getRight(), hl.getBottom(), false));
            g.fillPath (pill);
        }

        for (int i = 0; i < labels.size(); ++i)
        {
            auto c = cellFor (i);
            const float overlap = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (c.getCentreX () - hl.getCentreX()) / c.getWidth());
            const float ov = std::abs (c.getCentreY() - hl.getCentreY()) < c.getHeight() * 0.5f ? overlap : 0.0f;
            auto col = p.textDim.interpolatedWith (p.text, hover[(size_t) i]).interpolatedWith (juce::Colours::white, ov);
            if (drawIcon) drawIcon (g, c.reduced (c.getWidth() * 0.22f, c.getHeight() * 0.24f), i, col);
            else
            {
                g.setColour (col);
                g.setFont (skin.font (juce::jlimit (9.5f, 12.5f, c.getHeight() * 0.5f), true));
                g.drawText (labels[i], c, juce::Justification::centred, false);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        hovered = -1;
        for (int i = 0; i < labels.size(); ++i) if (cellFor (i).contains (e.position)) hovered = i;
        setMouseCursor (hovered >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < labels.size(); ++i)
            if (cellFor (i).contains (e.position))
            {
                if (att) att->setValueAsCompleteGesture ((float) i);
                else { setIndex (i); if (onSelect) onSelect (i); }
                return;
            }
    }
    void resized() override { snapHighlight = true; hl = {}; }

    const Skin& skin;

private:
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::StringArray labels;
    int columns, selected = 0, hovered = -1;
    std::vector<float> hover;
    juce::Rectangle<float> hl;
    bool snapHighlight = true;
    std::function<void (int)> onSelect;
};

// ============================================================================ StepSelector (< value >)
class StepSelector : public juce::Component, public Animated
{
public:
    StepSelector (const Skin& s, APVTS& apvts, const juce::String& paramID, juce::String prefixText = {}) : skin (s), prefix (std::move (prefixText))
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID));
        jassert (param != nullptr);
        choices = param->choices;
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { index = juce::roundToInt (v); repaint(); });
        att->sendInitialUpdate();
    }

    void tick (float dt) override
    {
        const float t = hovered ? 1.0f : 0.0f;
        const float n = skin.approach (hover, t, dt, 14.0f);
        if (std::abs (n - hover) > 0.004f) { hover = n; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (p.accent.withAlpha (0.10f + 0.08f * hover));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (p.accent.withAlpha (0.35f));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

        g.setColour (p.accent);
        g.setFont (skin.font (13.0f, true));
        g.drawText (juce::String::charToString (0x2039), b.removeFromLeft (b.getHeight()), juce::Justification::centred, false);
        g.drawText (juce::String::charToString (0x203a), b.removeFromRight (b.getHeight()), juce::Justification::centred, false);
        g.setColour (p.text);
        g.setFont (skin.font (11.5f, true));
        g.drawText (prefix + choices[index], b, juce::Justification::centred, false);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true; }
    void mouseExit (const juce::MouseEvent&) override  { hovered = false; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = choices.size();
        if (e.x < getHeight()) att->setValueAsCompleteGesture ((float) ((index + n - 1) % n));
        else if (e.x > getWidth() - getHeight()) att->setValueAsCompleteGesture ((float) ((index + 1) % n));
        else
        {
            juce::PopupMenu m;
            for (int i = 0; i < n; ++i) m.addItem (i + 1, choices[i], true, i == index);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                             [this] (int r) { if (r > 0) att->setValueAsCompleteGesture ((float) (r - 1)); });
        }
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int n = choices.size();
        att->setValueAsCompleteGesture ((float) ((index + (w.deltaY > 0 ? 1 : n - 1)) % n));
    }

    const Skin& skin;

private:
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::StringArray choices;
    juce::String prefix;
    int index = 0;
    bool hovered = false;
    float hover = 0.0f;
};

// ============================================================================ ToggleSwitch
class ToggleSwitch : public juce::Component, public Animated
{
public:
    ToggleSwitch (const Skin& s, bool initial, std::function<void (bool)> cb)
        : skin (s), state (initial), pos (initial ? 1.0f : 0.0f), onChange (std::move (cb)) {}

    void setState (bool b) { state = b; }
    void tick (float dt) override
    {
        const float n = skin.approach (pos, state ? 1.0f : 0.0f, dt, 16.0f);
        if (std::abs (n - pos) > 0.0005f) { pos = n; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (p.track.interpolatedWith (p.accent, pos).withAlpha (0.35f + 0.65f * pos));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        const float d = b.getHeight() - 6.0f;
        const float x = b.getX() + 3.0f + pos * (b.getWidth() - d - 6.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x, b.getY() + 3.0f, d, d);
    }
    void mouseDown (const juce::MouseEvent&) override { state = ! state; if (onChange) onChange (state); }
    const Skin& skin;
private:
    bool state; float pos; std::function<void (bool)> onChange;
};

// ============================================================================ SwatchRow
class SwatchRow : public juce::Component, public Animated
{
public:
    SwatchRow (const Skin& s, int initial, std::function<void (int)> cb) : skin (s), selected (initial), onSelect (std::move (cb)) {}
    void setIndex (int i) { selected = i; }
    void tick (float dt) override
    {
        bool ch = false;
        for (int i = 0; i < 4; ++i)
        {
            const float n = skin.approach (sel[i], i == selected ? 1.0f : 0.0f, dt, 14.0f);
            if (std::abs (n - sel[i]) > 0.004f) ch = true;
            sel[i] = n;
        }
        if (ch) repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const float d = (float) getHeight();
        for (int i = 0; i < 4; ++i)
        {
            auto pal = makePalette (skin.pal.night, i);
            auto r = juce::Rectangle<float> ((d + 10.0f) * (float) i, 0.0f, d, d).reduced (3.0f - 2.0f * sel[i]);
            g.setGradientFill (juce::ColourGradient (pal.accent, r.getX(), r.getY(), pal.accent2, r.getRight(), r.getBottom(), false));
            g.fillEllipse (r);
            if (sel[i] > 0.01f)
            {
                g.setColour (skin.pal.text.withAlpha (sel[i]));
                g.drawEllipse (r.expanded (3.0f), 2.0f);
            }
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float d = (float) getHeight();
        const int i = (int) (e.position.x / (d + 10.0f));
        if (i >= 0 && i < 4) { selected = i; if (onSelect) onSelect (i); }
    }
    const Skin& skin;
private:
    int selected; float sel[4] {}; std::function<void (int)> onSelect;
};

// ============================================================================ PinkButton
class PinkButton : public juce::Component, public Animated
{
public:
    PinkButton (const Skin& s, juce::String t, std::function<void()> cb, bool filled = true)
        : skin (s), text (std::move (t)), onClick (std::move (cb)), fill (filled) {}
    void tick (float dt) override
    {
        const float n = skin.approach (hover, hovered ? 1.0f : 0.0f, dt, 16.0f);
        if (std::abs (n - hover) > 0.004f) { hover = n; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const float press = isMouseButtonDown() ? 0.97f : 1.0f;
        b = b.withSizeKeepingCentre (b.getWidth() * press, b.getHeight() * press);
        if (fill)
        {
            g.setGradientFill (juce::ColourGradient (p.accent, b.getX(), b.getY(), p.accent2, b.getRight(), b.getBottom(), false));
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.25f * hover));
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
            g.setColour (juce::Colours::white);
        }
        else
        {
            g.setColour (p.accent.withAlpha (0.08f + 0.12f * hover));
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
            g.setColour (p.accent.withAlpha (0.6f));
            g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.2f);
            g.setColour (p.text);
        }
        g.setFont (skin.font (12.0f, true));
        g.drawText (text, b, juce::Justification::centred, false);
    }
    void mouseEnter (const juce::MouseEvent&) override { hovered = true; }
    void mouseExit (const juce::MouseEvent&) override  { hovered = false; }
    void mouseUp (const juce::MouseEvent& e) override  { repaint(); if (contains (e.getPosition()) && onClick) onClick(); }
    void mouseDown (const juce::MouseEvent&) override  { repaint(); }
    const Skin& skin;
private:
    juce::String text; std::function<void()> onClick; bool fill, hovered = false; float hover = 0;
};

// ============================================================================ GearButton
class GearButton : public juce::Component, public Animated
{
public:
    GearButton (const Skin& s, std::function<void()> cb) : skin (s), onClick (std::move (cb)) {}
    void setOpen (bool o) { open = o; }
    void tick (float dt) override
    {
        const float target = open ? 1.0f : 0.0f;
        const float nA = skin.approach (angle, target, dt, 7.0f);
        const float nH = skin.approach (hover, hovered ? 1.0f : 0.0f, dt, 14.0f);
        if (std::abs (nA - angle) > 0.0005f || std::abs (nH - hover) > 0.004f) repaint();
        angle = nA; hover = nH;
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (p.panel);
        g.fillEllipse (b);
        g.setColour (p.accent.withAlpha (0.25f + 0.5f * hover));
        g.drawEllipse (b, 1.4f);

        const float r = b.getWidth() * 0.30f;
        juce::Path gear;
        gear.addStar (b.getCentre(), 8, r * 0.80f, r, 0.0f);
        gear.addEllipse (b.getCentreX() - r * 0.3f, b.getCentreY() - r * 0.3f, r * 0.6f, r * 0.6f);
        gear.setUsingNonZeroWinding (false);
        g.setGradientFill (juce::ColourGradient (p.accent, b.getX(), b.getY(), p.accent2, b.getRight(), b.getBottom(), false));
        g.fillPath (gear, juce::AffineTransform::rotation (angle * 3.1416f * 1.0f + hover * 0.4f, b.getCentreX(), b.getCentreY()));
    }
    void mouseEnter (const juce::MouseEvent&) override { hovered = true; }
    void mouseExit (const juce::MouseEvent&) override  { hovered = false; }
    void mouseUp (const juce::MouseEvent& e) override  { if (contains (e.getPosition()) && onClick) onClick(); }
    const Skin& skin;
private:
    std::function<void()> onClick; bool open = false, hovered = false; float angle = 0, hover = 0;
};
} // namespace pinky
