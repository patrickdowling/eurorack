#include "braids/cv_scaler.h"

namespace braids {

void CvScaler::Init() {
  adc_.Init();
}

void CvScaler::Read(Parameters *parameters) {
/*
  parameters->values[VALUE_PARAM1] = adc_.raw_value(ADC_PARAM1_POT);
  parameters->values[VALUE_PARAM2] = adc_.raw_value(ADC_PARAM2_POT);
  parameters->values[VALUE_PITCH]  = adc_.raw_value(ADC_PITCH_POT);
  parameters->values[VALUE_FM] = adc_.raw_value(ADC_FM_POT);
*/
  parameters->values[VALUE_PARAM1] = 0;
  parameters->values[VALUE_PARAM2] = 0;
  parameters->values[VALUE_PITCH]  = 0;
  parameters->values[VALUE_FM] = 0;

//  pitch += internal_adc.value(0) >> 8;

  // trigger next ADC scan
  adc_.Convert();
}

} // namespace braids
