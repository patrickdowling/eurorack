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

// Encapsulate pot smoothing and allow a pot to be used for multiple purposes
// by providing a locking mechanism. When locked, the pot value is stored and
// can be retrieved separately from the changing value.
// To avoid jumps when switching between states pots are snapped to the set
// value, i.e. the readable value only changes once the pot has been moved
// passed the snap value (same as Shruti, Peaks).

class LockableSmoothedPot {
public:
  void Init() {
    smoothed_value_ = locked_value_ = snap_value_ = 0;
    locked_ = false;
    snapped_ = true;
  }

  template<int32_t samples>
  void Update(int32_t value) {
    const int32_t smoothed = (smoothed_value_ * (samples - 1) + value) / samples;
    if (!snapped_) {
      int32_t delta = snap_value_ - smoothed;
      if (delta < 0)
        delta = -delta;
      if (delta <= 0x40)
        snapped_ = true;
    }
    smoothed_value_ = smoothed;
  }

  void Lock(int32_t snap_value) {
    locked_value_ = snapped_ ? smoothed_value_ : snap_value_;
    snap_value_ = snap_value;
    snapped_ = false;
    locked_ = true;
  }

  void Unlock() {
    if (locked_) {
      snap_value_ = locked_value_;
      snapped_ = false;
      locked_ = false;
    }
  }

  // Get value (ignore lock, but respect snap)
  int32_t value() const {
    return snapped_ ? smoothed_value_ : snap_value_;
  }

  // Get value that might be locked (expected to be the "normal" path)
  int32_t lockable_value() const {
    if (!locked_)
      return snapped_ ? smoothed_value_ : snap_value_;
    else
      return locked_value_;
  }

private:
  int32_t smoothed_value_;
  int32_t locked_value_;
  int32_t snap_value_;
  bool locked_;
  bool snapped_;
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

  inline uint16_t pot_value(size_t channel) const {
    return pot_adc_.value(channel);
  }

  void CalibrateOffsets() {
    std::copy(pot_adc_.values(), pot_adc_.values() + POT_LAST, calibration_data_->offset);
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
  void LockPots(uint16_t mask, int32_t *snap_values);

  // Release locked pots
  void UnlockPots();

  // Get pot value for locked pot, respecting latch value
  float ReadLockedPot(Potentiometer pot) {
    return static_cast<float>(pots_[pot].value()) / 65536.f;
  }

private:

  CvAdc cv_adc_;
  PotsAdc pot_adc_;
  CalibrationData *calibration_data_;

  int32_t cv_c2_;

  LockableSmoothedPot pots_[POT_LAST];

  template <Potentiometer pot, int32_t offset>
  int32_t readPot() {
    return pots_[pot].lockable_value() + offset;
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
  void UpdatePot() {
    pots_[pot].Update<samples>(pot_adc_.value(pot));
  }

  int32_t adc_to_pitch(int32_t pitch_cv, int32_t pitch_coarse);

  DISALLOW_COPY_AND_ASSIGN(CvScaler);
};

} // namespace braids

#endif // BRAIDS_CV_SCALER_H_
