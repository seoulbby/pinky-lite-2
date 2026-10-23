// Author: seoulbby
#include "Params.h"
#include "Shapers.h"

namespace pinky
{
using namespace juce;

static String fmtDb (float v, int)   { return String (v, 1) + " dB"; }
static String fmtPct (float v, int)  { return String (roundToInt (v)) + " %"; }
static String fmtSigned (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " %"; }

static String fmtHz (float v, int)
{
    if (v >= 1000.0f) return String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return String (roundToInt (v)) + " Hz";
}

static String fmtLfoHz (float v, int) { return String (v, v < 1.0f ? 2 : 1) + " Hz"; }

static String fmtBits (float v, int)
{
    return v >= 15.95f ? String ("off") : String (v, 1) + " bit";
}

static String fmtRate (float v, int)
{
    return v <= 1.02f ? String ("off") : "/" + String (v, 1);
}

static std::unique_ptr<AudioParameterFloat> makeFloat (const char* pid, const String& name, NormalisableRange<float> r,
                                                       float def, std::function<String (float, int)> fn)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (fn));
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;

    StringArray modes;   for (auto* m : modeNames) modes.add (m);
    StringArray shapes;  for (auto* m : lfoShapeNames) shapes.add (m);
    StringArray syncs;   for (auto* m : syncNames) syncs.add (m);

    l.add (makeFloat (id::inGain, "Input", { -24.0f, 24.0f, 0.1f }, 0.0f, fmtDb));
    l.add (makeFloat (id::drive, "Drive", { 0.0f, 100.0f, 0.1f }, 30.0f, fmtPct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::mode, 1 }, "Mode", modes, 0));
    l.add (makeFloat (id::shape, "Shape", { 0.0f, 100.0f, 0.1f }, 50.0f, fmtPct));
    l.add (makeFloat (id::bias, "Bias", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (id::tone, "Tone", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));

    NormalisableRange<float> xr (60.0f, 8000.0f, 1.0f);       xr.setSkewForCentre (700.0f);
    l.add (makeFloat (id::xover, "Crossover", xr, 700.0f, fmtHz));
    l.add (makeFloat (id::lowAmt, "Low Drive", { 0.0f, 200.0f, 0.1f }, 100.0f, fmtPct));
    l.add (makeFloat (id::highAmt, "High Drive", { 0.0f, 200.0f, 0.1f }, 100.0f, fmtPct));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::quality, 1 }, "Quality",
                                                   StringArray { "Eco 1x", "HQ 2x", "Ultra 4x" }, 1));

    l.add (makeFloat (id::bits, "Bit Depth", { 1.0f, 16.0f, 0.1f }, 16.0f, fmtBits));
    NormalisableRange<float> rr (1.0f, 40.0f, 0.1f);          rr.setSkewForCentre (6.0f);
    l.add (makeFloat (id::rate, "Downsample", rr, 1.0f, fmtRate));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::fType, 1 }, "Filter",
                                                   StringArray { "Off", "Low-pass", "High-pass", "Band-pass" }, 0));
    NormalisableRange<float> cr (20.0f, 20000.0f, 1.0f);      cr.setSkewForCentre (1000.0f);
    l.add (makeFloat (id::cutoff, "Cutoff", cr, 8000.0f, fmtHz));
    l.add (makeFloat (id::reso, "Resonance", { 0.0f, 100.0f, 0.1f }, 20.0f, fmtPct));

    l.add (makeFloat (id::width, "Width", { 0.0f, 200.0f, 0.1f }, 100.0f, fmtPct));
    l.add (makeFloat (id::mix, "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, fmtPct));
    l.add (makeFloat (id::outGain, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, fmtDb));

    NormalisableRange<float> lr (0.05f, 20.0f, 0.01f);        lr.setSkewForCentre (2.0f);
    l.add (makeFloat (id::lfoRate, "LFO Rate", lr, 2.0f, fmtLfoHz));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::lfoSync, 1 }, "LFO Sync", syncs, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { id::lfoShape, 1 }, "LFO Shape", shapes, 0));
    l.add (makeFloat (id::lfoDrive, "LFO > Drive", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (id::lfoCut, "LFO > Cutoff", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (id::lfoCrush, "LFO > Crush", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));

    return l;
}

const std::vector<Preset>& factoryPresets()
{
    using namespace id;
    static const std::vector<Preset> p = {
        { "Init", {} },
        { "Strawberry Milk",   { { drive, 28 }, { mode, Tube }, { shape, 40 }, { tone, -12 }, { fType, 1 }, { cutoff, 9500 }, { reso, 12 }, { mix, 85 } } },
        { "Bubblegum Crunch",  { { drive, 62 }, { mode, Fuzz }, { shape, 45 }, { bits, 9 }, { rate, 3.2f }, { tone, 18 }, { mix, 70 } } },
        { "Cotton Candy Fold", { { drive, 55 }, { mode, Fold }, { shape, 62 }, { highAmt, 160 }, { lowAmt, 40 }, { xover, 900 }, { tone, 10 }, { width, 130 }, { mix, 80 } } },
        { "Denim Grit",        { { drive, 48 }, { mode, Tape }, { shape, 70 }, { lowAmt, 150 }, { highAmt, 60 }, { xover, 400 }, { tone, -18 }, { fType, 1 }, { cutoff, 6500 } } },
        { "Glitter Pump",      { { drive, 40 }, { mode, Soft }, { shape, 55 }, { lfoShape, 4 }, { lfoSync, 3 }, { lfoDrive, 70 }, { lfoCut, 35 }, { fType, 1 }, { cutoff, 5200 }, { reso, 30 } } },
        { "Y2K Radio",         { { drive, 50 }, { mode, Clip }, { shape, 25 }, { bits, 7 }, { rate, 5.5f }, { fType, 3 }, { cutoff, 1700 }, { reso, 38 }, { tone, 14 }, { width, 60 } } },
        { "Pink Noise Melt",   { { drive, 66 }, { mode, Warp }, { shape, 58 }, { lfoShape, 0 }, { lfoSync, 0 }, { lfoRate, 0.35f }, { lfoCut, 60 }, { lfoDrive, 25 }, { fType, 1 }, { cutoff, 2400 }, { reso, 48 }, { width, 150 } } },
        { "Sugar Rush Lead",   { { drive, 72 }, { mode, Clip }, { shape, 35 }, { bias, 18 }, { tone, 22 }, { lowAmt, 70 }, { highAmt, 130 }, { outGain, -3 } } },
        { "Moonlit Haze",      { { drive, 22 }, { mode, Soft }, { shape, 80 }, { fType, 1 }, { cutoff, 3200 }, { reso, 25 }, { lfoShape, 1 }, { lfoSync, 0 }, { lfoRate, 0.2f }, { lfoCut, 45 }, { width, 135 }, { mix, 70 } } },
        { "Rectified Rose",    { { drive, 45 }, { mode, Rect }, { shape, 40 }, { tone, 8 }, { bits, 12 }, { mix, 55 } } },
    };
    return p;
}
} // namespace pinky
