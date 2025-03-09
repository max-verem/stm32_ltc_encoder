/* USER CODE BEGIN Header */

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//#include <stdatomic.h>
#include "ltc_encoder.h"
#include "ltc_decoder.h"
#include "cli.h"
#include "SSD1306.h"
#include "font-14x30.h"

#include "usb_device.h"
#include "usbd_cdc_acm_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

SSD1306_DEF(oled1, hi2c1, SSD1306_I2C_ADDR, 32, 0);

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

uint32_t tc_bcd_normalize_test = 0;

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define TC_DISPLAY_UNKNOWN 	0xAAAAAAAA
#define TC_DISPLAY_NONE 	0xFFFFFFFF
uint32_t tc_display = TC_DISPLAY_UNKNOWN, tc_displayed = TC_DISPLAY_NONE;

#define TIMER_A_DIV(DIV) if(!(timerA_cnt % DIV))
#define TIMER_A_DIV_BLINK_FAST 50
#define TIMER_A_DIV_BLINK_SLOW 500
volatile int timerA_cnt = 0;
static void timerA_cb(TIM_HandleTypeDef *htim)
{
	unsigned int now = HAL_GetTick();

	timerA_cnt++;

	/* blink fast it timecode detected */
	/* check if we should solid led if no timecode detected */
	if((ltc_last + 1000) < now)
	{
		// HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
		tc_display = TC_DISPLAY_UNKNOWN;

		TIMER_A_DIV(TIMER_A_DIV_BLINK_SLOW)
			HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
	}
	else
	{
		/* blink fast it timecode detected */
		TIMER_A_DIV(TIMER_A_DIV_BLINK_FAST)
			HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
	};

	SSD1306_refresh(&oled1);
};

extern USBD_HandleTypeDef hUsbDevice;

static volatile int cdc_ready[2] = {0};
static void ltc_decoder_callback(uint32_t tc_bcd, uint8_t *tc_str_data, uint32_t tc_str_len)
{
	tc_display = tc_bcd;
	if(cdc_ready[1])
		CDC_Transmit(1, tc_str_data, tc_str_len);
}

static uint32_t console_font_14x30_transposed[16 /* glyphs */][14 /* width */];

static void console_font_14x30_transpose()
{
	int g;

	for(g = 0; g < 12; g++)
	{
		int w, b, o;
		const uint8_t *src_row = console_font_14x30 + 2 * 30 * g;

		for(w = 0, o = 0, b = 0x80; w < 14; w++, b >>= 1)
		{
			int h;
			uint32_t W = 0;

			if(!b)
			{
				b = 0x80;
				o = 1;
			}

			for(h = 0; h < 30; h++)
				if(src_row[h * 2 + o] & b)
					W |= 1 << h;

			console_font_14x30_transposed[g][w] = W << 4;
		}
	};
}

volatile int semicolon_offset = 3;

static void oled_idle()
{
	uint32_t bcd;
	int i = 0, j, s;

	if(tc_display == tc_displayed || oled1.dirty || oled1.busy)
		return;

#if 1
    /* this is for estimating display rendering time */
	HAL_GPIO_WritePin(TP2_GPIO_Port, TP2_Pin, GPIO_PIN_SET);
#endif

    bcd = tc_display;
	tc_displayed = tc_display;

	for(j = 0, s = 28; j < 8; j++, s -= 4)
	{
		int w, g;

		// spacer
		if(j && !(j & 1))
		for(w = 0; w < 5; w++, i++)
		{
			uint32_t W = console_font_14x30_transposed[11 /* glyphs */][w + semicolon_offset /* width */];

			oled1.fb[0][SSD1306_DATA_OFFSET + i] = W >>  0;
			oled1.fb[1][SSD1306_DATA_OFFSET + i] = W >>  8;
			oled1.fb[2][SSD1306_DATA_OFFSET + i] = W >> 16;
			oled1.fb[3][SSD1306_DATA_OFFSET + i] = W >> 24;
		}

		// find glyph index
		g = bcd >> s;
		g &= 0x0F;
		if(g > 10) g = 10;

		// copy glyph bitmap data
		for(w = 0; w < 14; w++, i++)
		{
		    uint32_t W = console_font_14x30_transposed[g /* glyphs */][w /* width */];

			oled1.fb[0][SSD1306_DATA_OFFSET + i] = W >>  0;
			oled1.fb[1][SSD1306_DATA_OFFSET + i] = W >>  8;
			oled1.fb[2][SSD1306_DATA_OFFSET + i] = W >> 16;
			oled1.fb[3][SSD1306_DATA_OFFSET + i] = W >> 24;
		}
	}

	// mark page dirty
	oled1.dirty = 0x0f;
#if 1
	/* this is for estimating display rendering time */
	HAL_GPIO_WritePin(TP2_GPIO_Port, TP2_Pin, GPIO_PIN_RESET);
#endif
}

extern USBD_HandleTypeDef hUsbDevice;
extern USBD_CDC_ACM_ItfTypeDef USBD_CDC_ACM_fops;
static USBD_CDC_ACM_ItfTypeDef _USBD_CDC_ACM_fops;
static int8_t _CDC_Init(uint8_t cdc_ch)
{
	int8_t r;

	r = _USBD_CDC_ACM_fops.Init(cdc_ch);

	cdc_ready[cdc_ch]++;

	return r;
}

static int8_t _CDC_Control(uint8_t cdc_ch, uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
	int8_t r;

	r = _USBD_CDC_ACM_fops.Control(cdc_ch, cmd, pbuf, length);

	return r;
}

static int8_t _CDC_Receive(uint8_t cdc_ch, uint8_t *Buf, uint32_t *Len)
{
	if(!cdc_ch)
		cli_callback_recv_data(Buf, *Len);
	USBD_CDC_SetRxBuffer(cdc_ch, &hUsbDevice, &Buf[0]);
	USBD_CDC_ReceivePacket(cdc_ch, &hUsbDevice);
	return (USBD_OK);
}

static int8_t _CDC_TransmitCplt(uint8_t cdc_ch, uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
	if(!cdc_ch)
		cli_callback_sent_data(Buf, *Len);
	return (USBD_OK);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  _USBD_CDC_ACM_fops = USBD_CDC_ACM_fops;
  USBD_CDC_ACM_fops.Init = _CDC_Init;
  USBD_CDC_ACM_fops.Control = _CDC_Control;
  USBD_CDC_ACM_fops.TransmitCplt = _CDC_TransmitCplt;
  USBD_CDC_ACM_fops.Receive = _CDC_Receive;
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USB_OTG_FS_PCD_Init();
  /* USER CODE BEGIN 2 */

  MX_USB_DEVICE_Init();

  // start generic timer
  HAL_TIM_RegisterCallback(&htim3, HAL_TIM_PERIOD_ELAPSED_CB_ID, timerA_cb);
  HAL_TIM_Base_Start_IT(&htim3);

  // ltc out init and run
  ltc_encoder_init(&htim1, LTC_OUT_GPIO_Port, LTC_OUT_Pin);

  // ltc in init and run
  ltc_decoder_init(&htim2);

  // setup and init oled display
  console_font_14x30_transpose();
  SSD1306_setup(&oled1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	ltc_encoder_idle();
	ltc_decoder_idle(ltc_decoder_callback);
    cli_idle();
    oled_idle();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
