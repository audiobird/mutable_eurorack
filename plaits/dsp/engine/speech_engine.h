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
// Various flavours of speech synthesis.

#ifndef PLAITS_DSP_ENGINE_SPEECH_ENGINE_H_
#define PLAITS_DSP_ENGINE_SPEECH_ENGINE_H_

#include "stmlib/dsp/hysteresis_quantizer.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/speech/lpc_speech_synth_controller.h"
#include "plaits/dsp/speech/naive_speech_synth.h"
#include "plaits/dsp/speech/sam_speech_synth.h"

namespace plaits {

class NaiveSpeechEngine {
public:
  void Init();
  void Reset() {}
  void Render(const EngineParameters &parameters, float *out, float *aux,
              size_t size);

private:
  std::array<std::array<float, kMaxBlockSize>, 2> temp_buffer_;
  NaiveSpeechSynth naive_speech_synth_;
  SAMSpeechSynth sam_speech_synth_;
};

class SamSpeechEngine {
public:
  void Init();
  void Reset() {}
  void Render(const EngineParameters &parameters, float *out, float *aux,
              size_t size);

private:
  LPCSpeechSynthWordBank lpc_speech_synth_word_bank_;
  std::array<std::array<float, kMaxBlockSize>, 2> temp_buffer_;
  LPCSpeechSynthController lpc_speech_synth_controller_;
  SAMSpeechSynth sam_speech_synth_;
};

class LPCSpeechEngine {
public:
  struct Params {
    // LPC_SPEECH_SYNTH_NUM_WORD_BANKS + 1
    int bank;
    float note;
    float timbre;
    float morph;
    float speed;
    float prosody;
    bool accent;
    bool trigger;
  };

  void Init();
  void Reset();
  void Render(const Params &parameters, float *out, float *aux, size_t size,
              bool *already_enveloped);

private:
  LPCSpeechSynthWordBank lpc_speech_synth_word_bank_;
  LPCSpeechSynthController lpc_speech_synth_controller_;
};

} // namespace plaits

#endif // PLAITS_DSP_ENGINE_SPEECH_ENGINE_H_
