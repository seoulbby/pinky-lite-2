// Pinky Lite 2 - live visualisers, meters and the sparkly background
// Author: seoulbby
#pragma once
#include "Skin.h"
#include "PluginProcessor.h"
#include "Shapers.h"

namespace pinky
{
// shared look for the "screen" panels
inline void paintScreen (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> b)
{
    const auto& p = skin.pal;
    g.setColour (p.panel);
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (p.accent.withAlpha (p.night ? 0.22f : 0.20f));
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
}

inline juce::Path smoothPathFromPoints (const std::vector<juce::Point<float>>& pts)
{
    juce::Path path;
    for (size_t i = 0; i < pts.size(); ++i)
        i == 0 ? path.startNewSubPath (pts[i]) : path.lineTo (pts[i]);
    return path;
}

static inline float paramValue (juce::AudioProcessorValueTreeState& a, const char* id)
{
    return a.getRawParameterValue (id)->load();
}

// ============================================================================ ShaperView
class ShaperView : public juce::Component, public Animated
{
public:
    ShaperView (const Skin& s, PinkyProcessor& p) : skin (s), proc (p) { setOpaque (false); }

    void tick (float dt) override
    {
        auto& a = proc.apvts;
        const float tDrive = juce::jlimit (0.0f, 1.0f, proc.liveDrive.load());
        const float tShape = paramValue (a, id::shape) * 0.01f, tBias = paramValue (a, id::bias) * 0.01f;
        const float tIn = juce::jlimit (0.0f, 1.0f, proc.inPeak.load());
        mode = (int) paramValue (a, id::mode);
        drive = skin.approach (drive, tDrive, dt, 14.0f);
        shape = skin.approach (shape, tShape, dt, 14.0f);
        bias = skin.approach (bias, tBias, dt, 14.0f);
        level = skin.approach (level, tIn, dt, 22.0f);
        phase += dt;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat();
        paintScreen (g, skin, b);
        auto area = b.reduced (12.0f);

        // grid
        g.setColour (p.track);
        for (int i = 1; i < 4; ++i)
        {
            g.drawLine (area.getX(), area.getY() + area.getHeight() * (float) i / 4.0f, area.getRight(), area.getY() + area.getHeight() * (float) i / 4.0f, 1.0f);
            g.drawLine (area.getX() + area.getWidth() * (float) i / 4.0f, area.getY(), area.getX() + area.getWidth() * (float) i / 4.0f, area.getBottom(), 1.0f);
        }
        g.setColour (p.textDim.withAlpha (0.35f));
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.2f);
        g.drawLine (area.getCentreX(), area.getY(), area.getCentreX(), area.getBottom(), 1.2f);

        const float gv = juce::Decibels::decibelsToGain (drive * 28.0f);
        const float yMax = 1.25f;
        auto toPt = [&] (float x) -> juce::Point<float>
        {
            const float y = juce::jlimit (-yMax, yMax, pinky::shapeSample (mode, x * gv * 0.5f, shape, bias));
            return { area.getX() + (x * 0.5f + 0.5f) * area.getWidth(), area.getCentreY() - y / yMax * area.getHeight() * 0.5f };
        };

        // diagonal reference
        g.setColour (p.textDim.withAlpha (0.28f));
        juce::Path ref; ref.startNewSubPath (area.getX(), area.getBottom()); ref.lineTo (area.getRight(), area.getY());
        float dashes[] = { 4.0f, 5.0f };
        juce::Path dashed; juce::PathStrokeType (1.0f).createDashedStroke (dashed, ref, dashes, 2);
        g.fillPath (dashed);

        std::vector<juce::Point<float>> pts;
        for (int i = 0; i <= 140; ++i) pts.push_back (toPt (-1.0f + 2.0f * (float) i / 140.0f));
        auto curve = smoothPathFromPoints (pts);

