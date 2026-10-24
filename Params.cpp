// Author: seoulbby
#include "Params.h"
#include "Lfo.h"
#include "Glitch.h"
#include "Glass.h"

namespace pinky
{
using namespace juce;

static String fmtDb (float v, int)     { return String (v, 1) + " dB"; }
static String fmtPct (float v, int)    { return String (roundToInt (v)) + " %"; }
static String fmtSigned (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " %"; }
static String fmtSt (float v, int)     { return (v > 0.4f ? "+" : "") + String (roundToInt (v)) + " st"; }
static String fmtMs (float v, int)     { return String (v, 1) + " ms"; }
static String fmtLfoHz (float v, int)  { return String (v, v < 1.0f ? 2 : 1) + " Hz"; }
static String fmtBits (float v, int)   { return v >= 15.95f ? String ("off") : String (v, 1) + " bit"; }
static String fmtRate (float v, int)   { return v <= 1.02f ? String ("off") : "/" + String (v, 1); }
static String fmtHz (float v, int)
{
    if (v >= 1000.0f) return String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return String (roundToInt (v)) + " Hz";
}

static std::unique_ptr<AudioParameterFloat> makeFloat (const char* pid, const String& name, NormalisableRange<float> r,
                                                       float def, std::function<String (float, int)> fn)
{
    return std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (fn));
}

static std::unique_ptr<AudioParameterChoice> makeChoice (const char* pid, const String& name, StringArray s, int def)
{
    return std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, s, def);
}

