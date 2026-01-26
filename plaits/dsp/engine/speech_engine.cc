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

#include "plaits/dsp/engine/speech_engine.h"

#include "plaits/dsp/speech/lpc_speech_synth_words.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void LPCSpeechEngine::Init() {
  lpc_speech_synth_word_bank_.Init(word_banks_,
                                   LPC_SPEECH_SYNTH_NUM_WORD_BANKS);
  lpc_speech_synth_controller_.Init(&lpc_speech_synth_word_bank_);
}

void NaiveSpeechEngine::Init() {
  sam_speech_synth_.Init();
  naive_speech_synth_.Init();
}

void SamSpeechEngine::Init() {
  sam_speech_synth_.Init();
  lpc_speech_synth_word_bank_.Init(
      word_banks_,
      // for now, we wont use the last word bank, because for whatever reason it
      // crashes. that's okay though, because the words are kinda lame
      // TODO: remove bank, or replace with better words, fix crash
      LPC_SPEECH_SYNTH_NUM_WORD_BANKS - 1);
  lpc_speech_synth_controller_.Init(&lpc_speech_synth_word_bank_);
}

void LPCSpeechEngine::Reset() { lpc_speech_synth_word_bank_.Reset(); }

void NaiveSpeechEngine::Render(const EngineParameters &parameters, float *out,
                               float *aux, size_t size) {
  const float f0 = NoteToFrequency(parameters.note);

  float blend = parameters.harmonics;

  naive_speech_synth_.Render(parameters.trigger == TRIGGER_RISING_EDGE, f0,
                             parameters.morph, parameters.timbre,
                             temp_buffer_[0].data(), aux, out, size);

  sam_speech_synth_.Render(
      parameters.trigger == TRIGGER_RISING_EDGE, f0, parameters.morph,
      parameters.timbre, temp_buffer_[0].data(), temp_buffer_[1].data(), size);

  blend *= blend * (3.0f - 2.0f * blend);
  blend *= blend * (3.0f - 2.0f * blend);
  for (size_t i = 0; i < size; ++i) {
    aux[i] += (temp_buffer_[0][i] - aux[i]) * blend;
    out[i] += (temp_buffer_[1][i] - out[i]) * blend;
  }
}

void SamSpeechEngine::Render(const EngineParameters &parameters, float *out,
                             float *aux, size_t size) {
  const float f0 = NoteToFrequency(parameters.note);

  lpc_speech_synth_controller_.Render(false, parameters.trigger, -1, f0, 0.0f,
                                      0.0f, parameters.morph, parameters.timbre,
                                      1.0f, aux, out, size);

  sam_speech_synth_.Render(parameters.trigger, f0, parameters.morph,
                           parameters.timbre, temp_buffer_[0].data(),
                           temp_buffer_[1].data(), size);

  float blend = parameters.harmonics;
  blend *= blend * (3.0f - 2.0f * blend);
  blend *= blend * (3.0f - 2.0f * blend);
  for (size_t i = 0; i < size; ++i) {
    aux[i] += (temp_buffer_[0][i] - aux[i]) * blend;
    out[i] += (temp_buffer_[1][i] - out[i]) * blend;
  }
}

void LPCSpeechEngine::Render(const Params &parameters, float *out, float *aux,
                             size_t size, bool *already_enveloped) {
  const float f0 = NoteToFrequency(parameters.note);

  const int word_bank = parameters.bank - 1;

  const bool replay_prosody = word_bank >= 0;

  *already_enveloped = replay_prosody;

  lpc_speech_synth_controller_.Render(
      false, parameters.trigger, word_bank, f0, parameters.prosody,
      parameters.speed, parameters.morph, parameters.timbre,
      replay_prosody ? parameters.accent : 1.0f, aux, out, size);
}

} // namespace plaits
