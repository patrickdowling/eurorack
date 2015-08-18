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
#include "braids/drivers/internal_adc.h"
#include "braids/settings.h"

namespace braids {

// to start, make things reasonably compatible with codebase
struct Parameters {

  uint16_t pitch;
  uint16_t fm;

  uint16_t param1;
  uint16_t param2;  

  int32_t fine;
};

class CvScaler {
public:
  CvScaler() { }
  ~CvScaler() { }

  void Init(Settings *settings);

  void Read(Parameters *parameters);

  inline uint16_t adc_value(size_t channel) const {
    return adc_.raw_value(channel);
  }

  inline const uint16_t *raw_values() const {
    return adc_.raw_values();
  }

private:

  InternalAdc adc_;
  Settings *settings_;

  // WIP
  int32_t state_[ADC_CHANNEL_LAST];

  DISALLOW_COPY_AND_ASSIGN(CvScaler);
};

} // namespace braids

#endif // BRAIDS_CV_SCALER_H_
