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

#include "plaits/dsp/speech/lpc_speech_synth_controller.h"

#include <algorithm>

#include "plaits/dsp/speech/lpc_speech_banks.hh"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"
#include "stmlib/dsp/units.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void LPCSpeechSynthController::Init() { synth_.Init(); }

static constexpr LPCSpeechSynth::Frame phonemes_[kLPCSpeechSynthNumPhonemes] = {
    {192, 80, -18368, 11584, 52, 29, 23, 14, -17, 79, 37, 4},
    {192, 80, -14528, 1536, 38, 29, 11, 14, -41, 79, 57, 4},
    {192, 80, 14528, 9216, 25, -54, -70, 36, 19, 79, 57, 22},
    {192, 80, -14528, -13440, 38, 57, 57, 14, -53, 7, 37, 77},
    {192, 80, -26368, 4160, 11, 15, -1, 36, -41, 31, 77, 22},
    {15, 0, 5184, 9216, -29, -12, 0, 0, 0, 0, 0, 0},
    {10, 0, 27968, 17856, 25, 43, -24, -20, -53, 55, -4, -51},
    {128, 160, 14528, -3712, -43, -26, -24, -20, -53, 55, -4, -51},
    {128, 160, 10048, 11584, -16, 15, 0, 0, 0, 0, 0, 0},
    {224, 100, 18368, -13440, -97, -26, -12, -53, -41, 7, 57, 32},
    {192, 80, -10048, 9216, -70, 15, 34, -20, -17, 31, -24, 22},
    {96, 160, -18368, 17856, -29, -12, -35, 3, -5, 7, 37, 22},
    {64, 80, -21632, -6272, -83, 29, 57, 3, -5, 7, 16, 32},
    {192, 80, 0, -1088, 11, -26, -24, -9, -5, 55, 37, 22},
    {64, 80, 21632, -17536, -97, 85, 57, -20, -17, 31, -4, 59}};

void LPCSpeechSynthController::RenderNoBank(bool trigger, float frequency,
                                            float prosody_amount, float speed,
                                            float address, float formant_shift,
                                            float gain, float *output,
                                            size_t size) {
  const float rate_ratio = SemitonesToRatio((formant_shift - 0.5f) * 36.0f);
  const float rate = rate_ratio / 6.0f;

  // All utterances have been normalized for an average f0 of 100 Hz.
  const float pitch_shift = frequency / (rate_ratio * kLPCSpeechSynthDefaultF0 /
                                         kCorrectedSampleRate);
  const float time_stretch = SemitonesToRatio(
      -speed * 24.0f +
      (formant_shift < 0.4f
           ? (formant_shift - 0.4f) * -45.0f
           : (formant_shift > 0.6f ? (formant_shift - 0.6f) * -45.0f : 0.0f)));

  static constexpr int num_frames = kLPCSpeechSynthNumVowels;

  auto frames = phonemes_;

  if (trigger) {
    // Pick a pseudo-random consonant, and play it for the duration of a
    // frame.
    int r = (address + 3.0f * formant_shift + 7.0f * frequency) * 8.0f;
    playback_frame_ = (r % kLPCSpeechSynthNumConsonants);
    playback_frame_ += kLPCSpeechSynthNumVowels;
    last_playback_frame_ = playback_frame_ + 1;
    remaining_frame_samples_ = 0;
  }

  if (playback_frame_ == -1 && remaining_frame_samples_ == 0) {
    synth_.PlayFrame(
        frames, address * (static_cast<float>(num_frames) - 1.0001f), true);
  } else {
    if (remaining_frame_samples_ == 0) {
      synth_.PlayFrame(frames, float(playback_frame_), false);
      remaining_frame_samples_ =
          kSampleRate / kLPCSpeechSynthFPS * time_stretch;
      ++playback_frame_;
      if (playback_frame_ >= last_playback_frame_) {
        playback_frame_ = -1;
      }
    }
    remaining_frame_samples_ -= min(size, remaining_frame_samples_);
  }

  ParameterInterpolator gain_modulation(&gain_, gain, size);

  float this_sample[2];
  while (size--) {
    copy(&next_sample_[0], &next_sample_[2], &this_sample[0]);
    fill(&next_sample_[0], &next_sample_[2], 0.0f);

    clock_phase_ += rate;
    if (clock_phase_ >= 1.0f) {
      clock_phase_ -= 1.0f;
      float reset_time = clock_phase_ / rate;
      float new_sample[2];

      synth_.Render(prosody_amount, pitch_shift, &new_sample[0], &new_sample[1],
                    1);

      float discontinuity[2] = {new_sample[0] - sample_[0],
                                new_sample[1] - sample_[1]};
      this_sample[0] += discontinuity[0] * ThisBlepSample(reset_time);
      next_sample_[0] += discontinuity[0] * NextBlepSample(reset_time);
      this_sample[1] += discontinuity[1] * ThisBlepSample(reset_time);
      next_sample_[1] += discontinuity[1] * NextBlepSample(reset_time);
      copy(&new_sample[0], &new_sample[2], &sample_[0]);
    }
    next_sample_[0] += sample_[0];
    next_sample_[1] += sample_[1];
    const float gain = gain_modulation.Next();
    *output++ = this_sample[1] * gain;
  }
}

