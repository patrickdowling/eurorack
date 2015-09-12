// Copyright 2015 Patrick Dowling
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
// Driver for pots ADC.

#include <algorithm>
#include "braids/drivers/pots_adc.h"
#include "braids/drivers//platform.h"

namespace braids {

static const AdcChannelDesc PotAdcChannels[POT_LAST] = {
  { GPIOA, GPIO_Pin_5, ADC_Channel_5, ADC_SampleTime_480Cycles }, // PA5, ADC_PITCH_POT
  { GPIOA, GPIO_Pin_4, ADC_Channel_4, ADC_SampleTime_480Cycles }, // PA4, ADC_FINE_POT
  { GPIOC, GPIO_Pin_1, ADC_Channel_11, ADC_SampleTime_480Cycles }, // PC1, ADC_FM_POT
  { GPIOC, GPIO_Pin_3, ADC_Channel_13, ADC_SampleTime_144Cycles }, // PC3, ADC_PARAM1_POT
  { GPIOC, GPIO_Pin_0, ADC_Channel_10, ADC_SampleTime_144Cycles }, // PC0, ADC_MOD_POT
  { GPIOA, GPIO_Pin_7, ADC_Channel_7, ADC_SampleTime_144Cycles }, // PA7, ADC_PARAM2_POT
};

void PotsAdc::Init() {

  std::fill(values_, values_ + POT_LAST, 0);

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC2, ENABLE);

  GPIO_InitTypeDef gpio_init;
  GPIO_StructInit(&gpio_init);
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  gpio_init.GPIO_Mode = GPIO_Mode_AN;

  for (int i = 0; i < POT_LAST; ++i) {
    gpio_init.GPIO_Pin = PotAdcChannels[i].pin;
    GPIO_Init(PotAdcChannels[i].gpio, &gpio_init);
  }

  DMA_InitTypeDef dma_init;
  ADC_CommonInitTypeDef adc_common_init;
  ADC_InitTypeDef adc_init;

  // Use DMA to automatically copy ADC data register to values_ buffer.
  dma_init.DMA_Channel = DMA_Channel_1;
  dma_init.DMA_PeripheralBaseAddr = (uint32_t)&ADC2->DR;
  dma_init.DMA_Memory0BaseAddr = (uint32_t)&values_[0];
  dma_init.DMA_DIR = DMA_DIR_PeripheralToMemory;
  dma_init.DMA_BufferSize = POT_LAST;
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
  DMA_Init(DMA2_Stream2, &dma_init);
  DMA_Cmd(DMA2_Stream2, ENABLE);
  
  adc_common_init.ADC_Mode = ADC_Mode_Independent;
  adc_common_init.ADC_Prescaler = ADC_Prescaler;
  adc_common_init.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
  adc_common_init.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay;
  ADC_CommonInit(&adc_common_init);
  
  adc_init.ADC_Resolution = ADC_Resolution_12b;
  adc_init.ADC_ScanConvMode = ENABLE;
  adc_init.ADC_ContinuousConvMode = DISABLE;
  adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
  adc_init.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
  adc_init.ADC_DataAlign = ADC_DataAlign_Left;
  adc_init.ADC_NbrOfConversion = POT_LAST;
  ADC_Init(ADC2, &adc_init);

  // 168M / 2 / 8 / (3 x (480+20) + 3 x (144+20)) = 5.27KHz
   int rank = 1;
  for (int i = 0; i < POT_LAST; ++i) {
    ADC_RegularChannelConfig(ADC2, PotAdcChannels[i].channel, rank++, PotAdcChannels[i].sample_time);
  }

  ADC_DMARequestAfterLastTransferCmd(ADC2, ENABLE);
  ADC_Cmd(ADC2, ENABLE);
  ADC_DMACmd(ADC2, ENABLE);
  Convert();
}

void PotsAdc::DeInit() {
  ADC_DeInit();
}

void PotsAdc::Convert() {
  ADC_SoftwareStartConv(ADC2);
}

}  // namespace braids
