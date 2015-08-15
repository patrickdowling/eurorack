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

#include "braids/drivers/internal_adc.h"
#include "braids/drivers//platform.h"

// TODO Single channel mode for bootloader
// TODO Use ADC1 + ADC2 in DUAL mode, which (w|c)ould allow all channels to
// use ADC_SampleTime_480Cycles

namespace braids {

struct AdcChannelDesc {
  GPIO_TypeDef *gpio;
  gpio_pin_t pin;
  uint8_t channel;
  uint8_t sample_time;
};

static const AdcChannelDesc AdcCvChannels[] = {
  { GPIOA, GPIO_Pin_3, ADC_Channel_3, ADC_SampleTime_480Cycles }, // PA3, ADC_VOCT_CV
  { GPIOA, GPIO_Pin_1, ADC_Channel_1, ADC_SampleTime_480Cycles }, // PA1, ADC_FM_CV
  { GPIOA, GPIO_Pin_2, ADC_Channel_2, ADC_SampleTime_480Cycles }, // PA2, ADC_PARAM1_CV
  { GPIOB, GPIO_Pin_1, ADC_Channel_12, ADC_SampleTime_480Cycles }, // PB1, ADC_PARAM2_CV
};

static const AdcChannelDesc AdcPotChannels[] = {
  { GPIOA, GPIO_Pin_4, ADC_Channel_4, ADC_SampleTime_144Cycles }, // PA4, ADC_PITCH_POT
  { GPIOA, GPIO_Pin_5, ADC_Channel_5, ADC_SampleTime_144Cycles }, // PA5, ADC_FINE_POT
  { GPIOC, GPIO_Pin_1, ADC_Channel_11, ADC_SampleTime_144Cycles }, // PC1, ADC_FM_POT
  { GPIOC, GPIO_Pin_3, ADC_Channel_13, ADC_SampleTime_144Cycles }, // PC3, ADC_PARAM1_POT
  { GPIOC, GPIO_Pin_0, ADC_Channel_10, ADC_SampleTime_144Cycles }, // PC0, ADC_MOD_POT
  { GPIOA, GPIO_Pin_7, ADC_Channel_7, ADC_SampleTime_144Cycles }, // PA7, ADC_PARAM2_POT
};
  
void InternalAdc::Init() {

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOC, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

  GPIO_InitTypeDef gpio_init;
  GPIO_StructInit(&gpio_init);
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_init.GPIO_Mode = GPIO_Mode_AN;

  for (int i = 0; i < 4; ++i) {
    gpio_init.GPIO_Pin = AdcCvChannels[i].pin;
    GPIO_Init(AdcCvChannels[i].gpio, &gpio_init);
  }

  for (int i = 0; i < 6; ++i) {
    gpio_init.GPIO_Pin = AdcPotChannels[i].pin;
    GPIO_Init(AdcPotChannels[i].gpio, &gpio_init);
  }

  DMA_InitTypeDef dma_init;
  ADC_CommonInitTypeDef adc_common_init;
  ADC_InitTypeDef adc_init;

  // Use DMA to automatically copy ADC data register to values_ buffer.
  dma_init.DMA_Channel = DMA_Channel_0;
  dma_init.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
  dma_init.DMA_Memory0BaseAddr = (uint32_t)&values_[0];
  dma_init.DMA_DIR = DMA_DIR_PeripheralToMemory;
  dma_init.DMA_BufferSize = ADC_CHANNEL_LAST;
  dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
  dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable; 
  dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
  dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
  dma_init.DMA_Mode = DMA_Mode_Circular;
  dma_init.DMA_Priority = DMA_Priority_High;
  dma_init.DMA_FIFOMode = DMA_FIFOMode_Disable;
  dma_init.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
  dma_init.DMA_MemoryBurst = DMA_MemoryBurst_Single;
  dma_init.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
  DMA_Init(DMA2_Stream0, &dma_init);
  DMA_Cmd(DMA2_Stream0, ENABLE);
  
  adc_common_init.ADC_Mode = ADC_Mode_Independent;
  adc_common_init.ADC_Prescaler = ADC_Prescaler_Div6;
  adc_common_init.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
  adc_common_init.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;
  ADC_CommonInit(&adc_common_init);
  
  adc_init.ADC_Resolution = ADC_Resolution_12b;
  adc_init.ADC_ScanConvMode = ENABLE;
  adc_init.ADC_ContinuousConvMode = DISABLE;
  adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
  adc_init.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  adc_init.ADC_DataAlign = ADC_DataAlign_Left;
  adc_init.ADC_NbrOfConversion = ADC_CHANNEL_LAST;
  ADC_Init(ADC1, &adc_init);

  // Using ADC_Prescaler_Div6
  // 168M / 2 / 6 / (4x(480+20) + 6x(144+20)) = 4.69KHz
 
  for (int i = 0; i < 4; ++i)
    ADC_RegularChannelConfig(ADC1, AdcCvChannels[i].channel, i+1, AdcCvChannels[i].sample_time);

  for (int i = 0; i < 6; ++i)
    ADC_RegularChannelConfig(ADC1, AdcPotChannels[i].channel, i+1, AdcPotChannels[i].sample_time);

  ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
  ADC_Cmd(ADC1, ENABLE);
  ADC_DMACmd(ADC1, ENABLE);
  Convert();
}

void InternalAdc::DeInit() {
  ADC_DeInit();
}

void InternalAdc::Convert() {
  ADC_SoftwareStartConv(ADC1);
}

}  // namespace braids
