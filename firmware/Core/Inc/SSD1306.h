#ifndef SSD1306_h
#define SSD1306_h

#include "i2c.h"

#define SSD1306_MAX_PAGES 	8
#define SSD1306_DATA_OFFSET 5
#define SSD1306_DATA_PADDING 11
#define SSD1306_WIDTH		128

typedef struct SSD1306_ctx_desc
{
	I2C_HandleTypeDef *hi2c;
	uint8_t fb[SSD1306_MAX_PAGES][SSD1306_DATA_OFFSET + SSD1306_WIDTH + SSD1306_DATA_PADDING];
	uint8_t addr, column_shift, pages, current_page, height;
	uint32_t busy, dirty;
	int cb_error_cnt, cb_complete_cnt;
	void (*cb_complete_proc)(struct __I2C_HandleTypeDef *hi2c);
	void (*cb_error_proc)(struct __I2C_HandleTypeDef *hi2c);
} SSD1306_ctx_t;


#define SSD1306_DEF(CTX_NAME, I2C_NAME, I2C_ADDR, HEIGHT, IS_SH1106) \
\
static SSD1306_ctx_t CTX_NAME; \
\
static void SSD1306_##I2C_NAME##_##CTX_NAME##_cb_complete () { \
	SSD1306_cb_complete((SSD1306_ctx_t*)&CTX_NAME); \
} \
\
static void SSD1306_##I2C_NAME##_##CTX_NAME##_cb_error () { \
	SSD1306_cb_error((SSD1306_ctx_t*)&CTX_NAME); \
} \
\
static SSD1306_ctx_t CTX_NAME = { \
	.column_shift = 2 * IS_SH1106, \
	.addr = I2C_ADDR, \
	.height = HEIGHT, \
	.pages = HEIGHT / 8, \
	.hi2c = &I2C_NAME, \
	.cb_complete_proc = SSD1306_##I2C_NAME##_##CTX_NAME##_cb_complete, \
	.cb_error_proc = SSD1306_##I2C_NAME##_##CTX_NAME##_cb_error \
} \

#define SSD1306_I2C_ADDR 0x3C

int SSD1306_setup(SSD1306_ctx_t *ctx);

void SSD1306_cb_complete(SSD1306_ctx_t *ctx);
void SSD1306_cb_error(SSD1306_ctx_t *ctx);
void SSD1306_refresh(SSD1306_ctx_t *ctx);

#endif
