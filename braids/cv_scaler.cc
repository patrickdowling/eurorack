#include <algorithm>
#include "stmlib/dsp/dsp.h"
#include "braids/cv_scaler.h"
#include "braids/settings.h"

namespace braids {

void CvScaler::Init(CalibrationData *calibration_data) {
  cv_adc_.Init();
  pot_adc_.Init();
  calibration_data_ = calibration_data;

  for (size_t pot = 0; pot < POT_LAST; ++pot)
    pots_[pot].Init();
}

/*
  Fun things to think about:
  - Instead of int32_t and clipping use SSAT?
  - re-assignable pots & stuff

  TODO Check generated code size/length/smell
  TODO Check if array of LockableSmoothedPot might be better as struct of arrays
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

  UpdatePot<POT_PITCH, 32>();
  UpdatePot<POT_FINE, 32>();
  UpdatePot<POT_FM, 32>();
  UpdatePot<POT_PARAM1, 16>();
  UpdatePot<POT_MOD, 16>();
  UpdatePot<POT_PARAM2, 16>();

  // trigger next ADC scan
  pot_adc_.Convert();
  cv_adc_.Sample();

  int32_t pitch_cv = readCV<CV_ADC_VOCT>();
  int32_t pitch_coarse = readAttenuverterPot<POT_PITCH>();

  parameters->pitch = adc_to_pitch(pitch_cv >> 4, pitch_coarse >> 4);

  // expects int32_t, No shift required
  parameters->pitch_fine = readAttenuverterPot<POT_FINE>();

  // TODO used calibrated offset?
  int32_t fm = attenuvert(readCV<CV_ADC_FM>(),
                          readAttenuverterPot<POT_FM>());
  parameters->fm = stmlib::Clip16(fm) >> 4;

  int32_t value;

  // cv input is 10v (-5,5), so scale x 2 to allow full range change for (-5,0) and (0,5)

  value = readPot<POT_PARAM1>()
      + attenuvert(2 * readCV<CV_ADC_PARAM1>(),
                   readAttenuverterPot<POT_MOD>());
  parameters->parameters[0] = stmlib::ClipU16(value) >> 4;

  value = readPot<POT_PARAM2>() + 2 * readCV<CV_ADC_PARAM2>();
  parameters->parameters[1] = stmlib::ClipU16(value) >> 4;
}

int32_t CvScaler::adc_to_pitch(int32_t pitch_cv, int32_t pitch_coarse) {
  // NOTE: pitch_cv and pitch_coarse are 12bit

  const PitchRange pitch_range = settings.pitch_range();
  int32_t pitch;

  if (pitch_range == PITCH_RANGE_EXTERNAL ||
      pitch_range == PITCH_RANGE_LFO) {
  // coarse moves around the note received on the V/Oct input
    pitch = pitch_cv * calibration_data_->pitch_cv_scale >> 12;
    pitch += (1024 + pitch_coarse) * kCoarseRange >> 12;
//    pitch += calibration_data_->pitch_cv_offset;
  } else if (pitch_range == PITCH_RANGE_FREE) {
  // coarse moves centered around C3
    pitch = 60 << 7;
    pitch += pitch_coarse * kCoarseRange >> 12;
    pitch += pitch_cv * calibration_data_->pitch_cv_scale >> 12;
  } else if (pitch_range == PITCH_RANGE_440) {
  // locks the oscillator frequency to 440 Hz exactly
    pitch = 69 << 7;
  } else {
  // XTND (extended) provides a larger frequency range, but disables accurate V/Oct scaling as a side effect.
    pitch = (pitch_cv + pitch_coarse) * 9 >> 1;
  }

  return pitch;
}

void CvScaler::LockPots(uint16_t mask, int32_t *snap_values) {
  for (size_t pot = 0; pot < POT_LAST; ++pot, mask >>= 1) {
    if (mask & 0x1)
      pots_[pot].Lock(snap_values[pot]);
   }
}

void CvScaler::UnlockPots() {
  for (size_t pot = 0; pot < POT_LAST; ++pot)
    pots_[pot].Unlock();
}

} // namespace braids