        juce::Path fill (curve);
        fill.lineTo (area.getRight(), area.getCentreY());
        fill.lineTo (area.getX(), area.getCentreY());
        fill.closeSubPath();
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (area.toNearestInt());
            g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.30f), area.getX(), area.getY(), p.accent2.withAlpha (0.05f), area.getX(), area.getBottom(), false));
            g.fillPath (fill);

            if (skin.anim == 2)
            {
                g.setColour (p.accent.withAlpha (0.20f));
                g.strokePath (curve, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setGradientFill (juce::ColourGradient (p.accent2, area.getX(), area.getBottom(), p.accent, area.getRight(), area.getY(), false));
            g.strokePath (curve, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // live input dot (rides the curve)
            const float lx = juce::jlimit (0.0f, 1.0f, level);
            const float sign = std::sin (phase * 5.0f) > 0 ? 1.0f : -1.0f;
            auto dot = toPt (lx * sign);
            g.setColour (p.accent.withAlpha (0.25f));
            g.fillEllipse (dot.x - 8, dot.y - 8, 16, 16);
            g.setColour (juce::Colours::white);
            g.fillEllipse (dot.x - 4.2f, dot.y - 4.2f, 8.4f, 8.4f);
            g.setColour (p.accent);
            g.drawEllipse (dot.x - 4.2f, dot.y - 4.2f, 8.4f, 8.4f, 1.6f);
        }

        g.setColour (p.textDim);
        g.setFont (skin.font (10.0f, true));
        g.drawText (juce::String (pinky::modeNames[juce::jlimit (0, (int) pinky::NumModes - 1, mode)]).toUpperCase(),
                    b.reduced (14.0f, 8.0f), juce::Justification::topRight, false);
    }

    const Skin& skin;
private:
    PinkyProcessor& proc;
    int mode = 0;
    float drive = 0.3f, shape = 0.5f, bias = 0.0f, level = 0.0f, phase = 0.0f;
};

// ============================================================================ BandView
class BandView : public juce::Component, public Animated
{
public:
    BandView (const Skin& s, PinkyProcessor& p) : skin (s), proc (p) {}

    void tick (float dt) override
    {
        auto& a = proc.apvts;
        const float tx = std::log (paramValue (a, id::xover));
        const float live = proc.liveDrive.load();
        xover = skin.approach (xover, tx, dt, 14.0f);
        low = skin.approach (low, juce::jmin (1.0f, live * paramValue (a, id::lowAmt) * 0.01f) , dt, 12.0f);
        high = skin.approach (high, juce::jmin (1.0f, live * paramValue (a, id::highAmt) * 0.01f), dt, 12.0f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat();
        paintScreen (g, skin, b);
        auto area = b.reduced (12.0f, 10.0f);
        area.removeFromBottom (12.0f);

        const float lo = std::log (20.0f), hi = std::log (20000.0f);
        auto fx = [&] (float logf) { return area.getX() + (logf - lo) / (hi - lo) * area.getWidth(); };
        const float xx = fx (xover);

        auto drawBand = [&] (float x0, float x1, float amount, juce::Colour c)
        {
            const float h = 6.0f + amount * (area.getHeight() - 6.0f);
            juce::Rectangle<float> r (x0, area.getBottom() - h, x1 - x0, h);
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.85f), r.getX(), r.getY(), c.withAlpha (0.18f), r.getX(), r.getBottom(), false));
            juce::Path pth; pth.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 7.0f, 7.0f, true, true, false, false);
            g.fillPath (pth);
        };
        drawBand (area.getX(), xx - 1.5f, low, p.accent);
        drawBand (xx + 1.5f, area.getRight(), high, p.accent2);

        g.setColour (p.text.withAlpha (0.75f));
        g.fillRoundedRectangle (xx - 1.0f, area.getY() - 2.0f, 2.0f, area.getHeight() + 2.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (xx - 4.5f, area.getY() - 4.0f, 9.0f, 9.0f);
        g.setColour (p.accent);
        g.drawEllipse (xx - 4.5f, area.getY() - 4.0f, 9.0f, 9.0f, 1.5f);

        g.setColour (p.textDim);
        g.setFont (skin.font (9.5f, true));
        for (auto f : { 100.0f, 1000.0f, 10000.0f })
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (fx (std::log (f)) - 14, area.getBottom() + 1, 28, 12), juce::Justification::centred, false);
        g.drawText ("LOW", area.reduced (6, 2), juce::Justification::topLeft, false);
        g.drawText ("HIGH", area.reduced (6, 2), juce::Justification::topRight, false);
    }

    const Skin& skin;
private:
    PinkyProcessor& proc;
    float xover = std::log (700.0f), low = 0.3f, high = 0.3f;
};