template <size_t N>
static StringArray toArray (const char* const (&a)[N]) { StringArray s; for (auto* x : a) s.add (x); return s; }

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    using namespace id;

    l.add (makeFloat (inGain, "Input", { -24.0f, 24.0f, 0.1f }, 0.0f, fmtDb));

    // Wobble
    l.add (makeFloat (wobDepth, "Wobble Depth", { 0.0f, 100.0f, 0.1f }, 20.0f, fmtPct));
    NormalisableRange<float> wr (0.1f, 10.0f, 0.01f);  wr.setSkewForCentre (1.5f);
    l.add (makeFloat (wobRate, "Wobble Rate", wr, 1.2f, fmtLfoHz));
    l.add (makeFloat (wobSpread, "Wobble Spread", { 0.0f, 100.0f, 0.1f }, 50.0f, fmtPct));

    // Shimmer
    l.add (makeFloat (shimAmt, "Shimmer", { 0.0f, 100.0f, 0.1f }, 30.0f, fmtPct));
    l.add (makeFloat (shimPitch, "Shimmer Pitch", { -12.0f, 12.0f, 1.0f }, 12.0f, fmtSt));
    l.add (makeFloat (shimDecay, "Shimmer Decay", { 0.0f, 100.0f, 0.1f }, 60.0f, fmtPct));

    // Glass
    l.add (makeFloat (glassAmt, "Glass", { 0.0f, 100.0f, 0.1f }, 25.0f, fmtPct));
    l.add (makeFloat (glassDecay, "Glass Decay", { 0.0f, 100.0f, 0.1f }, 50.0f, fmtPct));
    l.add (makeFloat (glassSpread, "Glass Spread", { 0.0f, 100.0f, 0.1f }, 40.0f, fmtPct));
    l.add (makeChoice (glassKey, "Glass Key", toArray (keyNames), 0));
    l.add (makeChoice (glassScale, "Glass Scale", toArray (scaleNames), 2));

    // Glitch
    l.add (makeFloat (glAmt, "Glitch", { 0.0f, 100.0f, 0.1f }, 0.0f, fmtPct));
    l.add (makeFloat (glChance, "Glitch Chance", { 0.0f, 100.0f, 0.1f }, 50.0f, fmtPct));
    l.add (makeFloat (glSmooth, "Glitch Smooth", { 1.0f, 40.0f, 0.1f }, 8.0f, fmtMs));
    l.add (makeChoice (glDiv, "Glitch Division", toArray (glitchDivNames), 1));
    l.add (makeChoice (glType, "Glitch Type", toArray (glitchTypeNames), 0));

    // Crush
    l.add (makeFloat (bits, "Bit Depth", { 1.0f, 16.0f, 0.1f }, 16.0f, fmtBits));
    NormalisableRange<float> rr (1.0f, 40.0f, 0.1f);   rr.setSkewForCentre (6.0f);
    l.add (makeFloat (rate, "Downsample", rr, 1.0f, fmtRate));

    // Filter
    l.add (makeChoice (fType, "Filter", StringArray { "Off", "Low-pass", "High-pass", "Band-pass" }, 0));
    NormalisableRange<float> cr (20.0f, 20000.0f, 1.0f);   cr.setSkewForCentre (1000.0f);
    l.add (makeFloat (cutoff, "Cutoff", cr, 8000.0f, fmtHz));
    l.add (makeFloat (reso, "Resonance", { 0.0f, 100.0f, 0.1f }, 20.0f, fmtPct));

    // Output
    l.add (makeFloat (tone, "Tone", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (width, "Width", { 0.0f, 200.0f, 0.1f }, 100.0f, fmtPct));
    l.add (makeFloat (mix, "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, fmtPct));
    l.add (makeFloat (outGain, "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, fmtDb));

    // Motion
    NormalisableRange<float> lr (0.05f, 20.0f, 0.01f);  lr.setSkewForCentre (2.0f);
    l.add (makeFloat (lfoRate, "LFO Rate", lr, 2.0f, fmtLfoHz));
    l.add (makeChoice (lfoSync, "LFO Sync", toArray (syncNames), 0));
    l.add (makeChoice (lfoShape, "LFO Shape", toArray (lfoShapeNames), 0));
    l.add (makeFloat (lfoWob, "LFO > Wobble", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (lfoShim, "LFO > Shimmer", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (lfoGlass, "LFO > Glass", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    l.add (makeFloat (lfoCut, "LFO > Cutoff", { -100.0f, 100.0f, 0.1f }, 0.0f, fmtSigned));
    return l;
}

const std::vector<Preset>& factoryPresets()
{
    using namespace id;
    // glass scales: 0 major 1 minor 2 maj-pent 3 min-pent 4 lydian 5 dorian | glitch types: 0 stutter 1 reverse 2 stop 3 gate 4 random
    // glitch divs: 0 1/4  1 1/8  2 1/16  3 1/32
    static const std::vector<Preset> p = {
        { "Init", { { wobDepth, 0 }, { shimAmt, 0 }, { glassAmt, 0 }, { glAmt, 0 } } },
        { "Strawberry Milk",   { { wobDepth, 22 }, { wobRate, 0.8f }, { wobSpread, 60 }, { shimAmt, 38 }, { shimPitch, 12 }, { shimDecay, 55 }, { glassAmt, 0 }, { tone, 8 } } },
        { "Cotton Candy Cloud",{ { wobDepth, 30 }, { wobRate, 0.5f }, { shimAmt, 62 }, { shimPitch, 12 }, { shimDecay, 82 }, { glassAmt, 0 }, { fType, 1 }, { cutoff, 9000 }, { width, 150 } } },
        { "Glass Heart",       { { wobDepth, 12 }, { glassAmt, 72 }, { glassDecay, 62 }, { glassSpread, 55 }, { glassKey, 0 }, { glassScale, 2 }, { shimAmt, 26 }, { shimPitch, 7 }, { shimDecay, 60 }, { tone, 12 } } },
        { "Bubblegum Stutter", { { wobDepth, 10 }, { shimAmt, 18 }, { glassAmt, 0 }, { glAmt, 88 }, { glChance, 55 }, { glDiv, 2 }, { glType, 0 }, { glSmooth, 4 } } },
        { "Tape Stop Pop",     { { wobDepth, 38 }, { wobRate, 0.4f }, { shimAmt, 0 }, { glassAmt, 0 }, { glAmt, 92 }, { glChance, 40 }, { glDiv, 0 }, { glType, 2 }, { fType, 1 }, { cutoff, 7000 } } },
        { "Reverse Wish",      { { wobDepth, 14 }, { shimAmt, 46 }, { shimPitch, 12 }, { shimDecay, 70 }, { glassAmt, 20 }, { glAmt, 82 }, { glChance, 50 }, { glDiv, 0 }, { glType, 1 } } },
        { "Y2K Gate",          { { wobDepth, 0 }, { shimAmt, 0 }, { glassAmt, 0 }, { glAmt, 100 }, { glChance, 100 }, { glDiv, 2 }, { glType, 3 }, { glSmooth, 3 }, { bits, 9 }, { rate, 2.5f }, { fType, 3 }, { cutoff, 1800 }, { reso, 35 } } },
        { "Pink Heaven",       { { wobDepth, 26 }, { wobRate, 0.7f }, { shimAmt, 72 }, { shimPitch, 12 }, { shimDecay, 90 }, { glassAmt, 45 }, { glassKey, 5 }, { glassScale, 4 }, { glassDecay, 70 }, { lfoShape, 0 }, { lfoSync, 0 }, { lfoRate, 0.15f }, { lfoShim, 30 }, { width, 140 } } },
        { "Glitter Rain",      { { wobDepth, 8 }, { shimAmt, 52 }, { shimPitch, 12 }, { shimDecay, 66 }, { glassAmt, 34 }, { glassKey, 7 }, { glassScale, 2 }, { glAmt, 60 }, { glChance, 38 }, { glDiv, 3 }, { glType, 0 }, { glSmooth, 3 } } },
        { "Moonlit Haze",      { { wobDepth, 46 }, { wobRate, 0.3f }, { wobSpread, 80 }, { shimAmt, 42 }, { shimPitch, -12 }, { shimDecay, 88 }, { glassAmt, 0 }, { fType, 1 }, { cutoff, 3200 }, { reso, 25 }, { lfoShape, 1 }, { lfoSync, 0 }, { lfoRate, 0.2f }, { lfoCut, 45 }, { width, 135 }, { mix, 80 } } },
        { "Pump Pop",          { { wobDepth, 16 }, { shimAmt, 36 }, { glassAmt, 30 }, { glassKey, 9 }, { glassScale, 3 }, { lfoShape, 4 }, { lfoSync, 3 }, { lfoShim, 55 }, { lfoGlass, 40 }, { lfoCut, 35 }, { fType, 1 }, { cutoff, 6000 }, { reso, 28 } } },
    };
    return p;
}
} // namespace pinky
