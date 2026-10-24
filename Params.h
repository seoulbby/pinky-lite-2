// Pinky Lite 2 - parameter ids, layout and factory presets
// Author: seoulbby
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace pinky::id
{
inline constexpr auto inGain    = "in_gain";
inline constexpr auto outGain   = "out_gain";
inline constexpr auto mix       = "mix";
inline constexpr auto width     = "width";
inline constexpr auto tone      = "tone";

inline constexpr auto wobDepth  = "wob_depth";
inline constexpr auto wobRate   = "wob_rate";
inline constexpr auto wobSpread = "wob_spread";

inline constexpr auto shimAmt   = "shim_amt";
inline constexpr auto shimPitch = "shim_pitch";
inline constexpr auto shimDecay = "shim_decay";

inline constexpr auto glassAmt    = "glass_amt";
inline constexpr auto glassDecay  = "glass_decay";
inline constexpr auto glassSpread = "glass_spread";
inline constexpr auto glassKey    = "glass_key";
inline constexpr auto glassScale  = "glass_scale";

inline constexpr auto glAmt     = "gl_amt";
inline constexpr auto glChance  = "gl_chance";
inline constexpr auto glSmooth  = "gl_smooth";
inline constexpr auto glDiv     = "gl_div";
inline constexpr auto glType    = "gl_type";

inline constexpr auto bits      = "crush_bits";
inline constexpr auto rate      = "crush_rate";

inline constexpr auto fType     = "filter_type";
inline constexpr auto cutoff    = "cutoff";
inline constexpr auto reso      = "reso";

inline constexpr auto lfoRate   = "lfo_rate";
inline constexpr auto lfoSync   = "lfo_sync";
inline constexpr auto lfoShape  = "lfo_shape";
inline constexpr auto lfoWob    = "lfo_wob";
inline constexpr auto lfoShim   = "lfo_shim";
inline constexpr auto lfoGlass  = "lfo_glass";
inline constexpr auto lfoCut    = "lfo_cutoff";
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
