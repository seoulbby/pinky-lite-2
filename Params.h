// Pinky Lite 2 - parameter ids, layout and factory presets
// Author: seoulbby
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace pinky::id
{
inline constexpr auto inGain   = "in_gain";
inline constexpr auto drive    = "drive";
inline constexpr auto mode     = "mode";
inline constexpr auto shape    = "shape";
inline constexpr auto bias     = "bias";
inline constexpr auto tone     = "tone";
inline constexpr auto xover    = "xover";
inline constexpr auto lowAmt   = "low_amt";
inline constexpr auto highAmt  = "high_amt";
inline constexpr auto quality  = "quality";
inline constexpr auto bits     = "crush_bits";
inline constexpr auto rate     = "crush_rate";
inline constexpr auto fType    = "filter_type";
inline constexpr auto cutoff   = "cutoff";
inline constexpr auto reso     = "reso";
inline constexpr auto width    = "width";
inline constexpr auto mix      = "mix";
inline constexpr auto outGain  = "out_gain";
inline constexpr auto lfoRate  = "lfo_rate";
inline constexpr auto lfoSync  = "lfo_sync";
inline constexpr auto lfoShape = "lfo_shape";
inline constexpr auto lfoDrive = "lfo_drive";
inline constexpr auto lfoCut   = "lfo_cutoff";
inline constexpr auto lfoCrush = "lfo_crush";
} // namespace pinky::id

namespace pinky
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // actual (un-normalised) values
};
const std::vector<Preset>& factoryPresets();
} // namespace pinky
