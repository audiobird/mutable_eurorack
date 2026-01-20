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
// Chiptune waveforms with arpeggiator.

#ifndef PLAITS_DSP_ENGINE_CHIPTUNE_ENGINE_H_
#define PLAITS_DSP_ENGINE_CHIPTUNE_ENGINE_H_

#include "plaits/dsp/chords/chord_bank.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/oscillator/super_square_oscillator.h"

namespace plaits {

class ChiptuneEngine {
public:
  ChiptuneEngine() {}
  ~ChiptuneEngine() {}

  void Init();
  void Reset();
  void LoadUserData(const uint8_t *user_data) {}
  void RenderChord(const EngineParameters &parameters, float *out, float *aux,
                   size_t size);

private:
  SuperSquareOscillator voice_[kChordNumVoices];

  ChordBank chords_;

  DISALLOW_COPY_AND_ASSIGN(ChiptuneEngine);
};

} // namespace plaits

#endif // PLAITS_DSP_ENGINE_CHIPTUNE_ENGINE_H_