// ============================================================================ CrushView
class CrushView : public juce::Component, public Animated
{
public:
    CrushView (const Skin& s, PinkyProcessor& p) : skin (s), proc (p) {}
    void tick (float dt) override
    {
        auto& a = proc.apvts;
        bits = skin.approach (bits, paramValue (a, id::bits), dt, 14.0f);
        rate = skin.approach (rate, paramValue (a, id::rate), dt, 14.0f);
        phase += dt * (skin.anim ? 0.25f : 0.0f);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat();
        paintScreen (g, skin, b);
        auto area = b.reduced (10.0f, 12.0f);

        const int N = 120;
        auto sine = [&] (float i) { return std::sin ((i / 80.0f + phase) * 6.2831853f); };
        auto xAt = [&] (float i) { return area.getX() + i / (float) N * area.getWidth(); };
        auto yAt = [&] (float v) { return area.getCentreY() - v * area.getHeight() * 0.5f; };

        std::vector<juce::Point<float>> orig;
        for (int i = 0; i <= N; ++i) orig.push_back ({ xAt ((float) i), yAt (sine ((float) i)) });
        g.setColour (p.textDim.withAlpha (0.35f));
        g.strokePath (smoothPathFromPoints (orig), juce::PathStrokeType (1.2f));

        const float lv = std::exp2 (juce::jmax (1.0f, bits) - 1.0f);
        const float hold = juce::jmax (1.0f, rate);
        juce::Path steps;
        float held = 0.0f; float acc = 1.0f;
        for (int i = 0; i <= N; ++i)
        {
            acc += 1.0f / hold;
            if (acc >= 1.0f) { acc -= 1.0f; held = sine ((float) i); }
            const float v = bits >= 15.9f ? held : std::round (held * lv) / lv;
            const float x = xAt ((float) i), y = yAt (v);
            if (i == 0) steps.startNewSubPath (x, y);
            else { steps.lineTo (x, steps.getCurrentPosition().y); steps.lineTo (x, y); }
        }
        if (skin.anim == 2)
        {
            g.setColour (p.accent.withAlpha (0.2f));
            g.strokePath (steps, juce::PathStrokeType (6.0f, juce::PathStrokeType::mitered));
        }
        g.setGradientFill (juce::ColourGradient (p.accent, area.getX(), 0, p.accent2, area.getRight(), 0, false));
        g.strokePath (steps, juce::PathStrokeType (2.2f, juce::PathStrokeType::mitered));
    }
    const Skin& skin;
private:
    PinkyProcessor& proc;
    float bits = 16.0f, rate = 1.0f, phase = 0.0f;
};

// ============================================================================ FilterView
class FilterView : public juce::Component, public Animated
{
public:
    FilterView (const Skin& s, PinkyProcessor& p) : skin (s), proc (p) {}
    void tick (float dt) override
    {
        auto& a = proc.apvts;
        type = (int) paramValue (a, id::fType);
        const float live = std::log (juce::jlimit (20.0f, 20000.0f, proc.liveCutoff.load()));
        cut = skin.approach (cut, live, dt, 16.0f);
        reso = skin.approach (reso, paramValue (a, id::reso) * 0.01f, dt, 14.0f);
        on = skin.approach (on, type > 0 ? 1.0f : 0.0f, dt, 10.0f);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat();
        paintScreen (g, skin, b);
        auto area = b.reduced (10.0f, 10.0f);

        const float q = 0.707f + reso * reso * 14.0f;
        const float fc = std::exp (cut);
        const float lo = std::log (20.0f), hi = std::log (20000.0f);
        auto magDb = [&] (float f)
        {
            const float w = f / fc, w2 = w * w;
            const float den = std::sqrt ((1 - w2) * (1 - w2) + (w / q) * (w / q));
            float m = 1.0f / den;
            if (type == 2) m = w2 / den;
            if (type == 3) m = (w / q) / den;
            return 20.0f * std::log10 (juce::jmax (1.0e-4f, m));
        };

        std::vector<juce::Point<float>> pts;
        for (int i = 0; i <= 120; ++i)
        {
            const float lf = lo + (hi - lo) * (float) i / 120.0f;
            const float db = type == 0 ? 0.0f : magDb (std::exp (lf));
            const float y = area.getBottom() - (juce::jlimit (-30.0f, 18.0f, db) + 30.0f) / 48.0f * area.getHeight();
            const float yFlat = area.getBottom() - 30.0f / 48.0f * area.getHeight();
            pts.push_back ({ area.getX() + (float) i / 120.0f * area.getWidth(), yFlat + (y - yFlat) * on });
        }
        auto curve = smoothPathFromPoints (pts);
        juce::Path fill (curve);
        fill.lineTo (area.getRight(), area.getBottom()); fill.lineTo (area.getX(), area.getBottom()); fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (p.accent2.withAlpha (0.35f), 0, area.getY(), p.accent.withAlpha (0.04f), 0, area.getBottom(), false));
        g.fillPath (fill);
        g.setGradientFill (juce::ColourGradient (p.accent2, area.getX(), 0, p.accent, area.getRight(), 0, false));
        g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (type > 0)
        {
            const float x = area.getX() + (cut - lo) / (hi - lo) * area.getWidth();
            g.setColour (p.accent.withAlpha (0.35f));
            g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);
        }
    }
    const Skin& skin;
