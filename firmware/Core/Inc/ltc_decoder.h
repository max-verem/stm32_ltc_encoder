#ifndef LTC_DECODER_H
#define LTC_DECODER_H

#include "stm32f4xx_hal.h"

extern volatile unsigned int ltc_last;

typedef void (*ltc_decoder_cb)(uint32_t tc_bcd, uint8_t *tc_str_data, uint32_t tc_str_len);

void ltc_decoder_idle();
void ltc_decoder_init(TIM_HandleTypeDef* tim);

#endif
