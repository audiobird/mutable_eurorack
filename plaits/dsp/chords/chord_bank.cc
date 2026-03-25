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
// Chords: wavetable and divide-down organ/string machine.

#include "plaits/dsp/chords/chord_bank.h"

#include "stmlib/dsp/units.h"
#include <initializer_list>

namespace plaits {

using namespace stmlib;

struct ChordRatio {
  constexpr ChordRatio(std::initializer_list<float> interval, int count)
      : count{count} {
    for (auto [i, r] : std::ranges::zip_view(interval, ratio)) {
      r = std::pow(2, i / 12.f);
    }
  }

  std::array<float, 4> ratio{};
  int count{};
};

static constexpr std::array<ChordRatio, kChordNumChords> chords_ = {
    // Fixed Intervals
    ChordRatio{{0.00f, 0.01f, 11.99f, 12.00f}, 1}, // Octave
    ChordRatio{{0.00f, 7.00f, 7.01f, 12.00f}, 2},  // Fifth
    // Minor
    ChordRatio{{0.00f, 3.00f, 7.00f, 12.00f}, 3},  // Minor
    ChordRatio{{0.00f, 3.00f, 7.00f, 10.00f}, 4},  // Minor 7th
    ChordRatio{{0.00f, 3.00f, 10.00f, 14.00f}, 4}, // Minor 9th
    ChordRatio{{0.00f, 3.00f, 10.00f, 17.00f}, 4}, // Minor 11th
    // Major
    ChordRatio{{0.00f, 4.00f, 7.00f, 12.00f}, 3},  // Major
    ChordRatio{{0.00f, 4.00f, 7.00f, 11.00f}, 4},  // Major 7th
    ChordRatio{{0.00f, 4.00f, 11.00f, 14.00f}, 4}, // Major 9th
    // Colour Chords
    ChordRatio{{0.00f, 5.00f, 7.00f, 12.00f}, 3},  // Sus4
    ChordRatio{{0.00f, 2.00f, 9.00f, 16.00f}, 4},  // 69
    ChordRatio{{0.00f, 4.00f, 7.00f, 9.00f}, 4},   // 6th
    ChordRatio{{0.00f, 7.00f, 16.00f, 23.00f}, 4}, // 10th (Spread maj7)
    ChordRatio{{0.00f, 4.00f, 7.00f, 10.00f}, 4},  // Dominant 7th
    ChordRatio{{0.00f, 7.00f, 10.00f, 13.00f}, 4}, // Dominant 7th (b9)
    ChordRatio{{0.00f, 3.00f, 6.00f, 10.00f}, 4},  // Half Diminished
    ChordRatio{{0.00f, 3.00f, 6.00f, 9.00f}, 4},   // Fully Diminished
};

const float *ChordBank::ratios() const {
  return chords_[chord_idx_].ratio.data();
}

int ChordBank::num_notes() const { return chords_[chord_idx_].count; }

int ChordBank::ComputeChordInversion(float inversion, float *ratios,
                                     float *amplitudes) {
  const float *base_ratio = this->ratios();
  inversion = inversion * float(kChordNumNotes * kChordNumVoices);

  MAKE_INTEGRAL_FRACTIONAL(inversion);

  int num_rotations = inversion_integral / kChordNumNotes;
  int rotated_note = inversion_integral % kChordNumNotes;

  const float kBaseGain = 0.25f;

  int mask = 0;

  for (int i = 0; i < kChordNumNotes; ++i) {
    float transposition =
        0.25f *
        static_cast<float>(1 << ((kChordNumNotes - 1 + inversion_integral - i) /
                                 kChordNumNotes));
    int target_voice = (i - num_rotations + kChordNumVoices) % kChordNumVoices;
    int previous_voice = (target_voice - 1 + kChordNumVoices) % kChordNumVoices;

    if (i == rotated_note) {
      ratios[target_voice] = base_ratio[i] * transposition;
      ratios[previous_voice] = ratios[target_voice] * 2.0f;
      amplitudes[previous_voice] = kBaseGain * inversion_fractional;
      amplitudes[target_voice] = kBaseGain * (1.0f - inversion_fractional);
    } else if (i < rotated_note) {
      ratios[previous_voice] = base_ratio[i] * transposition;
      amplitudes[previous_voice] = kBaseGain;
    } else {
      ratios[target_voice] = base_ratio[i] * transposition;
      amplitudes[target_voice] = kBaseGain;
    }

    if (i == 0) {
      if (i >= rotated_note) {
        mask |= 1 << target_voice;
      }
      if (i <= rotated_note) {
        mask |= 1 << previous_voice;
      }
    }
  }
  return mask;
}

} // namespace plaits
