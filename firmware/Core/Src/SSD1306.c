#include "SSD1306.h"


#define SSD1306_CMD_SETCONTRAST 		0x81
#define SSD1306_CMD_DISPLAYALLON_RESUME 0xA4
#define SSD1306_CMD_DISPLAYALLON 		0xA5
#define SSD1306_CMD_NORMALDISPLAY 		0xA6
#define SSD1306_CMD_INVERTDISPLAY 		0xA7
#define SSD1306_CMD_DISPLAYOFF 			0xAE
#define SSD1306_CMD_DISPLAYON 			0xAF
#define SSD1306_CMD_SETDISPLAYOFFSET 	0xD3
#define SSD1306_CMD_SETCOMPINS 			0xDA
#define SSD1306_CMD_SETVCOMDETECT 		0xDB
#define SSD1306_CMD_SETDISPLAYCLOCKDIV 	0xD5
#define SSD1306_CMD_SETPRECHARGE 		0xD9
#define SSD1306_CMD_SETMULTIPLEX 		0xA8
#define SSD1306_CMD_SETLOWCOLUMN 		0x00
#define SSD1306_CMD_SETHIGHCOLUMN 		0x10
#define SSD1306_CMD_SETSTARTLINE 		0x40
#define SSD1306_CMD_MEMORYMODE 			0x20
#define SSD1306_CMD_COLUMNADDR 			0x21
#define SSD1306_CMD_PAGEADDR 			0x22
#define SSD1306_CMD_COMSCANINC 			0xC0
#define SSD1306_CMD_COMSCANDEC 			0xC8
#define SSD1306_CMD_SEGREMAP 			0xA0
#define SSD1306_CMD_CHARGEPUMP 			0x8D
#define SSD1306_CMD_SWITCHCAPVCC 		0x02
#define SSD1306_CMD_NOP 				0xE3
#define SSD1306_CMD_SETPAGESTARTADDR	0xB0

#define SSD1306_CTL_COMMAND_STREAM		0x00
#define SSD1306_CTL_DATA_STREAM			0x40
#define SSD1306_CTL_SINGLE_CMD_BYTE		0x80
#define SSD1306_CTL_SINGLE_DATA_BYTE	0xC0

void SSD1306_cb_error(SSD1306_ctx_t *ctx)
{
	ctx->cb_error_cnt++;
}

void SSD1306_cb_complete(SSD1306_ctx_t *ctx)
{
	uint32_t cp = 1 << ctx->current_page;

	// cnt
	ctx->cb_complete_cnt++;

	// check if data need transfer
	if(ctx->busy & cp && ctx->dirty & cp)
	{
		HAL_I2C_Master_Transmit_DMA(ctx->hi2c, ctx->addr << 1,
			ctx->fb[ctx->current_page] + SSD1306_DATA_OFFSET - 1, SSD1306_WIDTH + 1);

		// clear dirty bit
		ctx->dirty &= ~cp;
		return;
	}

	// clear busy bit
	ctx->busy &= ~cp;

	SSD1306_refresh(ctx);
}

