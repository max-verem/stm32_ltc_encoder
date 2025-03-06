#include "ltc_encoder.h"

#define LTC_BITS_LEN			80
#define LTC_32BITS_PER_OUT_BIT 	2
#define LTC_32BITS_WORDS		(LTC_BITS_LEN * LTC_32BITS_PER_OUT_BIT)
#define SYNC_WORD 0xBFFC // 1011 1111 1111 1100

static uint32_t ltc_out_buffer_data[2 * LTC_32BITS_WORDS];
static uint32_t ltc_raw_buffer_data[3] = {0x1050006, 0x9030005, SYNC_WORD};
static uint8_t ltc_bcd_buffer_data[9]= {0, 0, 0, 0, 0, 0, 0, 0};
uint32_t ltc_bcd_update = 0xFFFFFFFF, ltc_bcd_curr = 0;

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
			ltc_out_buffer_data[w_idx + 0] = toggle ? 0xFFFFFFFF : 0x00000000;
			toggle ^= 1;
			ltc_out_buffer_data[w_idx + 1] = toggle ? 0xFFFFFFFF : 0x00000000;
			toggle ^= 1;
		}
		else
		{
			// bit '0'
			ltc_out_buffer_data[w_idx + 0] =
		    ltc_out_buffer_data[w_idx + 1] = toggle ? 0xFFFFFFFF : 0x00000000;
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

static void ltc_bcd_inc()
{
	ltc_bcd_buffer_data[0]++;

#define BCD_TC_CARY(IDX, DECS, ONES)												\
	if(ltc_bcd_buffer_data[IDX] == 10)												\
	{																				\
		ltc_bcd_buffer_data[IDX] = 0;												\
		ltc_bcd_buffer_data[IDX + 1]++;												\
	}																				\
	if(ltc_bcd_buffer_data[IDX + 1] == DECS && ltc_bcd_buffer_data[IDX] == ONES) 	\
	{																				\
		ltc_bcd_buffer_data[IDX + 2]++;												\
		ltc_bcd_buffer_data[IDX + 0] = 0;											\
		ltc_bcd_buffer_data[IDX + 1] = 0;											\
	}

	BCD_TC_CARY(0, 2, 5);
	BCD_TC_CARY(2, 6, 0);
	BCD_TC_CARY(4, 6, 0);
	BCD_TC_CARY(6, 6, 0);

	/* compose last timecode BCD value */
	ltc_bcd_curr = 0;
	ltc_bcd_curr |= ltc_bcd_buffer_data[7]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[6]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[5]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[3]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[4]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[2]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[1]; ltc_bcd_curr <<= 4;
	ltc_bcd_curr |= ltc_bcd_buffer_data[0];

	/* check if we requested to update timecode */
	if(ltc_bcd_update != 0xFFFFFFFF)
	{
		ltc_bcd_buffer_data[0] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[1] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[2] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[3] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[4] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[5] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[6] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;
		ltc_bcd_buffer_data[7] = ltc_bcd_update & 0x0f; ltc_bcd_update >>= 4;

		ltc_bcd_update = 0xFFFFFFFF;
	}
};

static void ltc_bcd_to_raw()
{
	ltc_raw_buffer_data[0] = ltc_raw_buffer_data[1] = 0;

	ltc_raw_buffer_data[0] |= ltc_bcd_buffer_data[3] & 0x07;
	ltc_raw_buffer_data[0] <<= 8;
	ltc_raw_buffer_data[0] |= ltc_bcd_buffer_data[2] & 0x0F;
	ltc_raw_buffer_data[0] <<= 8;
	ltc_raw_buffer_data[0] |= ltc_bcd_buffer_data[1] & 0x03;
	ltc_raw_buffer_data[0] <<= 8;
	ltc_raw_buffer_data[0] |= ltc_bcd_buffer_data[0] & 0x0F;

	ltc_raw_buffer_data[1] |= ltc_bcd_buffer_data[7] & 0x03;
	ltc_raw_buffer_data[1] <<= 8;
	ltc_raw_buffer_data[1] |= ltc_bcd_buffer_data[6] & 0x0F;
	ltc_raw_buffer_data[1] <<= 8;
	ltc_raw_buffer_data[1] |= ltc_bcd_buffer_data[5] & 0x07;
	ltc_raw_buffer_data[1] <<= 8;
	ltc_raw_buffer_data[1] |= ltc_bcd_buffer_data[4] & 0x0F;

	ltc_raw_toggle_bit_59(ltc_raw_buffer_data);
};

static void ltc_new_frame(int half_idx)
{
	ltc_bcd_inc();
	ltc_bcd_to_raw();
	ltc_out_buffer_fill(ltc_raw_buffer_data, half_idx);
};

static void dma_cb_half(I2S_HandleTypeDef *hspi)
{
	ltc_new_frame(0);
};

static void dma_cb_full(I2S_HandleTypeDef *hspi)
{
	ltc_new_frame(1);
};

void ltc_decoder_init(I2S_HandleTypeDef* i2s)
{
	  HAL_I2S_RegisterCallback(i2s, HAL_I2S_TX_HALF_COMPLETE_CB_ID, dma_cb_half);
	  HAL_I2S_RegisterCallback(i2s, HAL_I2S_TX_COMPLETE_CB_ID, dma_cb_full);
	  ltc_new_frame(0);
	  ltc_new_frame(1);
	  HAL_I2S_Transmit_DMA(i2s, (uint16_t*)ltc_out_buffer_data, sizeof(ltc_out_buffer_data));
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

