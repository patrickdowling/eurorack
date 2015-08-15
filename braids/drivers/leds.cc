#include "braids/drivers/leds.h"

namespace braids {

void Leds::Init() {

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

  GPIO_InitTypeDef gpio_init;
  gpio_init.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_10;
  gpio_init.GPIO_Mode = GPIO_Mode_OUT;
  gpio_init.GPIO_OType = GPIO_OType_PP;
  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
  gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &gpio_init);

  // TODO
  // PB5 -> TIM3_CH2
  // PB10 -> TIM2_CH3

  gate_ = 0;
  status_ = 0;
}

void Leds::Write() {

	GPIO_WriteBit(GPIOB, GPIO_Pin_5, status_ ? Bit_SET : Bit_RESET );
	GPIO_WriteBit(GPIOB, GPIO_Pin_10, gate_ ? Bit_SET : Bit_RESET );
}

} // namespace braids
