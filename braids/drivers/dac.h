// Copyright 2012 Olivier Gillet.
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
// Driver for DAC.

#ifndef BRAIDS_DRIVERS_DAC_H_
#define BRAIDS_DRIVERS_DAC_H_

#include "platform.h"
#include "stmlib/stmlib.h"

#define DAC_SPIX SPI3
#define DAC_RCC_SPIX RCC_APB1Periph_SPI3
#define DAC_AF_SPIX GPIO_AF_SPI3

#define DAC_USE_DMA_DISABLED

#ifdef DAC_USE_DMA
// channel:stream see stm32f4xx reference 10.3.3
#define DAC_DMA_CLOCK RCC_AHB1Periph_DMA1
#define DAC_DMA_STREAM  DMA1_Stream4
#define DAC_DMA_CHANNEL DMA_Channel_0
#endif

namespace braids {

class Dac {
 public:
  Dac() { }
  ~Dac() { }
  
  void Init();

#ifdef DAC_USE_DMA
  inline void Write(uint16_t value) {
    GPIO_SET(GPIOA, kPinSS);
    DMA_Cmd(DAC_DMA_STREAM, DISABLE);
    GPIO_RESET(GPIOA, kPinSS);

    dma_buffer_[0] = value >> 8;
    dma_buffer_[1] = value << 8;

    DMA_SetCurrDataCounter(DAC_DMA_STREAM, 2);
    DMA_Cmd(DAC_DMA_STREAM, ENABLE);
  }
#else
  inline void Write(uint16_t value) {
    GPIO_SET(GPIOA, kPinSS);
    __asm__("nop");
    GPIO_RESET(GPIOA, kPinSS);
    __asm__("nop");
    DAC_SPIX->DR = value >> 8;
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    __asm__("nop");
    DAC_SPIX->DR = value << 8;
  }
 #endif
 
 private:

  static const gpio_pin_t kPinSS = GPIO_Pin_6; // PA6

#ifdef DAC_USE_DMA
  DMA_InitTypeDef dma_init_tx_;
  uint16_t dma_buffer_[2];
#endif
  
  DISALLOW_COPY_AND_ASSIGN(Dac);
};

}  // namespace braids

#endif  // BRAIDS_DRIVERS_DAC_H_
