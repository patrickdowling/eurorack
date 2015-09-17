// Copyright 2015 Patrick Dowling.
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
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
// CV and pot input scaling/mixing/processing stuff
// Based on cv_scalers in other MI eurorack projects

#ifndef BRAIDS_CV_SCALER_H_
#define BRAIDS_CV_SCALER_H_

#include "stmlib/stmlib.h"
#include "braids/settings.h"
#include "braids/drivers/cv_adc.h"
#include "braids/drivers/pots_adc.h"

namespace braids {

// to start, make things reasonably compatible with codebase and encapsulate
// any additional processing here
struct Parameters {

  int32_t pitch; // already mixed pot & cv, but not quantized
  int32_t pitch_fine;
  int32_t fm; // already offset

  uint16_t parameters[2];
};

class CvScaler {
public:
  CvScaler() { }
  ~CvScaler() { }

  void Init(CalibrationData *calibration_data);

  void Read(Parameters *parameters);

  inline uint16_t cv_value(size_t channel) const {
    return cv_adc_.value(channel);
  }

  inline const uint16_t *cv_values() const {
    return cv_adc_.values();
  }

  inline uint16_t pot_value(size_t channel) const {
    return pot_adc_.value(channel);
  }

  inline const int32_t *smoothed_values() const {
    return smoothed_;
  }

  void CalibrateOffsets() {
    //std::copy(adc_.raw_values() + ADC_PITCH_POT, adc_.raw_values() + ADC_CHANNEL_LAST, calibration_data_->offset);
  }

  void Calibrate1V() {
    cv_c2_ = cv_adc_.value(CV_ADC_VOCT) >> 4;
  }

  bool Calibrate3V() {
    int32_t c2 = cv_c2_;
    int32_t c4 = cv_adc_.value(CV_ADC_VOCT) >> 4;

    if (c4 != c2) {
      int32_t scale = (24 * 128 * 4096L) / (c4 - c2);
      calibration_data_->pitch_cv_scale = scale;
      calibration_data_->pitch_cv_offset = (60<<7) -
          (scale * ((c2 + c4) >> 1) >> 12);
      return true;
    } else {
      return false;
    }
  }

  // Lock pots and set snap values so they can be used for other purposes
  // Multiple calls to LockPots without UnlockPots will probably cause mayhem
  void LockPots(uint16_t mask, int32_t *snap);

  // Release locked pots
  void UnlockPots();

  // Get pot value for locked pot, respecting snap value
  float ReadPot(Potentiometer pot) {
    const uint16_t mask = 0x1 << pot;
    if (locked_mask_ & mask) {
      if (snapped_mask_ & mask)
        return snapped_[pot] / 65536.f;
      else
        return smoothed_[pot] / 65536.f;
    } else {
      return 0.f;
    }
  }

private:

  CvAdc cv_adc_;
  PotsAdc pot_adc_;
  CalibrationData *calibration_data_;

  uint32_t scan_;
  int32_t cv_c2_;

  // WIP
  int32_t smoothed_[POT_LAST]; // smoothed pot values, 16 bit
  int32_t locked_[POT_LAST];
  int32_t snapped_[POT_LAST];

  uint16_t locked_mask_;
  uint16_t snapped_mask_;

  template <Potentiometer pot, int32_t offset>
  int32_t readPot() {
    const uint16_t channel_mask = 0x1 << pot;
    int32_t value;
    if (!(locked_mask_ & channel_mask)) {
      if (!(snapped_mask_ & channel_mask))
        value = smoothed_[pot];
      else
        value = snapped_[pot];
    } else {
      value = locked_[pot];
    }
    return value + offset;
  }

  template <CvAdcChannel channel>
  int32_t readCvUni() {
    return 65535 - cv_adc_.value(channel);
  }

  template <CvAdcChannel channel>
  int32_t readCvBi() {
    return 32767 - cv_adc_.value(channel);
  }

  template <Potentiometer pot, int32_t samples>
  int32_t pot_smooth() {
    const int32_t value = pot_adc_.value(pot);
    const int32_t smoothed = (smoothed_[pot] * (samples - 1) + value) / samples;
    smoothed_[pot] = smoothed;

    const uint16_t mask = 0x1 << pot;
    if (snapped_mask_ & mask) {
      int32_t delta = snapped_[pot] - smoothed;
      if (delta < 0)
        delta = -delta;
      if (delta <= 0x40)
        snapped_mask_ &= ~mask;
    }

    return smoothed;
  }

  int32_t adc_to_pitch(int32_t pitch_cv, int32_t pitch_coarse);

  DISALLOW_COPY_AND_ASSIGN(CvScaler);
};

} // namespace braids

#endif // BRAIDS_CV_SCALER_H_
