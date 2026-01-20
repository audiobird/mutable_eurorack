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

#include "plaits/dsp/engine2/chiptune_engine.h"

#include <algorithm>

namespace plaits {

using namespace std;
using namespace stmlib;

void ChiptuneEngine::Init() {
  for (int i = 0; i < kChordNumNotes; ++i) {
    voice_[i].Init();
  }

  chords_.Init();
}

void ChiptuneEngine::Reset() { chords_.Reset(); }

void ChiptuneEngine::RenderChord(const EngineParameters &parameters, float *out,
                                 float *aux, size_t size) {
  const float f0 = NoteToFrequency(parameters.note);
  const float shape = parameters.morph * 0.995f;

  float ratios[kChordNumVoices];
  float amplitudes[kChordNumVoices];

  chords_.set_chord(static_cast<int>(parameters.harmonics));
  chords_.ComputeChordInversion(parameters.timbre, ratios, amplitudes);
  for (int j = 1; j < kChordNumVoices; j += 2) {
    amplitudes[j] = -amplitudes[j];
  }

  fill(&out[0], &out[size], 0.0f);
  for (int voice = 0; voice < kChordNumVoices; ++voice) {
    const float voice_f0 = f0 * ratios[voice];
    voice_[voice].Render(voice_f0, shape, aux, size);
    for (size_t j = 0; j < size; ++j) {
      out[j] += aux[j] * amplitudes[voice];
    }
  }
}

} // namespace plaits
