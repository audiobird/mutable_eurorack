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
// 6-operator FM synth.

#include "plaits/dsp/engine2/six_op_engine.h"

#include <algorithm>
#include <string_view>

#include "plaits/dsp/dsp.h"
#include "plaits/fm_patch_sysex.hh"
#include "synth/block.hh"

namespace plaits {

using namespace fm;
using namespace std;
using namespace stmlib;

void FMVoice::Init(fm::Algorithms<6> *algorithms) { voice_.Init(algorithms); }

void FMVoice::Render(float *buffer, size_t size) {
  voice_.Render(parameters_, buffer, size);
}

void FMVoice::LoadPatch(const fm::Patch *patch) {
  voice_.SetPatch(patch);
  lfo_.Set(patch->modulations);
}

void SixOpEngine::Init() {
  algorithms_.Init();
  for (int i = 0; i < kNumSixOpVoices; ++i) {
    voice_[i].Init(&algorithms_);
  }
}

using PatchBank = std::array<fm::Patch, kNumPatchesPerBank>;
using Banks = std::array<PatchBank, 3>;

static constexpr Banks bank = []() {
  Banks out{};
  for (auto i = 0u; i < out.size(); ++i) {
    for (auto p = 0u; p < kNumPatchesPerBank; ++p) {
      out[i][p].Unpack(plaits::fm_patches_table[i] + p * fm::Patch::SYX_SIZE);
    }
  }
  return out;
}();

void SixOpEngine::Render(const EngineParameters &parameters,
                         ToySynth::Synth::Bus &bus) {
  const auto patch_p = static_cast<unsigned>(parameters.harmonics);
  const auto patch_bank = patch_p / kNumPatchesPerBank;
  const auto patch_index = patch_p % kNumPatchesPerBank;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    active_voice_ = (active_voice_ + 1) % kNumSixOpVoices;
    voice_[active_voice_].LoadPatch(&bank[patch_bank][patch_index]);
    voice_[active_voice_].mutable_lfo()->Reset();
  }

  auto p = voice_[active_voice_].mutable_parameters();
  p->note = parameters.note;
  p->velocity = parameters.accent;
  p->envelope_control = parameters.morph;
  voice_[active_voice_].mutable_lfo()->Step(float(bus.size()));

  for (int i = 0; i < kNumSixOpVoices; ++i) {
    auto p = voice_[i].mutable_parameters();
    p->brightness = parameters.timbre;
    p->sustain = false;
    p->gate = (parameters.trigger & TRIGGER_HIGH) && (i == active_voice_);
    if (voice_[i].patch() != voice_[active_voice_].patch()) {
      voice_[i].mutable_lfo()->Step(float(bus.size()));
      voice_[i].set_modulations(voice_[i].lfo());
    } else {
      voice_[i].set_modulations(voice_[active_voice_].lfo());
    }
  }

  std::copy(acc_buffer_.data(),
            &acc_buffer_[(kNumSixOpVoices - 1) * bus.size()],
            temp_buffer_.data());

  std::fill(&temp_buffer_[(kNumSixOpVoices - 1) * bus.size()],
            &temp_buffer_[kNumSixOpVoices * bus.size()], 0.0f);

  rendered_voice_ = (rendered_voice_ + 1) % kNumSixOpVoices;

  voice_[rendered_voice_].Render(temp_buffer_.data(),
                                 bus.size() * kNumSixOpVoices);

  for (size_t i = 0; i < bus.size(); ++i) {
    bus[i].left =
        ToySynth::Fixed::from_float(SoftClip(temp_buffer_[i] * 0.25f));
  }
  copy(&temp_buffer_[bus.size()], &temp_buffer_[kNumSixOpVoices * bus.size()],
       &acc_buffer_[0]);
}

} // namespace plaits
