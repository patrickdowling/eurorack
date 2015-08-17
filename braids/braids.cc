// Copyright 2012 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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

#include "braids/drivers/platform.h"

#include <algorithm>

#include "stmlib/utils/dsp.h"
#include "stmlib/utils/ring_buffer.h"
#include "stmlib/system/system_clock.h"
#include "stmlib/system/uid.h"

#include "braids/drivers/dac.h"
#include "braids/drivers/debug_pin.h"
#include "braids/drivers/gate_input.h"
#include "braids/drivers/system.h"
#include "braids/cv_scaler.h"
#include "braids/envelope.h"
#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "braids/ui.h"

//#define DAC_TEST

using namespace braids;
using namespace std;
using namespace stmlib;

const size_t kNumBlocks = 4;
const size_t kBlockSize = 24;

MacroOscillator osc;
Envelope envelope;
CvScaler cv_scaler;
Dac dac;
DebugPin debug_pin;
GateInput gate_input;
SignatureWaveshaper ws;
System sys;
VcoJitterSource jitter_source;
Ui ui;

size_t current_sample;
volatile size_t playback_block;
volatile size_t render_block;
int16_t audio_samples[kNumBlocks][kBlockSize];
uint8_t sync_samples[kNumBlocks][kBlockSize];

bool trigger_detected_flag;
volatile bool trigger_flag;
uint16_t trigger_delay;

extern "C" {
  
void HardFault_Handler(void) { while (1); }
void MemManage_Handler(void) { while (1); }
void BusFault_Handler(void) { while (1); }
void UsageFault_Handler(void) { while (1); }
void NMI_Handler(void) { }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

}

