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

#include <algorithm>
#include <string.h>

#include "braids/resources.h"

namespace braids {

#define GPIO_DISP_SPI GPIOA
const gpio_pin_t kPinClk = GPIO_Pin_8;
const gpio_pin_t kPinData = GPIO_Pin_9;
const gpio_pin_t kPinEnable = GPIO_Pin_10;

#define GPIO_DISP_CHAR GPIOB
const gpio_pin_t kCharacterEnablePins[] = {
  GPIO_Pin_12,
  GPIO_Pin_14,
  GPIO_Pin_13,
  GPIO_Pin_15,
};

void Display::Init() {
  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Pin = kCharacterEnablePins[0];
  gpio_init.GPIO_Pin |= kCharacterEnablePins[1];
  gpio_init.GPIO_Pin |= kCharacterEnablePins[2];
  gpio_init.GPIO_Pin |= kCharacterEnablePins[3];
  
  gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIO_DISP_CHAR, &gpio_init);

  gpio_init.GPIO_Pin = kPinClk | kPinData;
  gpio_init.GPIO_Pin |= kPinEnable;
  gpio_init.GPIO_Speed = GPIO_Speed_25MHz; // \sa clouds/drivers/leds.cc
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIO_DISP_SPI, &gpio_init);

  GPIO_SET(GPIO_DISP_SPI, kPinEnable);
  active_position_ = 0;
  brightness_pwm_cycle_ = 0;
  brightness_ = 3;
  memset(buffer_, ' ', kDisplayWidth);
  clear_decimals();
}

void Display::Refresh() {
  if (brightness_pwm_cycle_ <= brightness_) {
    GPIO_RESET(GPIO_DISP_CHAR, kCharacterEnablePins[active_position_]);
    active_position_ = (active_position_ + 1) % kDisplayWidth;
    uint16_t chr = chr_characters[
        static_cast<uint8_t>(buffer_[active_position_])];
    if (decimal_[active_position_])
      chr |= 0x02;
    Shift14SegmentsWord(chr);
    GPIO_SET(GPIO_DISP_CHAR, kCharacterEnablePins[active_position_]);
  } else {
    GPIO_RESET(GPIO_DISP_CHAR, kCharacterEnablePins[active_position_]);
  }
  brightness_pwm_cycle_ = (brightness_pwm_cycle_ + 1) % kBrightnessLevels;
}

void Display::Print(const char* s) {
  strncpy(buffer_, s, kDisplayWidth);
}

#ifndef STM32F4XX
void Display::Shift14SegmentsWord(uint16_t data) {
  GPIO_RESET(GPIO_DISP_SPI, kPinEnable);
  for (uint16_t i = 0; i < 16; ++i) {
    GPIO_RESET(GPIO_DISP_SPI, kPinClk);
    if (data & 1) {
      GPIO_SET(GPIO_DISP_SPI, kPinData);
    } else {
      GPIO_RESET(GPIO_DISP_SPI, kPinData);
    }
    data >>= 1;
    GPIO_SET(GPIO_DISP_SPI, kPinClk);
  }
  GPIO_SET(GPIO_DISP_SPI, kPinEnable);
}
#else
// Looks like the fast register setting version is too fast, so there's
// garbage characters displays. The garbage is consistent but still...
// In anticipation that there might be a hardware SPI version this is a
// duplicate using the "slow" function call GPIO set/reset.
void Display::Shift14SegmentsWord(uint16_t data) {
  GPIO_WriteBit(GPIO_DISP_SPI, kPinEnable, Bit_RESET);
  for (uint16_t i = 0; i < 16; ++i) {
    GPIO_WriteBit(GPIO_DISP_SPI, kPinClk, Bit_RESET);
    if (data & 1) {
      GPIO_WriteBit(GPIO_DISP_SPI, kPinData, Bit_SET);
    } else {
      GPIO_WriteBit(GPIO_DISP_SPI, kPinData, Bit_RESET);
    }
    data >>= 1;
    GPIO_WriteBit(GPIO_DISP_SPI, kPinClk, Bit_SET);
  }
  GPIO_WriteBit(GPIO_DISP_SPI, kPinEnable, Bit_SET);
}
#endif

void Display::clear_decimals() {
  std::fill(decimal_, decimal_ + kDisplayWidth, false);
}

void Display::set_decimal_hex(uint16_t value) {
  for (size_t i = 0; i < kDisplayWidth; ++i)
    decimal_[kDisplayWidth - 1 - i] = value & (0x1 << i);
}

}  // namespace braids