private:
    PinkyProcessor& proc;
    int type = 0;
    float cut = std::log (8000.0f), reso = 0.2f, on = 0.0f;
};

// ============================================================================ LfoView
class LfoView : public juce::Component, public Animated
{
public:
    LfoView (const Skin& s, PinkyProcessor& p) : skin (s), proc (p) {}
    void tick (float dt) override
    {
        auto& a = proc.apvts;
        shape = (int) paramValue (a, id::lfoShape);
        const float amt = juce::jmax (std::abs (paramValue (a, id::lfoDrive)), std::abs (paramValue (a, id::lfoCut)), std::abs (paramValue (a, id::lfoCrush)));
        active = skin.approach (active, amt > 0.5f ? 1.0f : 0.0f, dt, 8.0f);
        phase = proc.lfoPhaseOut.load();
        repaint();
    }
    static float evalShape (int shape, float p)
    {
        if (shape != pinky::Random) return pinky::lfoEval (shape, p);
        static const float steps[] = { 0.6f, -0.4f, 0.9f, -0.8f, 0.2f, -0.1f };
        return steps[juce::jlimit (0, 5, (int) (p * 6.0f))];
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat();
        paintScreen (g, skin, b);
        auto area = b.reduced (12.0f, 10.0f);

        auto pt = [&] (float ph) -> juce::Point<float>
        { return { area.getX() + ph * area.getWidth(), area.getCentreY() - evalShape (shape, ph) * area.getHeight() * 0.5f }; };

        std::vector<juce::Point<float>> pts;
        for (int i = 0; i <= 160; ++i) pts.push_back (pt ((float) i / 160.0f));
        auto curve = smoothPathFromPoints (pts);
        juce::Path fill (curve);
        fill.lineTo (area.getRight(), area.getCentreY()); fill.lineTo (area.getX(), area.getCentreY()); fill.closeSubPath();

        const float a = 0.35f + 0.65f * active;
        g.setColour (p.textDim.withAlpha (0.25f));
        g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);
        g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (0.28f * a), 0, area.getY(), p.accent2.withAlpha (0.04f), 0, area.getBottom(), false));
        g.fillPath (fill);
        g.setGradientFill (juce::ColourGradient (p.accent.withAlpha (a), area.getX(), 0, p.accent2.withAlpha (a), area.getRight(), 0, false));
        g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float x = area.getX() + phase * area.getWidth();
        g.setColour (p.accent.withAlpha (0.35f * a));
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);
        auto d = pt (phase);
        g.setColour (p.accent.withAlpha (0.25f * a));
        g.fillEllipse (d.x - 8, d.y - 8, 16, 16);
        g.setColour (juce::Colours::white);
        g.fillEllipse (d.x - 4, d.y - 4, 8, 8);
        g.setColour (p.accent.withAlpha (a));
        g.drawEllipse (d.x - 4, d.y - 4, 8, 8, 1.5f);

        if (active < 0.5f)
        {
            g.setColour (p.textDim);
            g.setFont (skin.font (10.0f, true));
            g.drawText ("turn an amount knob to hear it move", b.reduced (8.0f, 4.0f), juce::Justification::bottomRight, false);
        }
    }
    const Skin& skin;
private:
    PinkyProcessor& proc;
    int shape = 0; float active = 0, phase = 0;
};

