// Pinky Lite 2 - colours, fonts, animation helpers
// Author: seoulbby
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace pinky
{
struct Palette
{
    juce::Colour bgTop, bgMid, bgBot;
    juce::Colour card, cardEdge, panel;
    juce::Colour text, textDim;
    juce::Colour accent, accent2;
    juce::Colour track, shadow;
    bool night = false;
};

inline Palette makePalette (bool night, int accentIdx)
{
    using juce::Colour;
    struct A { uint32_t a, b; };
    static constexpr A accents[] = { { 0xffff4fa3, 0xffffa3d1 },   // bubblegum
                                     { 0xffff7a6b, 0xffffc27a },   // peach fizz
                                     { 0xffa66bff, 0xfff08cff },   // lilac dream
                                     { 0xff4f8cff, 0xff8ae6ff } }; // denim ice
    const auto ac = accents[juce::jlimit (0, 3, accentIdx)];

    Palette p;
    p.night = night;
    p.accent = Colour (ac.a);
    p.accent2 = Colour (ac.b);
    if (! night)
    {
        p.bgTop = Colour (0xfffff4fa).interpolatedWith (p.accent2, 0.10f);
        p.bgMid = Colour (0xffffe3f1).interpolatedWith (p.accent2, 0.14f);
        p.bgBot = Colour (0xfff6d6ff).interpolatedWith (p.accent2, 0.16f);
        p.card = Colour (0xffffffff).withAlpha (0.58f);
        p.cardEdge = Colour (0xffffffff).withAlpha (0.95f);
        p.panel = Colour (0xffffffff).withAlpha (0.62f);
        p.text = Colour (0xff5c2146);
        p.textDim = Colour (0xff5c2146).withAlpha (0.58f);
        p.track = Colour (0xff5c2146).withAlpha (0.10f);
        p.shadow = p.accent.darker (0.3f).withAlpha (0.28f);
    }
    else
    {
        p.bgTop = Colour (0xff2c1233).interpolatedWith (p.accent, 0.08f);
        p.bgMid = Colour (0xff1d0c27).interpolatedWith (p.accent, 0.06f);
        p.bgBot = Colour (0xff12081c).interpolatedWith (p.accent2, 0.05f);
        p.card = Colour (0xffffffff).withAlpha (0.065f);
        p.cardEdge = Colour (0xffffffff).withAlpha (0.14f);
        p.panel = Colour (0xff000000).withAlpha (0.28f);
        p.text = Colour (0xffffe9f5);
        p.textDim = Colour (0xffffe9f5).withAlpha (0.55f);
        p.track = Colour (0xffffffff).withAlpha (0.10f);
        p.shadow = Colour (0xff000000).withAlpha (0.45f);
    }
    return p;
}

// Shared by every widget. Owned by the editor.
struct Skin
{
    Palette pal = makePalette (false, 0);
    int anim = 2;            // 0 off, 1 light, 2 full
    float knobPixels = 200;  // drag distance for full range

    juce::Font font (float h, bool bold = false) const
    {
        return juce::Font (juce::FontOptions (fontName(), h, bold ? juce::Font::bold : juce::Font::plain));
    }

    static const juce::String& fontName()
    {
        static const juce::String name = []
        {
            auto all = juce::Font::findAllTypefaceNames();
            for (auto* want : { "Nunito", "Quicksand", "Poppins", "Segoe UI Variable Display", "Segoe UI", "SF Pro Rounded", "Helvetica Neue" })
                if (all.contains (want)) return juce::String (want);
            return juce::Font::getDefaultSansSerifFontName();
        }();
        return name;
    }

    // exponential ease towards a target, frame-rate independent
    float approach (float cur, float target, float dt, float speed) const
    {
        if (anim == 0) return target;
        return cur + (target - cur) * (1.0f - std::exp (-dt * speed));
    }
};

struct Animated
{
    virtual ~Animated() = default;
    virtual void tick (float dt) = 0;
};

inline juce::Path fourPointStar (float cx, float cy, float r, float pinch = 0.22f)
{
    juce::Path p;
    p.startNewSubPath (cx, cy - r);
    p.quadraticTo (cx + r * pinch, cy - r * pinch, cx + r, cy);
    p.quadraticTo (cx + r * pinch, cy + r * pinch, cx, cy + r);
    p.quadraticTo (cx - r * pinch, cy + r * pinch, cx - r, cy);
    p.quadraticTo (cx - r * pinch, cy - r * pinch, cx, cy - r);
    p.closeSubPath();
    return p;
}
} // namespace pinky