void LPCSpeechSynthController::Render(bool trigger, int bank, float frequency,
                                      float prosody_amount, float speed,
                                      float address, float formant_shift,
                                      float gain, float *output, size_t size) {
  const float rate_ratio = SemitonesToRatio((formant_shift - 0.5f) * 36.0f);
  const float rate = rate_ratio / 6.0f;

  // All utterances have been normalized for an average f0 of 100 Hz.
  const float pitch_shift = frequency / (rate_ratio * kLPCSpeechSynthDefaultF0 /
                                         kCorrectedSampleRate);
  const float time_stretch = SemitonesToRatio(
      -speed * 24.0f +
      (formant_shift < 0.4f
           ? (formant_shift - 0.4f) * -45.0f
           : (formant_shift > 0.6f ? (formant_shift - 0.6f) * -45.0f : 0.0f)));

  auto frames = cur_word.data();

  if (trigger) {
    const auto &cur_bank = plaits::bank[bank];
    cur_word = cur_bank[static_cast<uint32_t>(address * cur_bank.size())];
    playback_frame_ = 0;
    remaining_frame_samples_ = 0;
  }

  if (playback_frame_ == -1 && remaining_frame_samples_ == 0) {
  } else {
    if (remaining_frame_samples_ == 0) {
      synth_.PlayFrame(frames, float(playback_frame_), false);
      remaining_frame_samples_ =
          kSampleRate / kLPCSpeechSynthFPS * time_stretch;
      ++playback_frame_;
      if (static_cast<uint32_t>(playback_frame_) >= cur_word.size() - 1) {
        playback_frame_ = cur_word.size() - 2;
      }
    }
    remaining_frame_samples_ -= min(size, remaining_frame_samples_);
  }

  ParameterInterpolator gain_modulation(&gain_, gain, size);

  float this_sample[2];
  while (size--) {
    copy(&next_sample_[0], &next_sample_[2], &this_sample[0]);
    fill(&next_sample_[0], &next_sample_[2], 0.0f);

    clock_phase_ += rate;
    if (clock_phase_ >= 1.0f) {
      clock_phase_ -= 1.0f;
      float reset_time = clock_phase_ / rate;
      float new_sample[2];

      synth_.Render(prosody_amount, pitch_shift, &new_sample[0], &new_sample[1],
                    1);

      float discontinuity[2] = {new_sample[0] - sample_[0],
                                new_sample[1] - sample_[1]};
      this_sample[0] += discontinuity[0] * ThisBlepSample(reset_time);
      next_sample_[0] += discontinuity[0] * NextBlepSample(reset_time);
      this_sample[1] += discontinuity[1] * ThisBlepSample(reset_time);
      next_sample_[1] += discontinuity[1] * NextBlepSample(reset_time);
      copy(&new_sample[0], &new_sample[2], &sample_[0]);
    }
    next_sample_[0] += sample_[0];
    next_sample_[1] += sample_[1];
    const float gain = gain_modulation.Next();
    *output++ = this_sample[1] * gain;
  }
}

} // namespace plaits