int SSD1306_setup(SSD1306_ctx_t *ctx)
{
	int r, i;

	uint8_t buf[] = {
		SSD1306_CTL_COMMAND_STREAM,

		// Figure 2 : Software Initialization Flow Chart
		/* primary setup */

		SSD1306_CMD_DISPLAYOFF, // turn off display

		/* Set Multiplex Ratio */
		SSD1306_CMD_SETMULTIPLEX,
		(ctx->height == 32) ?
			0x1F				//128×32 (N=31 + 1)
			:
			0x3F,				//128×64 (RESET)

		/* 10.1.15 Set Display Offset */
		SSD1306_CMD_SETDISPLAYOFFSET,
		0x00,

		/* Set Display Start Line */
		SSD1306_CMD_SETSTARTLINE | 0x00,

		/* Set Segment Re-map */
		SSD1306_CMD_SEGREMAP | 0x1,	// column address 127 is mapped to SEG0, Flip?

		/* Set COM Output Scan Direction */
		SSD1306_CMD_COMSCANDEC,		// remapped mode. Scan from COM[N-1] to COM0 Where N is the Multiplex ratio

		/* Set COM Pins Hardware Configuration */
		SSD1306_CMD_SETCOMPINS,
		(ctx->height == 32) ?
			// A[4]=0b, Sequential COM pin configuration
			// A[5]=0b(RESET), Disable COM Left/Right remap
			0x02 	//128×32
			:
			// A[4]=1b(RESET), Alternative COM pin configuration
			// A[5]=0b(RESET), Disable COM Left/Right remap
			0x12,	//128×64

		/* Set Contrast Control for BANK0 */
		SSD1306_CMD_SETCONTRAST,
		0xCF, // Max contrast (RESET = 7Fh )

		/* Entire Display ON */
		SSD1306_CMD_DISPLAYALLON_RESUME, // Resume to RAM content display (RESET)

		/* Set Normal/Inverse Display */
		SSD1306_CMD_NORMALDISPLAY,

		/* Set Display Clock Divide Ratio/Oscillator Frequency */
		SSD1306_CMD_SETDISPLAYCLOCKDIV,
		0x80,	// Oscillator Frequency = [1]  , Divide ratio = [0] + 1

		/* Charge Pump Setting */
		SSD1306_CMD_CHARGEPUMP,
		0x14,	// Enable charge pump during display on

		/* Set Memory Addressing Mode */
		SSD1306_CMD_MEMORYMODE,
		0x02,	// Page addressing mode

		/* Set Pre-charge Period */
		SSD1306_CMD_SETPRECHARGE,
		0xF1,

		/* Set VCOMH Deselect Level */
		SSD1306_CMD_SETVCOMDETECT,
		0x40,	//0x40

		/* Set Display ON/OFF */
		SSD1306_CMD_DISPLAYON,	// Display ON in normal mode
	};

	r = HAL_I2C_Master_Transmit(ctx->hi2c, ctx->addr << 1, buf, sizeof(buf), 100);

	// setup commands data in page buffers
	for(i = 0; i < ctx->pages; i++)
	{
		/* setup buffer, set proper page */
		ctx->fb[i][0] = SSD1306_CTL_COMMAND_STREAM;

		// Set Page Start Address for Page Addressing Mode
		ctx->fb[i][1] = SSD1306_CMD_SETPAGESTARTADDR | i;

		// Set Lower Column Start Address for Page Addressing Mode
		ctx->fb[i][2] = SSD1306_CMD_SETLOWCOLUMN + ctx->column_shift + 0;

		// Set Higher Column Start Address for Page Addressing Mode
		ctx->fb[i][3] = SSD1306_CMD_SETHIGHCOLUMN + 0;

		/* init data buffer */
		ctx->fb[i][4] = SSD1306_CTL_DATA_STREAM;
	}

	// register callbacks
	HAL_I2C_RegisterCallback(ctx->hi2c, HAL_I2C_MASTER_TX_COMPLETE_CB_ID, ctx->cb_complete_proc);
	HAL_I2C_RegisterCallback(ctx->hi2c, HAL_I2C_ERROR_CB_ID, ctx->cb_error_proc);
	HAL_I2C_RegisterCallback(ctx->hi2c, HAL_I2C_ABORT_CB_ID, ctx->cb_error_proc);
	HAL_I2C_RegisterCallback(ctx->hi2c, HAL_I2C_MEM_RX_COMPLETE_CB_ID, ctx->cb_error_proc);
	HAL_I2C_RegisterCallback(ctx->hi2c, HAL_I2C_MEM_TX_COMPLETE_CB_ID, ctx->cb_error_proc);
	HAL_I2C_RegisterCallback(ctx->hi2c, HAL_I2C_LISTEN_COMPLETE_CB_ID, ctx->cb_error_proc);

	return r;
}

void SSD1306_refresh(SSD1306_ctx_t *ctx)
{
	int r;

	if(ctx->busy)
		return;

	// fix dirty mask
	ctx->dirty &= (1 << ctx->pages) - 1;
	if(!ctx->dirty)
		return;

	// find next dirty page
	for(r = 0; r < ctx->pages; r++)
	{
		// inc page counter
		ctx->current_page++;
		if(ctx->current_page == ctx->pages)
			ctx->current_page = 0;

		if(ctx->dirty & (1 << ctx->current_page))
			break;
	};

	// setup busy flag
	ctx->busy |= 1 << ctx->current_page;

	/* send first transaction */
	HAL_I2C_Master_Transmit_DMA(ctx->hi2c, ctx->addr << 1,
		ctx->fb[ctx->current_page], 4);
}
