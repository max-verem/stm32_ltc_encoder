#ifndef LTC_ENCODER_H
#define LTC_ENCODER_H

#include "stm32f4xx_hal.h"

void ltc_decoder_init(I2S_HandleTypeDef* i2s);
extern uint32_t ltc_bcd_update, ltc_bcd_curr;

#endif /* LTC_ENCODER_H */