extern "C" {

void SysTick_Handler() {
  ui.Poll();
}

void PLATFORM_TIM1_UP_IRQHandler(void) {
  if (!(TIM1->SR & TIM_IT_Update)) {
    return;
  }
  TIM1->SR = (uint16_t)~TIM_IT_Update;

  // TODO Original code outputs zeroes on DAC in this version, which is odd;
  // the first protoype board uses the original code and gets data, so it
  // might be something else that I'm missing right now.
  //
  // In any case, manually futzing the sample does the trick (pending proper
  // analysis) and outputs a waveform.
  const int32_t sample = static_cast<int32_t>(audio_samples[playback_block][current_sample] + 32768;
  dac.Write(sample);
  //dac.Write(audio_samples[playback_block][current_sample] + 32768);

  bool trigger_detected = gate_input.raised();
  sync_samples[playback_block][current_sample] = trigger_detected;
  trigger_detected_flag = trigger_detected_flag | trigger_detected;
  
  current_sample = current_sample + 1;
  if (current_sample >= kBlockSize) {
    current_sample = 0;
    playback_block = (playback_block + 1) % kNumBlocks;
  }

// TODO Trigger stuff -- how often?
// PipelinedScan would complete after kNumChannels x 3 aquisition stages of ADC?
// 
#if 0
  bool adc_scan_cycle_complete = adc.PipelinedScan();
  if (adc_scan_cycle_complete) {
    //ui.UpdateCv(adc.channel(0), adc.channel(1), adc.channel(2), adc.channel(3));
    if (trigger_detected_flag) {
      trigger_delay = settings.trig_delay()
          ? (1 << settings.trig_delay()) : 0;
      ++trigger_delay;
      trigger_detected_flag = false;
    }
    if (trigger_delay) {
      --trigger_delay;
      if (trigger_delay == 0) {
        trigger_flag = true;
      }
    }
  }
#endif
}

}

void Init() {
  sys.Init(F_CPU / 96000 - 1, true);
  settings.Init();
  ui.Init();
  system_clock.Init();
  cv_scaler.Init();
  gate_input.Init();
  debug_pin.Init();
  dac.Init();
  osc.Init();
  
  for (size_t i = 0; i < kNumBlocks; ++i) {
    fill(&audio_samples[i][0], &audio_samples[i][kBlockSize], 0);
    fill(&sync_samples[i][0], &sync_samples[i][kBlockSize], 0);
  }
  playback_block = kNumBlocks / 2;
  render_block = 0;
  current_sample = 0;
  
  envelope.Init();
  ws.Init(GetUniqueId(2));
  jitter_source.Init(GetUniqueId(1));
  sys.StartTimers();
}

const uint16_t bit_reduction_masks[] = {
    0xc000,
    0xe000,
    0xf000,
    0xf800,
    0xff00,
    0xfff0,
    0xffff };

const uint16_t decimation_factors[] = { 24, 12, 6, 4, 3, 2, 1 };

struct TrigStrikeSettings {
  uint8_t attack;
  uint8_t decay;
  uint8_t amount;
};

const TrigStrikeSettings trig_strike_settings[] = {
  { 0, 30, 30 },
  { 0, 40, 60 },
  { 0, 50, 90 },
  { 0, 60, 110 },
  { 0, 70, 90 },
  { 0, 90, 80 },
  { 60, 100, 70 },
  { 40, 72, 60 },
  { 34, 60, 20 },
};

void RenderBlock(const Parameters *parameters) {
  static uint16_t previous_pitch_adc_code = 0;
  static int32_t previous_pitch = 0;
  static int32_t previous_shape = 0;

  debug_pin.High();
  
  const TrigStrikeSettings& trig_strike = \
      trig_strike_settings[settings.GetValue(SETTING_TRIG_AD_SHAPE)];
  envelope.Update(trig_strike.attack, trig_strike.decay, 0, 0);
  uint16_t ad_value = envelope.Render();
  uint8_t ad_timbre_amount = settings.GetValue(SETTING_TRIG_DESTINATION) & 1
      ? trig_strike.amount
      : 0;
  
  if (ui.paques()) {
    osc.set_shape(MACRO_OSC_SHAPE_QUESTION_MARK);
  } else if (settings.meta_modulation()) {
    int32_t shape = parameters->values[VALUE_FM];
    shape -= settings.data().fm_cv_offset;
    if (shape > previous_shape + 2 || shape < previous_shape - 2) {
      previous_shape = shape;
    } else {
      shape = previous_shape;
    }
    shape = MACRO_OSC_SHAPE_LAST * shape >> 11;
    shape += settings.shape();
    if (shape >= MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META) {
      shape = MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META;
    } else if (shape <= 0) {
      shape = 0;
    }
    MacroOscillatorShape osc_shape = static_cast<MacroOscillatorShape>(shape);
    osc.set_shape(osc_shape);
    ui.set_meta_shape(osc_shape);
  } else {
    osc.set_shape(settings.shape());
  }
  uint16_t parameter_1 = parameters->values[VALUE_PARAM1] << 3;
  parameter_1 += static_cast<uint32_t>(ad_value) * ad_timbre_amount >> 9;
  if (parameter_1 > 32767) {
    parameter_1 = 32767;
  }
  osc.set_parameters(parameter_1, parameters->values[VALUE_PARAM2] << 3);
  
  // Apply hysteresis to ADC reading to prevent a single bit error to move
  // the quantized pitch up and down the quantization boundary.
  uint16_t pitch_adc_code = parameters->values[VALUE_PITCH];
  if (settings.pitch_quantization()) {
    if ((pitch_adc_code > previous_pitch_adc_code + 4) ||
        (pitch_adc_code < previous_pitch_adc_code - 4)) {
      previous_pitch_adc_code = pitch_adc_code;
    } else {
      pitch_adc_code = previous_pitch_adc_code;
    }
  }
  int32_t pitch = settings.adc_to_pitch(pitch_adc_code);
  if (settings.pitch_quantization() == PITCH_QUANTIZATION_QUARTER_TONE) {
    pitch = (pitch + 32) & 0xffffffc0;
  } else if (settings.pitch_quantization() == PITCH_QUANTIZATION_SEMITONE) {
    pitch = (pitch + 64) & 0xffffff80;
  }
  if (!settings.meta_modulation()) {
    pitch += settings.adc_to_fm(parameters->values[VALUE_FM]);
  }
  
  // Check if the pitch has changed to cause an auto-retrigger
  int32_t pitch_delta = pitch - previous_pitch;
  if (settings.data().auto_trig &&
      (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
    trigger_detected_flag = true;
  }
  previous_pitch = pitch;
  
  if (settings.vco_drift()) {
    int16_t jitter = jitter_source.Render(parameters->values[VALUE_PARAM2] << 3);
    pitch += (jitter >> 8);
  }

  if (pitch > 32767) {
    pitch = 32767;
  } else if (pitch < 0) {
    pitch = 0;
  }
  
  if (settings.vco_flatten()) {
    if (pitch > 16383) {
      pitch = 16383;
    }
    pitch = Interpolate88(lut_vco_detune, pitch << 2);
  }
  osc.set_pitch(pitch + settings.pitch_transposition());

  if (trigger_flag) {
    osc.Strike();
    envelope.Trigger(ENV_SEGMENT_ATTACK);
    ui.StepMarquee();
    trigger_flag = false;
  }
  
  uint8_t destination = settings.GetValue(SETTING_TRIG_DESTINATION);
  uint8_t* sync_buffer = sync_samples[render_block];
  int16_t* render_buffer = audio_samples[render_block];
  if (destination == 1) {
    // Disable hardsync when the trigger is used for envelopes.
    memset(sync_buffer, 0, kBlockSize);
  }
  osc.Render(sync_buffer, render_buffer, kBlockSize);
  
#ifdef DAC_TEST
  static volatile uint16_t s = 0;
  for (size_t i = 0; i < kBlockSize; ++i)
    render_buffer[i] = s++;
#else
  // Copy to DAC buffer with sample rate and bit reduction applied.
  int16_t sample = 0;
  size_t decimation_factor = decimation_factors[settings.data().sample_rate];
  uint16_t bit_mask = bit_reduction_masks[settings.data().resolution];
  int32_t gain = settings.GetValue(SETTING_TRIG_DESTINATION) & 2
      ? ad_value : 65535;
  for (size_t i = 0; i < kBlockSize; ++i) {
    if ((i % decimation_factor) == 0) {
      sample = render_buffer[i] & bit_mask;
      if (settings.signature()) {
        sample = ws.Transform(sample);
      }
    }
    render_buffer[i] = static_cast<int32_t>(sample) * gain >> 16;
  }
#endif
  render_block = (render_block + 1) % kNumBlocks;
  
  debug_pin.Low();

  ui.UpdateCv(cv_scaler.raw_values());
}

int main(void) {
  Init();
  Parameters parameters;
  while (1) {
    while (render_block != playback_block) {
      cv_scaler.Read(&parameters);
      RenderBlock(&parameters);
    }
    ui.DoEvents();
  }
}
