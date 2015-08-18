#include <algorithm>
#include "braids/cv_scaler.h"

namespace braids {

void CvScaler::Init(Settings *settings) {
  adc_.Init();
  settings_ = settings;

  std::fill(state_, state_ + ADC_CHANNEL_LAST, 0);
}

void CvScaler::Read(Parameters *parameters) {

  parameters->pitch = adc_.raw_value(ADC_PITCH_POT);
  parameters->fm = 0;

  parameters->param1 = adc_.raw_value(ADC_PARAM1_POT);
  parameters->param2 = adc_.raw_value(ADC_PARAM2_POT);

  // Averaging code from internal_adc.h
  int32_t v = (static_cast<int32_t>(adc_.raw_value(ADC_FINE_POT)) - 32768) << 8;
  int32_t delta = v - state_[ADC_FINE_POT];
  state_[ADC_FINE_POT] += (delta >> 8);

  parameters->fine = state_[ADC_FINE_POT] >> 8;

  // trigger next ADC scan
  adc_.Convert();
}

} // namespace braids
