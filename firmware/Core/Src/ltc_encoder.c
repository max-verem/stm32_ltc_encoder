#include "ltc_encoder.h"

#define TP1
#ifdef TP1
#include "main.h"
#endif

/*

 	 80 bits per 25 fps - 2000 bits per seconds
 	 each bit encoded by two bits - 4000 half bits per second
 	 timer should be configured for 4000Hz

 */

#define LTC_BITS_LEN			80
#define LTC_32BITS_PER_OUT_BIT 	2
#define LTC_32BITS_WORDS		(LTC_BITS_LEN * LTC_32BITS_PER_OUT_BIT)
#define SYNC_WORD 0xBFFC // 1011 1111 1111 1100

volatile static uint32_t ltc_pin;
volatile static uint32_t LTC_OUT_BIT_HIGH;
volatile static uint32_t LTC_OUT_BIT_LOW;
static uint32_t ltc_out_buffer_bcd[2];
static uint32_t ltc_out_buffer_data[2 * LTC_32BITS_WORDS];
static uint32_t ltc_raw_buffer_data[3] = {0x1050006, 0x9030005, SYNC_WORD};
volatile uint32_t ltc_bcd_update = 0xFFFFFFFF, ltc_bcd_curr = 0;

static void ltc_out_buffer_fill(uint32_t* bits_buffer, int half_idx)
{
	int i, w_idx = half_idx ? LTC_32BITS_WORDS : 0;

	/*
	 * always starts with a rising edge at the beginning of bit 0
	 */
	int toggle = 1;

	for(i = 0; i < LTC_BITS_LEN; w_idx += 2, i++)
	{
		int idx = i / 32, bit = i % 32;
		if(bits_buffer[idx] & (1 << bit))
		{
			// bit '1'
			ltc_out_buffer_data[w_idx + 0] = toggle ? LTC_OUT_BIT_HIGH : LTC_OUT_BIT_LOW;
			toggle ^= 1;
			ltc_out_buffer_data[w_idx + 1] = toggle ? LTC_OUT_BIT_HIGH : LTC_OUT_BIT_LOW;
			toggle ^= 1;
		}
		else
		{
			// bit '0'
			ltc_out_buffer_data[w_idx + 0] =
		    ltc_out_buffer_data[w_idx + 1] = toggle ? LTC_OUT_BIT_HIGH : LTC_OUT_BIT_LOW;
			toggle ^= 1;
		}
	};
};

static uint32_t ltc_raw_zero_bits_cnt(uint32_t* bits_buffer)
{
	int r, i;

	for(r = 0, i = 0; i < LTC_BITS_LEN; i++)
	{
		int idx = i / 32, bit = i % 32;

		if(!(bits_buffer[idx] & (1 << bit)))
			r++;
	};

	return r;
};

static void ltc_raw_toggle_bit_59(uint32_t* bits_buffer)
{
	int z = ltc_raw_zero_bits_cnt(bits_buffer);
	if(z & 1)
		bits_buffer[1] ^= 1 << (59 - 32);
};

uint32_t tc_bcd_normalize(uint32_t tc_bcd)
{
    int i;

    register uint32_t
        digit_mask = 0x0000000F,
        comp_mask = 0x000000FF,
        digit_max = 0x0000000A,
        carry = 0x00000010,
        tc_max = 0x23595924;

    for(i = 0; i < 8; i++)
    {
    	register uint32_t digit = tc_bcd & digit_mask;

        if(digit >= digit_max)
        {
            digit -= digit_max;
            tc_bcd &= ~digit_mask;
            tc_bcd |= digit;
            tc_bcd += carry;
        };

        if(i & 1)
        {
            if((tc_bcd & comp_mask) > (tc_max & comp_mask))
            {
                tc_bcd &= ~comp_mask;
                tc_bcd += carry;
            };

            comp_mask <<= 8;
        }

        digit_mask <<= 4;
        digit_max <<= 4;
        carry <<= 4;
    };

    return tc_bcd;
};


