// Copyright 2021 Emilie Gillet.
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
// DX7 voice.

#ifndef PLAITS_DSP_FM_VOICE_H_
#define PLAITS_DSP_FM_VOICE_H_

#include "conf/toy_synth.hh"
#include "stmlib/stmlib.h"

#include "plaits/dsp/fm/algorithms.h"
#include "plaits/dsp/fm/dx_units.h"
#include "plaits/dsp/fm/envelope.h"
#include "plaits/dsp/fm/patch.h"
#include <array>

// When enabled, the amplitude modulation LFO linearly modulates the amplitude
// of an operator. Otherwise, a more complex formula involving an exponential
// is used, to match Dexed's output.
// #define FAST_OP_LEVEL_MODULATION

namespace plaits {

namespace fm {

template <int num_operators> class Voice {
public:
  static constexpr auto sample_rate_ = ToySynth::sample_rate;

  struct Parameters {
    bool sustain{};
    bool gate{};
    float note{48.f};
    float velocity{.5f};
    float brightness{.5f};
    float envelope_control{.5f};
    float pitch_mod{};
    float amp_mod{};
  };

  inline void Init(const Algorithms<num_operators> *algorithms) {
    algorithms_ = algorithms;

    for (int i = 0; i < num_operators; ++i) {
      operator_[i].Reset();
    }

    patch_ = NULL;
  }

  inline void SetPatch(const Patch *patch) {
    patch_ = patch;
    dirty_ = true;
  }

  // Pre-compute everything that can be pre-computed once a patch is loaded:
  // - envelope constants
  // - frequency ratios
  inline bool Setup() {
    if (!dirty_) {
      return false;
    }

    pitch_envelope_.Set(patch_->pitch_envelope.rate,
                        patch_->pitch_envelope.level);
    for (int i = 0; i < num_operators; ++i) {
      const Patch::Operator &op = patch_->op[i];

      int level = OperatorLevel(op.level);
      operator_envelope_[i].Set(op.envelope.rate, op.envelope.level, level);

      // The level increase caused by keyboard scaling plus velocity
      // scaling should not exceed this number - otherwise it would be
      // equivalent to have an operator with a level above 99.
      level_headroom_[i] = float(127 - level);

      // Pre-compute frequency ratios. Encode the base frequency
      // (1Hz or the root note) as the sign of the ratio.
      float sign = op.mode == 0 ? 1.0f : -1.0f;
      ratios_[i] = sign * FrequencyRatio(op);
    }
    dirty_ = false;
    return true;
  }

  inline float op_level(int i) const { return level_[i]; }

  inline void Render(const Parameters &parameters, float *temp, float *out,
                     float *aux, size_t size) {
    float *buffers[4] = {out, aux, temp, temp};
    Render(parameters, buffers, size);
  }

  inline void Render(const Parameters &parameters, float *temp, size_t size) {
    float *buffers[4] = {temp, temp + size, temp + 2 * size, temp + 2 * size};
    Render(parameters, buffers, size);
  }

  inline void Render(const Parameters &parameters, float *buffers[4],
                     size_t size) {
    if (Setup()) {
      // This prevents a CPU overrun, since there is not enough CPU to perform
      // both a patch setup and a full render in the time alloted for
      // a render. As a drawback, this causes a 0.5ms blank before a new
      // patch starts playing. But this is a clean blank, as opposed to a
      // glitchy overrun.
      return;
    }

    const float envelope_rate = float(size);
    const float ad_scale =
        Pow2Fast<1>((0.5f - parameters.envelope_control) * 8.0f);
    const float r_scale =
        Pow2Fast<1>(-fabsf(parameters.envelope_control - 0.3f) * 8.0f);
    const float gate_duration = 1.5f * sample_rate_;
    const float envelope_sample = gate_duration * parameters.envelope_control;

    // Apply LFO and pitch envelope modulations.
    const float pitch_envelope =
        parameters.sustain
            ? pitch_envelope_.RenderAtSample(envelope_sample, gate_duration)
            : pitch_envelope_.Render(parameters.gate, envelope_rate, ad_scale,
                                     r_scale);
    const float pitch_mod = pitch_envelope + parameters.pitch_mod;
    const float f0 = a0_ * 0.25f *
                     stmlib::SemitonesToRatioSafe(parameters.note - 9.0f +
                                                  pitch_mod * 12.0f);

    // Sample the note and velocity (used for scaling) only when a trigger
    // is received, or constantly when we are in free-running mode.
    const bool note_on = parameters.gate && !gate_;
    gate_ = parameters.gate;
    if (note_on || parameters.sustain) {
      normalized_velocity_ = NormalizeVelocity(parameters.velocity);
      note_ = parameters.note;
    }

    // Reset operator phase if a note on is detected & if the patch requires it.
    if (note_on && patch_->reset_phase) {
      for (int i = 0; i < num_operators; ++i) {
        operator_[i].phase = 0;
      }
    }

    // Compute frequencies and amplitudes.
    float f[num_operators];
    float a[num_operators];
    for (int i = 0; i < num_operators; ++i) {
      const Patch::Operator &op = patch_->op[i];

      f[i] = ratios_[i] * (ratios_[i] < 0.0f ? -one_hz_ : f0);

      const float rate_scaling = RateScaling(note_, op.rate_scaling);
      float level = parameters.sustain
                        ? operator_envelope_[i].RenderAtSample(envelope_sample,
                                                               gate_duration)
                        : operator_envelope_[i].Render(
                              parameters.gate, envelope_rate * rate_scaling,
                              ad_scale, r_scale);
      const float kb_scaling = KeyboardScaling(note_, op.keyboard_scaling);
      const float velocity_scaling =
          normalized_velocity_ * float(op.velocity_sensitivity);
      const float brightness = algorithms_->is_modulator(patch_->algorithm, i)
                                   ? (parameters.brightness - 0.5f) * 32.0f
                                   : 0.0f;

      level += 0.125f * std::min(kb_scaling + velocity_scaling + brightness,
                                 level_headroom_[i]);

      level_[i] = level;

      const float sensitivity = AmpModSensitivity(op.amp_mod_sensitivity);
#ifdef FAST_OP_LEVEL_MODULATION
      const float level_mod = 1.0f - sensitivity * parameters.amp_mod;
      a[i] = Pow2Fast<2>(-14.0f + level) * level_mod;
#else
      const float log_level_mod = sensitivity * parameters.amp_mod - 1.0f;
      const float level_mod = 1.0f - Pow2Fast<2>(6.4f * log_level_mod);
      a[i] = Pow2Fast<2>(-14.0f + level * level_mod);
#endif // FAST_LINEAR_AMPLITUDE_MODULATION
    }

    for (int i = 0; i < num_operators;) {
      const typename Algorithms<num_operators>::RenderCall &call =
          algorithms_->render_call(patch_->algorithm, i);
      (*call.render_fn)(&operator_[i], &f[i], &a[i], feedback_state_.data(),
                        patch_->feedback, buffers[call.input_index],
                        buffers[call.output_index], size);
      i += call.n;
    }
  }

private:
  const Algorithms<num_operators> *algorithms_;
  static constexpr float one_hz_ = 1.f / sample_rate_;
  static constexpr float a0_ = 55.f / sample_rate_;

  bool gate_{};

  static constexpr float native_sr = 44100.0f; // Legacy sample rate.
  static constexpr float envelope_scale = native_sr * one_hz_;
  std::array<Operator, num_operators> operator_{};
  std::array<OperatorEnvelope<envelope_scale>, num_operators>
      operator_envelope_{};
  PitchEnvelope<envelope_scale> pitch_envelope_{};

  float normalized_velocity_{10.f};
  float note_{48.f};

  std::array<float, num_operators> ratios_{};
  std::array<float, num_operators> level_headroom_{};
  std::array<float, num_operators> level_{};

  std::array<float, 2> feedback_state_{};

  const Patch *patch_;

  bool dirty_{true};
};

} // namespace fm

} // namespace plaits

#endif // PLAITS_DSP_FM_VOICE_H_
