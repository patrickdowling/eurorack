#include "braids/drivers/switches.h"
#include <algorithm>

namespace braids {

static const gpio_pin_t pins[] = { GPIO_Pin_11, GPIO_Pin_5 };
static GPIO_TypeDef* const gpios[] = { GPIOB, GPIOC };

void Switches::Init() {

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

  GPIO_InitTypeDef gpio_init;
  GPIO_StructInit(&gpio_init);
  gpio_init.GPIO_Mode = GPIO_Mode_IN;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_25MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_UP;

  for (size_t i = 0; i < kNumSwitches; ++i ) {
    gpio_init.GPIO_Pin = pins[i];
    GPIO_Init(gpios[i], &gpio_init);
  }

  std::fill(switch_state_, switch_state_ + kNumSwitches, 0xff);
}

void Switches::Debounce() {

  for (size_t i = 0; i < kNumSwitches; ++i )
    switch_state_[i] = (switch_state_[i] << 1) | GPIO_ReadInputDataBit(gpios[i], pins[i]);
}

}; // namespace braids