void tc_bcd_to_ltc_raw(uint32_t tc_bcd, uint32_t *ltc_raw)
{
/*

BCD TIMECODE:

  | H | h | M | m | S | s | F | f |
  +---+---+---+---+---+---+---+---+
32| 28| 24| 20| 16| 12|  8|  4|  0|


LTC WORD 0:

  | _ | S | _ | s | _ | F | _ | f |
  +---+---+---+---+---+---+---+---+
32| 28| 24| 20| 16| 12|  8|  4|  0|

f [ 0] -> [ 0] = 0
F [ 4] -> [ 8] = 4
s [ 8] -> [16] = 8
S [12] -> [24] = 12

*/
	ltc_raw[0] =
//                   HhMmSsFf
//                   0S0s0F0f
		((tc_bcd & 0x0000f000) << 12) |
		((tc_bcd & 0x00000f00) << 8) |
		((tc_bcd & 0x000000f0) << 4) |
		((tc_bcd & 0x0000000f) << 0);

/*
LTC WORD 1:

  | _ | H | _ | h | _ | M | _ | m |
  +---+---+---+---+---+---+---+---+
32| 28| 24| 20| 16| 12|  8|  4|  0|

m [16] -> [ 0] = -16
M [20] -> [ 8] = -12
h [24] -> [16] =  -8
H [28] -> [24] =  -4

 */


	ltc_raw[1] =
//                   HhMmSsFf
//                   0H0h0M0m
		((tc_bcd & 0xf0000000) >> 4) |
		((tc_bcd & 0x0f000000) >> 8) |
		((tc_bcd & 0x00f00000) >> 12) |
		((tc_bcd & 0x000f0000) >> 16);

	ltc_raw_toggle_bit_59(ltc_raw);
};

static void ltc_new_frame(int half_idx)
{
	/* check if we requested to update timecode */
	if(ltc_bcd_update != 0xFFFFFFFF)
	{
		ltc_bcd_curr = ltc_bcd_update;
		ltc_bcd_update = 0xFFFFFFFF;
	}

	ltc_out_buffer_bcd[half_idx] = ltc_bcd_curr;

	/* inc tc counter */
	ltc_bcd_curr++;

	/* mormalize it value*/
	ltc_bcd_curr = tc_bcd_normalize(ltc_bcd_curr);
};

static void dma_cb_half(DMA_HandleTypeDef *hdma)
{
	ltc_new_frame(0);
};

static void dma_cb_full(DMA_HandleTypeDef *hdma)
{
	ltc_new_frame(1);
};

void ltc_encoder_idle()
{
/*
	Release mode - 60uS
	Debug mode - 230uS
 */

	if(ltc_out_buffer_bcd[0] != 0xFFFFFFFF)
	{
#ifdef TP1
	HAL_GPIO_WritePin(TP1_GPIO_Port, TP1_Pin, GPIO_PIN_SET);
#endif
		/* build raw LTC data */
		tc_bcd_to_ltc_raw(ltc_out_buffer_bcd[0], ltc_raw_buffer_data);

		/* decompose to bits out buffer */
		ltc_out_buffer_fill(ltc_raw_buffer_data, 0);

		/* mark as done */
		ltc_out_buffer_bcd[0] = 0xFFFFFFFF;

#ifdef TP1
	HAL_GPIO_WritePin(TP1_GPIO_Port, TP1_Pin, GPIO_PIN_RESET);
#endif
	};

	if(ltc_out_buffer_bcd[1] != 0xFFFFFFFF)
	{
#ifdef TP1
	HAL_GPIO_WritePin(TP1_GPIO_Port, TP1_Pin, GPIO_PIN_SET);
#endif
		/* build raw LTC data */
		tc_bcd_to_ltc_raw(ltc_out_buffer_bcd[1], ltc_raw_buffer_data);

		/* decompose to bits out buffer */
		ltc_out_buffer_fill(ltc_raw_buffer_data, 1);

		/* mark as done */
		ltc_out_buffer_bcd[1] = 0xFFFFFFFF;

#ifdef TP1
	HAL_GPIO_WritePin(TP1_GPIO_Port, TP1_Pin, GPIO_PIN_RESET);
#endif
	};
}

