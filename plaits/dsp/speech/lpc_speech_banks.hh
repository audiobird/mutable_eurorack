#pragma once

#include "lpc_speech_synth.h"
#include <span>

namespace plaits {

using FrameSpan = std::span<const LPCSpeechSynth::Frame>;

extern const std::array<std::span<const FrameSpan>, 4> bank;

} // namespace plaits
