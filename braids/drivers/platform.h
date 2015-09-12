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

#ifndef BRAIDS_DRIVERS_PLATFORM_H_
#define BRAIDS_DRIVERS_PLATFORM_H_

// TODO
// stm lib has the function GPIO_SetBits but original Braids source used direct
// register toggling. This their ugly child.

#ifdef STM32F4XX
#include <stm32f4xx_conf.h>

#define GPIO_SET(gpio,pins) \
do { gpio->BSRRL = pins; } while(0)

#define GPIO_RESET(gpio,pins) \
do { gpio->BSRRH = pins; } while(0)

#define PLATFORM_TIM1_UP_IRQn TIM1_UP_TIM10_IRQn
#define PLATFORM_TIM1_UP_IRQHandler TIM1_UP_TIM10_IRQHandler

#else
#include <stm32f10x_conf.h>

#define GPIO_SET(gpio,pins) \
do { gpio->BSRR = pins; } while(0)

#define GPIO_RESET(gpio,pins) \
do { gpio->BRR = pins; } while(0)

#define PLATFORM_TIM1_UP_IRQn TIM1_UP_IRQn
#define PLATFORM_TIM1_UP_IRQHandler TIM1_UP_IRQHandler

#endif

typedef uint16_t gpio_pin_t;

namespace braids {

struct AdcChannelDesc {
  GPIO_TypeDef *gpio;
  gpio_pin_t pin;
  uint8_t channel;
  uint8_t sample_time;
};

static const uint32_t ADC_Prescaler = ADC_Prescaler_Div8;
static const uint32_t ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;

} // namespace braids

#endif // BRAIDS_DRIVERS_PLATFORM_H_
