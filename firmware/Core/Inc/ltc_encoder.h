#ifndef LTC_ENCODER_H
#define LTC_ENCODER_H

#include "stm32f4xx_hal.h"

void ltc_decoder_init(TIM_HandleTypeDef* tim, GPIO_TypeDef *gpio, uint32_t pin);
void ltc_decoder_idle();
extern volatile uint32_t ltc_bcd_update, ltc_bcd_curr;
uint32_t tc_bcd_normalize(uint32_t src);

#endif /* LTC_ENCODER_H */