void ltc_encoder_init(TIM_HandleTypeDef* tim, GPIO_TypeDef *gpio, uint32_t pin)
{
	HAL_DMA_RegisterCallback(tim->hdma[TIM_DMA_ID_UPDATE], HAL_DMA_XFER_CPLT_CB_ID, dma_cb_full);
	HAL_DMA_RegisterCallback(tim->hdma[TIM_DMA_ID_UPDATE], HAL_DMA_XFER_HALFCPLT_CB_ID, dma_cb_half);
//	HAL_DMA_RegisterCallback(tim->hdma[TIM_DMA_ID_UPDATE], HAL_DMA_XFER_ERROR_CB_ID, dma_cb_error);
//	HAL_DMA_RegisterCallback(tim->hdma[TIM_DMA_ID_UPDATE], HAL_DMA_XFER_ABORT_CB_ID, dma_cb_abort);

	ltc_pin = pin;
	LTC_OUT_BIT_HIGH = ltc_pin << 16;
	LTC_OUT_BIT_LOW = ltc_pin << 0;
	ltc_new_frame(0);
	ltc_new_frame(1);

	HAL_DMA_Start_IT(
		tim->hdma[TIM_DMA_ID_UPDATE],
		(uint32_t)ltc_out_buffer_data,
		(uint32_t)&(gpio->BSRR), 2 * LTC_32BITS_WORDS);
	__HAL_TIM_ENABLE_DMA(tim, TIM_DMA_UPDATE ); //Enable the TIM Update DMA request
	__HAL_TIM_ENABLE(tim);                 //Enable the Peripheral
};

#if 0
  // bit 59 toggle test
  {
	  static uint32_t raw_ltc[3] = {0x1050006, 0x9030005, SYNC_WORD};
	  int z0, z1, z2, b59m = (1 << (59 - 32)), b59v0, b59v1, b59v2;

	  // initial state - calc zero bits
	  z0 = ltc_raw_zero_bits_cnt(raw_ltc);
	  b59v0 = (raw_ltc[1] & b59m) ? 1 : 0;

	  // corrupt
	  raw_ltc[1] ^= b59m;

	  // corrupted state - calc zero bits
	  z1 = ltc_raw_zero_bits_cnt(raw_ltc);
	  b59v1 = (raw_ltc[1] & b59m) ? 1 : 0;

	  // fix
	  ltc_raw_toggle_bit_59(raw_ltc);

	  // fixed state - calc zero bits
	  z2 = ltc_raw_zero_bits_cnt(raw_ltc);
	  b59v2 = (raw_ltc[1] & b59m) ? 1 : 0;

	  __BKPT();
  }
#endif

#if 0
  {
	  static uint32_t raw_ltc[3] = {0x1050006, 0x9030005, SYNC_WORD};
	  ltc_out_buffer_fill(raw_ltc, 0);
	  ltc_out_buffer_fill(raw_ltc, 1);
//	  __BKPT();
  }
#endif

  // static const uint32_t tmp[4] = {0xF0F0F0F0, 0xF0F0F0F0, 0xF0F0F0F0, 0xF0F0F0F0};
  // static const uint32_t tmp[4] = {0x0, 0x0, 0x0, 0x0};
  // static const uint32_t tmp[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0};
//  static const uint32_t tmp[] = {0xFFFFFFFF, 0, 0xFFFFFFFF, 0};
//  static uint32_t raw_ltc[3] = {0x1050006, 0x9030005, SYNC_WORD};

