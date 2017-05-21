// Copyright 2013 Olivier Gillet.
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
//
// -----------------------------------------------------------------------------
//
// Driver for ADC.
// Originally internal_adc.h

#ifndef BRAIDS_DRIVERS_CV_ADC_H_
#define BRAIDS_DRIVERS_CV_ADC_H_

#include "stmlib/stmlib.h"

namespace braids {

enum CvAdcChannel {
  CV_ADC_VOCT,
  CV_ADC_FM,
  CV_ADC_PARAM1,
  CV_ADC_PARAM2,
  CV_ADC_CHANNEL_LAST,
};

enum {
  CV_ADC_OVERSAMPLE = 4
};

class CvAdc {
 public:
  CvAdc() { }
  ~CvAdc() { }
  
  void Init();
  void DeInit();
  void Convert();

  inline float float_value(size_t channel) const {
    return static_cast<float>(values_[channel]) / 65536.0f;
  }

  void Sample() {
    for (size_t channel = 0; channel < CV_ADC_CHANNEL_LAST; ++channel) {
      uint32_t value = raw_values_[channel];
      value += raw_values_[channel + (1 * CV_ADC_CHANNEL_LAST)];
      value += raw_values_[channel + (2 * CV_ADC_CHANNEL_LAST)];
      value += raw_values_[channel + (3 * CV_ADC_CHANNEL_LAST)];
      values_[channel] = value / CV_ADC_OVERSAMPLE;
    }
  }

  inline int32_t value(size_t channel) const {
    return values_[channel];
  }

  inline const uint16_t* values() const {
    return &values_[0];
  }

 private:

  uint16_t raw_values_[CV_ADC_CHANNEL_LAST * CV_ADC_OVERSAMPLE];
  uint16_t values_[CV_ADC_CHANNEL_LAST];
  
  DISALLOW_COPY_AND_ASSIGN(CvAdc);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_CV_ADC_H_
