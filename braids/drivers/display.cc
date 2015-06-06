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
// Driver for 4x14-segments display.

#include "braids/drivers/display.h"

#include <string.h>

#include "braids/resources.h"

namespace braids {

#ifndef OLIMEX_STM32
#define GPIO_DISP_SER GPIOB
const uint16_t kPinClk = GPIO_Pin_11;
const uint16_t kPinData = GPIO_Pin_1;
#define GPIO_DISP_CTRL GPIOB
const uint16_t kPinEnable = GPIO_Pin_10;
#else
// To try and support SPI3 on H405, use GPIOC:10 for clk, GPIOC:12 for data
#define GPIO_DISP_SER GPIOC
const uint16_t kPinClk = GPIO_Pin_10;
const uint16_t kPinData = GPIO_Pin_12;
#define GPIO_DISP_CTRL GPIOB
const uint16_t kPinEnable = GPIO_Pin_1;
#endif

const uint16_t kCharacterEnablePins[] = {
  GPIO_Pin_5,
  GPIO_Pin_6,
  GPIO_Pin_7,
  GPIO_Pin_8
};

void Display::Init() {
  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Pin = kPinEnable;
  gpio_init.GPIO_Pin |= kCharacterEnablePins[0];
  gpio_init.GPIO_Pin |= kCharacterEnablePins[1];
  gpio_init.GPIO_Pin |= kCharacterEnablePins[2];
  gpio_init.GPIO_Pin |= kCharacterEnablePins[3];
  
  gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
  gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIO_DISP_CTRL, &gpio_init);

  gpio_init.GPIO_Pin = kPinClk | kPinData;
  gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
  gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(GPIO_DISP_SER, &gpio_init);

  GPIO_DISP_CTRL->BSRR = kPinEnable;
  active_position_ = 0;
  brightness_pwm_cycle_ = 0;
  brightness_ = 3;
  memset(buffer_, ' ', kDisplayWidth);
}

void Display::Refresh() {
  if (brightness_pwm_cycle_ <= brightness_) {
    GPIO_DISP_CTRL->BRR = kCharacterEnablePins[active_position_];
    active_position_ = (active_position_ + 1) % kDisplayWidth;
    Shift14SegmentsWord(chr_characters[
        static_cast<uint8_t>(buffer_[active_position_])]);
    GPIO_DISP_CTRL->BSRR = kCharacterEnablePins[active_position_];
  } else {
    GPIO_DISP_CTRL->BRR = kCharacterEnablePins[active_position_];
  }
  brightness_pwm_cycle_ = (brightness_pwm_cycle_ + 1) % kBrightnessLevels;
}

void Display::Print(const char* s) {
  strncpy(buffer_, s, kDisplayWidth);
}

void Display::Shift14SegmentsWord(uint16_t data) {
  GPIO_DISP_CTRL->BRR = kPinEnable;
  for (uint16_t i = 0; i < 16; ++i) {
    GPIO_DISP_SER->BRR = kPinClk;
    if (data & 1) {
      GPIO_DISP_SER->BSRR = kPinData;
    } else {
      GPIO_DISP_SER->BRR = kPinData;
    }
    data >>= 1;
    GPIO_DISP_SER->BSRR = kPinClk;
  }
  GPIO_DISP_CTRL->BSRR = kPinEnable;
}

}  // namespace braids
