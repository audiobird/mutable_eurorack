// Copyright 2016 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Feeds frames to the LPC10 speech synth.

#ifndef PLAITS_DSP_SPEECH_LPC_SPEECH_SYNTH_CONTROLLER_H_
#define PLAITS_DSP_SPEECH_LPC_SPEECH_SYNTH_CONTROLLER_H_

#include "plaits/dsp/speech/lpc_speech_banks.hh"
#include "plaits/dsp/speech/lpc_speech_synth.h"

#include "stmlib/utils/buffer_allocator.h"
#include <array>
#include <span>

namespace plaits {

inline constexpr int kLPCSpeechSynthMaxWords = 32;
inline constexpr int kLPCSpeechSynthMaxFrames = 1024;
inline constexpr int kLPCSpeechSynthNumVowels = 5;
inline constexpr int kLPCSpeechSynthNumConsonants = 10;
inline constexpr int kLPCSpeechSynthNumPhonemes =
    kLPCSpeechSynthNumVowels + kLPCSpeechSynthNumConsonants;
inline constexpr float kLPCSpeechSynthFPS = 40.0f;

class LPCSpeechSynthController {
public:
  void Init();

  void RenderNoBank(bool trigger, float frequency, float prosody_amount,
                    float speed, float address, float formant_shift, float gain,
                    float *output, size_t size);

  void Render(bool trigger, int bank, float frequency, float prosody_amount,
              float speed, float address, float formant_shift, float gain,
              float *output, size_t size);

private:
  float clock_phase_{};
  std::array<float, 2> sample_{};
  std::array<float, 2> next_sample_{};
  float gain_{};
  LPCSpeechSynth synth_;

  std::span<const LPCSpeechSynth::Frame> cur_word{plaits::bank[0][0]};
  int playback_frame_{-1};
  int last_playback_frame_{-1};
  size_t remaining_frame_samples_{};
};

}; // namespace plaits

#endif // PLAITS_DSP_SPEECH_LPC_SPEECH_SYNTH_CONTROLLER_H_
