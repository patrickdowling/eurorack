#include <algorithm>
#include "stmlib/dsp/dsp.h"
#include "braids/cv_scaler.h"
#include "braids/settings.h"

namespace braids {

void CvScaler::Init() {
  adc_.Init();

  std::fill(smoothed_, smoothed_ + ADC_CHANNEL_LAST, 0);
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

  parameters->pitch_cv = readCvUni<ADC_VOCT_CV, 1>() >> 4;
  parameters->pitch_coarse = readPot<ADC_PITCH_POT, 4, 0>() >> 4;

  // expects uint32_t, No shift required
  parameters->pitch_fine = readPot<ADC_FINE_POT, 64, -32768>();

  int32_t fm = attenuvert(readCvBi<ADC_FM_CV, 1>(),
                          readPot<ADC_FM_POT, 8, -32768>());
  parameters->fm = (32768 + stmlib::Clip16(fm)) >> 4;

  int32_t value;

  // cv input is 10v (-5,5), so scale x 2 to allow full range change for (-5,0) and (0,5)

  value = readPot<ADC_PARAM1_POT, 4, 0>()
      + attenuvert(2 * readCvBi<ADC_PARAM1_CV, 1>(),
                   readPot<ADC_MOD_POT, 4, -32768>());
  CONSTRAIN(value, 0, 65535)
  parameters->param1 = value >> 4;

  value = readPot<ADC_PARAM2_POT, 4, 0>() + 2 * readCvBi<ADC_PARAM2_CV, 1>();
  CONSTRAIN(value, 0, 65535)
  parameters->param2 = value >> 4;

  // trigger next ADC scan
  adc_.Convert();
}

} // namespace braids
