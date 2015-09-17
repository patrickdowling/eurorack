#include <algorithm>
#include "stmlib/dsp/dsp.h"
#include "braids/cv_scaler.h"
#include "braids/settings.h"

namespace braids {

void CvScaler::Init(CalibrationData *calibration_data) {
  cv_adc_.Init();
  pot_adc_.Init();
  calibration_data_ = calibration_data;
  scan_ = 0;

  std::fill(smoothed_, smoothed_ + POT_LAST, 0);
  std::fill(locked_, locked_ + POT_LAST, 0);

  locked_mask_ = 0;
  snapped_mask_ = 0;
}

/*
  Fun things to think about:
  - Instead of int32_t and clipping use SSAT?
  - re assignable pots & stuff

  Averaging code from internal_adc.h which smooths by factor 2^8?
  TODO compare with running average?

  int32_t v = (static_cast<int32_t>(adc_.raw_value(ADC_FINE_POT)) - 32768) << 8;
  int32_t delta = v - smoothed_[ADC_FINE_POT];
  smoothed_[ADC_FINE_POT] += (delta >> 8);
  parameters->fine = smoothed_[ADC_FINE_POT] >> 8;
*/

inline int32_t attenuvert(int32_t cv, int32_t attenuverter) {
  // "borrowed" from elements/cv_scaler.cc, LAW_QUADRATIC_BIPOLAR
  // Have an FPU might as well use it even if results are truncated to uint16_t
  float x = static_cast<float>(attenuverter) / 32768.f;
  float x2 = x * x;

  return attenuverter < 0
      ? static_cast<int32_t>(-x2 * static_cast<float>(cv))
      : static_cast<int32_t>( x2 * static_cast<float>(cv));
}

void CvScaler::Read(Parameters *parameters) {

  // Note: Original Braids had left-aligned internal ADC (FINE pot) but
  // right-aligned data from MCP. internal_adc is set to ADC_DataAlign_Left
  // for all channels so shift needed. ADCs in both cases are 12b anyway.

  // separate reading/smoothing from scaling/offsets to allow locking/re-use
  // of channels for other things (e.g. UI value setting)

  pot_smooth<POT_PITCH, 32>();
  pot_smooth<POT_FINE, 32>();
  pot_smooth<POT_FM, 16>();
  pot_smooth<POT_PARAM1, 16>();
  pot_smooth<POT_MOD, 16>();
  pot_smooth<POT_PARAM2, 16>();

  // trigger next ADC scan
  pot_adc_.Convert();
  cv_adc_.Sample();

  int32_t pitch_cv = readCvUni<CV_ADC_VOCT>();
  int32_t pitch_coarse = readPot<POT_PITCH, 0>();

  parameters->pitch = adc_to_pitch(pitch_cv, pitch_coarse);

  // expects int32_t, No shift required
  parameters->pitch_fine = readPot<POT_FINE, -32768>();

  // TODO used calibrated offset?
  int32_t fm = attenuvert(readCvBi<CV_ADC_FM>(),
                          readPot<POT_FM, -32768>());
  parameters->fm = stmlib::Clip16(fm) >> 4;

  int32_t value;

  // cv input is 10v (-5,5), so scale x 2 to allow full range change for (-5,0) and (0,5)

  value = readPot<POT_PARAM1, 0>()
      + attenuvert(2 * readCvBi<CV_ADC_PARAM1>(),
                   readPot<POT_MOD, -32768>());
  CONSTRAIN(value, 0, 65535)
  parameters->parameters[0] = value >> 4;

  value = readPot<POT_PARAM2, 0>() + 2 * readCvBi<CV_ADC_PARAM2>();
  CONSTRAIN(value, 0, 65535)
  parameters->parameters[1] = value >> 4;
}

int32_t CvScaler::adc_to_pitch(int32_t pitch_cv, int32_t pitch_coarse) {
  const PitchRange pitch_range = settings.pitch_range();
  int32_t pitch;

  // pitch_coarse as 12 bit = 4096 / 128 = 32 semitones = 2.8 octaves, so for four octaves
  // scale by (48 * 128) / 4096

  // TODO pitch_cv_offset

  if (pitch_range == PITCH_RANGE_EXTERNAL ||
      pitch_range == PITCH_RANGE_LFO) {
  // +/- 4 octaves around the note received on the V/Oct input
    pitch = pitch_cv * calibration_data_->pitch_cv_scale >> 12;
    pitch += pitch_coarse * (48 * 128) >> 12;
  } else if (pitch_range == PITCH_RANGE_FREE) {
  // +/- 4 octave centered around C3
    pitch = 60 << 7 << 4;
    pitch += pitch_coarse * (48 * 128) >> 12;
    pitch += pitch_cv * calibration_data_->pitch_cv_scale >> 12;
  } else if (pitch_range == PITCH_RANGE_440) {
  // locks the oscillator frequency to 440 Hz exactly
    pitch = 69 << 7 << 4;
  } else {
  // XTND (extended) provides a larger frequency range, but disables accurate V/Oct scaling as a side effect.
    pitch = pitch_cv + pitch_coarse;
  }

  return pitch >> 4;
}

// FIXME SLight bug when Lock -> Unlock -> Lock
// This needs to respect the values that haven't been re-snapped yet

void CvScaler::LockPots(uint16_t mask, int32_t *snap) {
  std::copy(smoothed_, smoothed_ + POT_LAST, locked_);
  std::copy(snap, snap + POT_LAST, snapped_);
  snapped_mask_ = mask;
  locked_mask_ = mask;
}

void CvScaler::UnlockPots() {
  std::copy(locked_, locked_ + POT_LAST, snapped_);
  snapped_mask_ = locked_mask_;
  locked_mask_ = 0;
}

} // namespace braids
