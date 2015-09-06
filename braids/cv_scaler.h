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
#include "braids/drivers/internal_adc.h"

namespace braids {

// to start, make things reasonably compatible with codebase and encapsulate
// any additional processing here
struct Parameters {

  int32_t pitch; // already mixed pot & cv, but not quantized
  int32_t pitch_fine;
  uint16_t fm;

  uint16_t param1;
  uint16_t param2;  

};

class CvScaler {
public:
  CvScaler() { }
  ~CvScaler() { }

  void Init(CalibrationData *calibration_data);

  void Read(Parameters *parameters);

  inline uint16_t adc_value(size_t channel) const {
    return adc_.raw_value(channel);
  }

  inline const uint16_t *raw_values() const {
    return adc_.raw_values();
  }

  inline const int32_t *smoothed_values() const {
    return smoothed_;
  }

  void CalibrateOffsets() {
    std::copy(adc_.raw_values() + ADC_PITCH_POT, adc_.raw_values() + ADC_CHANNEL_LAST, calibration_data_->offset);
  }

  void Calibrate1V() {
    cv_c2_ = adc_.raw_values()[ADC_VOCT_CV] >> 4;
  }

  bool Calibrate3V() {
    int32_t c2 = cv_c2_;
    int32_t c4 = adc_.raw_values()[ADC_VOCT_CV] >> 4;

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

  void LockChannels(uint16_t mask);
  void UnlockChannels();

private:

  InternalAdc adc_;
  CalibrationData *calibration_data_;

  int32_t cv_c2_;

  // WIP
  int32_t smoothed_[ADC_CHANNEL_LAST]; // smoothed values, 16 bit
  int32_t locked_[ADC_CHANNEL_LAST];
  uint16_t locked_mask_;
  uint16_t snapped_mask_;

  template <AdcChannel channel, int32_t offset>
  int32_t readPot() {
    return read<channel>() + offset;
  }

  template <AdcChannel channel>
  int32_t readCvUni() {
    return 65535 - smoothed_[channel];
  }

  template <AdcChannel channel>
  int32_t readCvBi() {
    return 32767 - smoothed_[channel];
  }

  template <AdcChannel channel, int32_t samples>
  int32_t adc_read() {
    int32_t value = adc_.raw_value(channel);
    const int32_t smoothed = (smoothed_[channel] * (samples - 1) + value) / samples;
    smoothed_[channel] = smoothed;
    return smoothed;
  }

  template <AdcChannel channel>
  int32_t read() {
    const uint16_t channel_mask = 0x1 << channel;
    if (locked_mask_ & channel_mask) {
      return locked_[channel];
    } else {
      if (!(snapped_mask_ & channel_mask)) {
        return smoothed_[channel];
      } else {
        int32_t delta = locked_[channel] - smoothed_[channel];
        if ( delta < 0 )
          delta = -delta;
        if ( delta <= 2 ) {
          snapped_mask_ &= ~channel_mask;
          return smoothed_[channel];
        } else {
          return locked_[channel];
        }
      }
    }
  }

  int32_t adc_to_pitch(int32_t pitch_cv, int32_t pitch_coarse);

  DISALLOW_COPY_AND_ASSIGN(CvScaler);
};

} // namespace braids

#endif // BRAIDS_CV_SCALER_H_
