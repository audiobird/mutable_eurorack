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
// Chord bank shared by several engines.

#ifndef PLAITS_DSP_CHORDS_CHORD_BANK_H_
#define PLAITS_DSP_CHORDS_CHORD_BANK_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "stmlib/utils/buffer_allocator.h"

#include <algorithm>
#include <array>

namespace plaits {

const int kChordNumNotes = 4;
const int kChordNumVoices = kChordNumNotes + 1;

// #define JON_CHORDS

const int kChordNumChords = 17;

class ChordBank {
public:
  ChordBank() {}
  ~ChordBank() {}

  void Reset();

  int ComputeChordInversion(float inversion, float *ratios, float *amplitudes);

  // this is the kind of thing that should be done at compile time
  inline void Sort() {
    for (int i = 0; i < kChordNumNotes; ++i) {
      float r = ratio(i);
      while (r > 2.0f) {
        r *= 0.5f;
      }
      sorted_ratios_[i] = r;
    }
    std::sort(&sorted_ratios_[0], &sorted_ratios_[kChordNumNotes]);
  }

  inline void set_chord(int idx) { chord_idx_ = idx; }

  inline int chord_index() const { return chord_idx_; }

  inline const float *ratios() const {
    return &ratios_[chord_index() * kChordNumNotes];
  }

  inline float ratio(int note) const {
    return ratios_[chord_index() * kChordNumNotes + note];
  }

  inline float sorted_ratio(int note) const { return sorted_ratios_[note]; }

  inline int num_notes() const { return note_count_[chord_index()]; }

private:
  int chord_idx_{};
  std::array<float, kChordNumNotes * kChordNumChords> ratios_;
  std::array<float, kChordNumNotes> sorted_ratios_;
  std::array<int, kChordNumChords> note_count_;

  DISALLOW_COPY_AND_ASSIGN(ChordBank);
};

} // namespace plaits

#endif // PLAITS_DSP_CHORDS_CHORD_BANK_H_
