#include "ltc_decoder.h"

/*
 * PAL TV Framerate is 25 fps.
 * Frame duration is 40ms (40000us)
 * Each bit is 500us
 * bit 0 is  1x500us
 * bit 1 is  2x250us
 *
 */

#define BIT_0_DUR 500
#define BIT_1_DUR 250
#define BIT_THRESHOLD 100

#define BIT_NONE 0
#define BIT_ZERO 1
#define BIT_ONE  2

//#define DURS_BUF 256

#ifdef DURS_BUF
volatile unsigned int durs_idx = 0;
volatile unsigned int durs_buf[DURS_BUF];
#endif

volatile unsigned int ltc_last = 0;

static volatile unsigned int ltc_raw[2], ltc_found = 0;

static volatile unsigned int bits_buffer[3], bits_count = 0;

#define SYNC_WORD 0xBFFC // 1011 1111 1111 1100

static void probe_ltc()
{
	if(bits_buffer[2] == SYNC_WORD)
	{
		ltc_found = bits_count;
		ltc_raw[0] = bits_buffer[0];
		ltc_raw[1] = bits_buffer[1];
		bits_count = 0;
	}
};

static void push_bit(int b)
{
	if(b == BIT_NONE)
		bits_count = 0;
	else
	{
		// shift
		bits_buffer[0] >>= 1;
		if(bits_buffer[1] & 1) bits_buffer[0] |= 0x80000000;
		bits_buffer[1] >>= 1;
		if(bits_buffer[2] & 1) bits_buffer[1] |= 0x80000000;
		bits_buffer[2] >>= 1;
		if(b == BIT_ONE)
			bits_buffer[2] |= (1 << 15);

		bits_count++;

		if(bits_count >= 80)
			probe_ltc();
	}
};

volatile unsigned int freq_cnt = 0;
volatile unsigned int bit_prev = BIT_NONE;

static void push_tick(unsigned int cnt)
{
	unsigned int dur;

	if(!freq_cnt)
	  freq_cnt = HAL_RCC_GetHCLKFreq() / 1000000;

	dur = cnt / freq_cnt;

	if((BIT_0_DUR - BIT_THRESHOLD) < dur && dur < (BIT_0_DUR + BIT_THRESHOLD))
	{
		bit_prev = BIT_NONE;
		push_bit(BIT_ZERO);
	}
	else if((BIT_1_DUR - BIT_THRESHOLD) < dur && dur < (BIT_1_DUR + BIT_THRESHOLD))
	{
		if(bit_prev)
		{
			bit_prev = BIT_NONE;
			push_bit(BIT_ONE);
		}
		else
			bit_prev = BIT_ONE;
	}
	else
	{
#ifdef DURS_BUF
		durs_buf[durs_idx] = dur; durs_idx = (durs_idx + 1) % DURS_BUF;
#endif
		bit_prev = BIT_NONE;
		push_bit(BIT_NONE);
	}
};

//  https://controllerstech.com/pwm-input-in-stm32/
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
	{
		uint32_t per, high, low;

		// Read the IC value
		per = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
		high = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
		if(per)
		{
			low = per - high;

			push_tick(high);
			push_tick(low);
		}
	}
};

static unsigned char str[13] = "--:--:--:--\r\n";
static const unsigned char str_map[16] = "0123456789ABCDEF";


void ltc_decoder_init(TIM_HandleTypeDef* tim)
{
  // start PWM counters
  HAL_TIM_IC_Start_IT(tim, TIM_CHANNEL_1);
  HAL_TIM_IC_Start(tim, TIM_CHANNEL_2);
}

void ltc_decoder_idle(ltc_decoder_cb cb)
{
	if(ltc_found == 80)
	{
		uint32_t tc = 0, tc_bcd;

		ltc_found = 0;

		tc |= (ltc_raw[0] & 0x0000000F) >> 0;
		tc |= (ltc_raw[0] & 0x00000300) >> 4;
		tc |= (ltc_raw[0] & 0x000F0000) >> 8;
		tc |= (ltc_raw[0] & 0x07000000) >> 12;

		tc |= (ltc_raw[1] & 0x0000000F) << 16;
		tc |= (ltc_raw[1] & 0x00000700) << 12;
		tc |= (ltc_raw[1] & 0x000F0000) << 8;
		tc |= (ltc_raw[1] & 0x03000000) << 4;

		tc_bcd = tc;

		str[10] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 9] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 7] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 6] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 4] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 3] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 1] = str_map[ tc & 0x0F ]; tc >>= 4;
		str[ 0] = str_map[ tc & 0x0F ]; tc >>= 4;

		ltc_last = HAL_GetTick();

		cb(tc_bcd, str, sizeof(str));
	}
}