// icon for LFO shape pills
inline void drawLfoIcon (juce::Graphics& g, juce::Rectangle<float> r, int shape, juce::Colour c)
{
    juce::Path p;
    for (int i = 0; i <= 24; ++i)
    {
        const float ph = (float) i / 24.0f;
        const float x = r.getX() + ph * r.getWidth();
        const float y = r.getCentreY() - LfoView::evalShape (shape, ph) * r.getHeight() * 0.5f;
        i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ============================================================================ LevelBar
class LevelBar : public juce::Component, public Animated
{
public:
    LevelBar (const Skin& s, std::atomic<float>& src, juce::String name) : skin (s), source (src), label (std::move (name)) {}
    void tick (float dt) override
    {
        const float db = juce::Decibels::gainToDecibels (source.load(), -60.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        level = t > level ? skin.approach (level, t, dt, 40.0f) : skin.approach (level, t, dt, 6.0f);
        peak = juce::jmax (level, peak - dt * 0.25f);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& p = skin.pal;
        auto b = getLocalBounds().toFloat();
        auto lab = b.removeFromLeft (26.0f);
        g.setColour (p.textDim);
        g.setFont (skin.font (10.0f, true));
        g.drawText (label, lab, juce::Justification::centredLeft, false);
        auto bar = b.withSizeKeepingCentre (b.getWidth(), 8.0f);
        g.setColour (p.track);
        g.fillRoundedRectangle (bar, 4.0f);
        auto f = bar.withWidth (juce::jmax (8.0f, bar.getWidth() * level));
        g.setGradientFill (juce::ColourGradient (p.accent2, bar.getX(), 0, level > 0.93f ? juce::Colour (0xffff3b6b) : p.accent, bar.getRight(), 0, false));
        g.fillRoundedRectangle (f, 4.0f);
        g.setColour (p.text.withAlpha (0.5f));
        const float px = bar.getX() + bar.getWidth() * peak;
        g.fillRoundedRectangle (px - 1.0f, bar.getY() - 1.0f, 2.0f, bar.getHeight() + 2.0f, 1.0f);
    }
    const Skin& skin;
private:
    std::atomic<float>& source; juce::String label; float level = 0, peak = 0;
};

// ============================================================================ Background (gradient, bokeh, cards, sparkles)
struct Layout
{
    static constexpr int W = 900, H = 620;
    static inline const juce::Rectangle<int> header { 0, 0, W, 70 },
        cardA { 16, 78, 572, 262 }, cardB { 596, 78, 288, 262 },
        cardC { 16, 350, 188, 258 }, cardD { 212, 350, 224, 258 },
        cardE { 444, 350, 296, 258 }, cardF { 748, 350, 136, 258 };
};

class BackgroundLayer : public juce::Component, public Animated
{
public:
    explicit BackgroundLayer (const Skin& s) : skin (s)
    {
        setInterceptsMouseClicks (false, false);
        juce::Random r (2012);
        for (int i = 0; i < 16; ++i)
            stars.push_back ({ r.nextFloat(), r.nextFloat(), 5.0f + r.nextFloat() * 9.0f, r.nextFloat() * 6.28f,
                               0.6f + r.nextFloat() * 1.4f, (r.nextFloat() - 0.5f) * 0.012f, 0.004f + r.nextFloat() * 0.01f });
    }

    void skinChanged() { cache = {}; repaint(); }

    void tick (float dt) override
    {
        if (skin.anim < 2) return;
        for (auto& st : stars)
        {
            auto old = boundsOf (st);
            st.t += dt * st.speed;
            st.x += st.dx * dt; st.y -= st.dy * dt;
            if (st.y < -0.05f) st.y = 1.05f;
            if (st.x < -0.05f) st.x = 1.05f;
            if (st.x > 1.05f) st.x = -0.05f;
            repaint (old.getUnion (boundsOf (st)));
        }
    }

    void paint (juce::Graphics& g) override
    {
        const float sc = juce::jmax (1.0f, juce::Component::getApproximateScaleFactorForComponent (this));
        const int cw = juce::roundToInt ((float) getWidth() * sc), ch = juce::roundToInt ((float) getHeight() * sc);
        if (! cache.isValid() || cache.getWidth() != cw || cache.getHeight() != ch)
            renderCache (cw, ch);
        g.drawImage (cache, getLocalBounds().toFloat());

        if (skin.anim == 2)
            for (auto& st : stars)
            {
                const float tw = 0.5f + 0.5f * std::sin (st.t * 2.0f + st.phase);
                const float r = st.size * (0.55f + 0.45f * tw);
                const float cx = st.x * (float) getWidth(), cy = st.y * (float) getHeight();
                auto star = fourPointStar (cx, cy, r);
                g.setColour ((skin.pal.night ? skin.pal.accent2 : juce::Colours::white).withAlpha (0.25f + 0.55f * tw));
                g.fillPath (star);
                g.setColour (skin.pal.accent.withAlpha (0.18f * tw));
                g.fillEllipse (cx - r * 1.1f, cy - r * 1.1f, r * 2.2f, r * 2.2f);
            }
    }
    void resized() override { cache = {}; }

private:
    struct Star { float x, y, size, phase, speed, dx, dy; float t = 0; };
    juce::Rectangle<int> boundsOf (const Star& s) const
    {
        const float cx = s.x * (float) getWidth(), cy = s.y * (float) getHeight();
        return juce::Rectangle<float> (cx - s.size * 1.4f, cy - s.size * 1.4f, s.size * 2.8f, s.size * 2.8f).getSmallestIntegerContainer();
    }

    void renderCache (int cw, int ch)
    {
        cache = juce::Image (juce::Image::ARGB, cw, ch, true);
        juce::Graphics g (cache);
        g.addTransform (juce::AffineTransform::scale ((float) cw / (float) Layout::W));
        const auto& p = skin.pal;
        const float W = (float) Layout::W, H = (float) Layout::H;

        juce::ColourGradient bg (p.bgTop, 0, 0, p.bgBot, W, H, false);
        bg.addColour (0.5, p.bgMid);
        g.setGradientFill (bg);
        g.fillAll();

        // bokeh blobs
        auto blob = [&] (float x, float y, float r, juce::Colour c)
        {
            juce::ColourGradient gr (c, x, y, c.withAlpha (0.0f), x + r, y, true);
            g.setGradientFill (gr);
            g.fillEllipse (x - r, y - r, r * 2, r * 2);
        };
        blob (80, 40, 260, p.accent2.withAlpha (p.night ? 0.16f : 0.38f));
        blob (840, 560, 320, p.accent.withAlpha (p.night ? 0.14f : 0.22f));
        blob (700, 30, 200, (p.night ? juce::Colour (0xff6f7bff) : juce::Colour (0xffbfd8ff)).withAlpha (p.night ? 0.14f : 0.45f));
        blob (130, 600, 200, (p.night ? juce::Colour (0xffb06bff) : juce::Colour (0xffe6c8ff)).withAlpha (p.night ? 0.12f : 0.4f));

        struct C { juce::Rectangle<int> r; const char* title; };
        const C cards[] = { { Layout::cardA, "DISTORTION" }, { Layout::cardB, "BANDS" }, { Layout::cardC, "CRUSH" },
                            { Layout::cardD, "FILTER" }, { Layout::cardE, "MOTION" }, { Layout::cardF, "OUTPUT" } };
        for (auto& c : cards)
        {
            auto r = c.r.toFloat();
            juce::Path pth; pth.addRoundedRectangle (r, 20.0f);
            juce::DropShadow (p.shadow.withAlpha (p.night ? 0.5f : 0.22f), 16, { 0, 6 }).drawForPath (g, pth);
            g.setColour (p.card);
            g.fillPath (pth);
            g.setGradientFill (juce::ColourGradient (p.cardEdge, r.getX(), r.getY(), p.cardEdge.withAlpha (0.25f), r.getRight(), r.getBottom(), false));
            g.strokePath (pth, juce::PathStrokeType (1.3f));

            // title with a little star
            g.setColour (p.accent);
            g.fillPath (fourPointStar (r.getX() + 24.0f, r.getY() + 17.0f, 5.5f));
            g.setColour (p.text);
            g.setFont (skin.font (11.5f, true));
            g.drawText (juce::String (c.title), juce::Rectangle<float> (r.getX() + 34.0f, r.getY() + 9.0f, 150.0f, 16.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    const Skin& skin;
    juce::Image cache;
    std::vector<Star> stars;
};
} // namespace pinky
