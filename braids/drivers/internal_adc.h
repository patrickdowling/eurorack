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

#ifndef BRAIDS_DRIVERS_INTERNAL_ADC_H_
#define BRAIDS_DRIVERS_INTERNAL_ADC_H_

#include "stmlib/stmlib.h"

namespace braids {

enum AdcChannel {
  ADC_VOCT_CV,
  ADC_FM_CV,
  ADC_PARAM1_CV,
  ADC_PARAM2_CV,
  ADC_PITCH_POT,
  ADC_FINE_POT,
  ADC_FM_POT,
  ADC_PARAM1_POT,
  ADC_MOD_POT,
  ADC_PARAM2_POT,
  ADC_CHANNEL_LAST,
};

class InternalAdc {
 public:
  InternalAdc() { }
  ~InternalAdc() { }
  
  void Init();
  void DeInit();
  void Convert();
  
  inline int32_t value(int channel) {
    int32_t v = (static_cast<int32_t>(values_[channel]) - 32768) << 8;
    int32_t delta = v - state_[channel];
    state_[channel] += (delta >> 8);
    return state_[channel] >> 8;
  }
  
 private:

  int32_t state_[ADC_CHANNEL_LAST];
  uint16_t values_[ADC_CHANNEL_LAST];
  
  DISALLOW_COPY_AND_ASSIGN(InternalAdc);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_INTERNAL_ADC_H_
