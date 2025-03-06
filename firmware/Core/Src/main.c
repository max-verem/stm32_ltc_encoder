/* USER CODE BEGIN Header */

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "i2s.h"
#include "tim.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <stdatomic.h>
#include "ltc_encoder.h"
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define TIMER1_DIV(DIV) if(!(timer1_cnt % DIV))
#define TIMER1_DIV_BLINK 50
volatile int timer1_cnt = 0;
static void timer1_cb(TIM_HandleTypeDef *htim)
{
	timer1_cnt++;

	/* blink fast it timecode detected */
	TIMER1_DIV(TIMER1_DIV_BLINK)
		HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

	/* this is for debug timer */
	// HAL_GPIO_WritePin(SYNC_GPIO_Port, SYNC_Pin, timer1_cnt % 3 ? GPIO_PIN_SET : GPIO_PIN_RESET);
//	HAL_GPIO_TogglePin(SYNC_GPIO_Port, SYNC_Pin);
};



#define CLI_RECV_BUFFER_SIZE	1024
#define CLI_SEND_BUFFER_SIZE	1024
#define CLI_LINE_BUFFER_SIZE	64
static uint8_t
	cli_recv_buffer[CLI_RECV_BUFFER_SIZE],
	cli_send_buffer[CLI_SEND_BUFFER_SIZE],
	cli_line_buffer[CLI_LINE_BUFFER_SIZE];
volatile static uint32_t
	cli_recv_head = 0,
	cli_recv_tail = 0,
	cli_send_head = 0,
	cli_send_curr = 0, // index of not sent characters
	cli_send_tail = 0, // next available index to store character to send
	cli_line_size = 0;

void cli_callback_recv_data(uint8_t* buf, uint32_t len)
{
	int i;

	for(i = 0; i < len; i++, cli_recv_tail++)
		cli_recv_buffer[cli_recv_tail % CLI_RECV_BUFFER_SIZE] = buf[i];
};

void cli_callback_sent_data(uint8_t* buf, uint32_t len)
{
	cli_send_head += len;
};

#define CLI_OUTPUT_CHAR(C) 										\
{																\
	cli_send_buffer[cli_send_tail % CLI_RECV_BUFFER_SIZE] = C;	\
	cli_send_tail++;											\
}

#define CLI_OUTPUT_NL 											\
	CLI_OUTPUT_CHAR('\r');										\
	CLI_OUTPUT_CHAR('\n')

#define CLI_OUTPUT_CHARS(CHARS, CNT)							\
{																\
	int i;														\
	for(i = 0; i < CNT; i++)									\
		CLI_OUTPUT_CHAR(CHARS[i]);								\
}

#define CLI_OUTPUT_MSG(MSG) CLI_OUTPUT_CHARS(MSG, sizeof(MSG))

#define CLI_OUTPUT_TIMECODE_BCD(BCD)							\
		CLI_OUTPUT_CHAR(('0' + ((BCD >> 28) &0x0F)));			\
		CLI_OUTPUT_CHAR(('0' + ((BCD >> 24) &0x0F)));			\
		CLI_OUTPUT_CHAR(':');									\
		CLI_OUTPUT_CHAR(('0' + ((BCD >> 20) &0x0F)));			\
		CLI_OUTPUT_CHAR(('0' + ((BCD >> 16) &0x0F)));			\
		CLI_OUTPUT_CHAR(':');									\
		CLI_OUTPUT_CHAR(('0' + ((BCD >> 12) &0x0F)));			\
		CLI_OUTPUT_CHAR(('0' + ((BCD >>  8) &0x0F)));			\
		CLI_OUTPUT_CHAR(':');									\
		CLI_OUTPUT_CHAR(('0' + ((BCD >>  4) &0x0F)));			\
		CLI_OUTPUT_CHAR(('0' + ((BCD >>  0) &0x0F)));

const static uint8_t cli_msg_help[] =
"# Welcome to LTC encoder!\r\n"
"# Usage:\r\n"
"# [timecode]<enter> - set new timecode\r\n"
"# <ctrl-c> - discard entered string\r\n"
"# <ctrl-r> - restart timecode counter\r\n"
"# <ctrl-z> - start timecode counter from zero\r\n"
"# <ctrl-x> - show last entered timecode\r\n"
"# <tab>    - show current timecode\r\n"
;

const static uint8_t cli_msg_reset[] =
"# Resetting Timecode to 00:00:00:00\r\n"
;

const static uint8_t cli_msg_restart[] =
"# Restarting Timecode from "
;

const static uint8_t cli_msg_current[] =
"# Current Timecode is "
;

const static uint8_t cli_msg_last[] =
"# Last enterd Timecode is "
;

const static uint8_t cli_msg_parse_error[] =
"# ERROR! Failed to parse timecode!\r\n"
"# format supported 00:00:00:00 or 00000000\r\n"
;

const static uint8_t cli_msg_new_value[] =
"# New Timecode is "
;

static uint32_t ltc_bcd_last = 0x12131415;

#define LTC_CHAR_IS_DIGIT(c) ((c >= '0') && (c <= '9'))
#define LTC_LINE_CHAR_IS_DIGIT(IDX) (LTC_CHAR_IS_DIGIT(cli_line_buffer[IDX]))

static void cli_line_process()
{
	int e = 0;
	if(cli_line_size == 8)
	{
		if
		(
			LTC_LINE_CHAR_IS_DIGIT(0) &&
			LTC_LINE_CHAR_IS_DIGIT(1) &&
			LTC_LINE_CHAR_IS_DIGIT(2) &&
			LTC_LINE_CHAR_IS_DIGIT(3) &&
			LTC_LINE_CHAR_IS_DIGIT(4) &&
			LTC_LINE_CHAR_IS_DIGIT(5) &&
			LTC_LINE_CHAR_IS_DIGIT(6) &&
			LTC_LINE_CHAR_IS_DIGIT(7)
		)
		{
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[0] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[1] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[2] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[3] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[4] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[5] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[6] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[7] - '0';
		}
		else
			e = 1;
	}
	else if(cli_line_size == 11)
	{
		if
		(
			LTC_LINE_CHAR_IS_DIGIT(0) &&
			LTC_LINE_CHAR_IS_DIGIT(1) &&
			cli_line_buffer[2] == ':' &&
			LTC_LINE_CHAR_IS_DIGIT(3) &&
			LTC_LINE_CHAR_IS_DIGIT(4) &&
			cli_line_buffer[5] == ':' &&
			LTC_LINE_CHAR_IS_DIGIT(6) &&
			LTC_LINE_CHAR_IS_DIGIT(7) &&
			cli_line_buffer[8] == ':' &&
			LTC_LINE_CHAR_IS_DIGIT(9) &&
			LTC_LINE_CHAR_IS_DIGIT(10)
		)
		{
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[0] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[1] - '0';
			//                                                 [2] == ':'
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[3] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[4] - '0';
			//                                                 [5] == ':'
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[6] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[7] - '0';
			//                                                 [8] == ':'
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[9] - '0';
			ltc_bcd_last <<= 4; ltc_bcd_last |= cli_line_buffer[10] - '0';
		}
		else
			e = 1;
	}
	else
		e = 1;

	if(e)
	{
		CLI_OUTPUT_MSG(cli_msg_parse_error);
		CLI_OUTPUT_NL;
	}
	else
	{
		CLI_OUTPUT_NL;
		CLI_OUTPUT_MSG(cli_msg_new_value);
		CLI_OUTPUT_TIMECODE_BCD(ltc_bcd_last);
		CLI_OUTPUT_NL;
		CLI_OUTPUT_NL;
		ltc_bcd_update = ltc_bcd_last;
	}
}

static void cli_idle()
{
	uint8_t c;
	int s, w, o;
	uint32_t tc;

	/* should we sent some data */
	if(cli_send_curr == cli_send_head && cli_send_curr < cli_send_tail)
	{
		/* find bytes need to send */
		s = cli_send_tail - cli_send_curr;

		/* calc max wrapped size */
		o = cli_send_head % CLI_SEND_BUFFER_SIZE;
		w = CLI_SEND_BUFFER_SIZE - o;
		if(w < s)
			s = w;

		/* check if max block send */
		if(s > 512)
			s = 512;

		CDC_Transmit_FS(cli_send_buffer + o, s);
		cli_send_curr += s;

		return;
	}

	/* check if we received data, then process it */
	while(cli_recv_head < cli_recv_tail)
	{
		/* fetch character */
		c = cli_recv_buffer[cli_recv_head % CLI_RECV_BUFFER_SIZE];
		cli_recv_head++;

		switch(c)
		{
			// <enter>
			case 0x0D:
			case 0x0A:
				CLI_OUTPUT_NL;
				if(cli_line_size)
					cli_line_process();
				else
				{
					CLI_OUTPUT_MSG(cli_msg_help);
					CLI_OUTPUT_NL;
				}
				cli_line_size = 0;
				break;

			// <ctrl-z>
			// start from zero
			case 0x1A:
				CLI_OUTPUT_NL;
				CLI_OUTPUT_MSG(cli_msg_reset);
				CLI_OUTPUT_NL;
				ltc_bcd_update = 0;
				break;

			// <ctrl-r>
			// start from last value
			case 0x12:
				CLI_OUTPUT_NL;
				CLI_OUTPUT_MSG(cli_msg_restart);
				CLI_OUTPUT_TIMECODE_BCD(ltc_bcd_last);
				CLI_OUTPUT_NL;
				CLI_OUTPUT_NL;
				ltc_bcd_update = ltc_bcd_last;
				break;

			// <tab>
			case 0x09:
				CLI_OUTPUT_NL;
				CLI_OUTPUT_MSG(cli_msg_current);
				tc = ltc_bcd_curr;
				CLI_OUTPUT_TIMECODE_BCD(tc);
				CLI_OUTPUT_NL;
				break;

			// <ctrl-x>
			case 0x18:
				CLI_OUTPUT_NL;
				CLI_OUTPUT_MSG(cli_msg_last);
				tc = ltc_bcd_last;
				CLI_OUTPUT_TIMECODE_BCD(tc);
				CLI_OUTPUT_NL;
				break;

			// <ctrl-d>
			case 0x04:
				continue;

			// <ctrl-c>
			case 0x03:
				CLI_OUTPUT_NL;
				CLI_OUTPUT_NL;
				cli_line_size = 0;
				break;

			// <backspace>
			case 0x7F:
			// <del>
			case 0xB1:
				/* 'backspace' implemented in 3 characters */
				if(cli_line_size)
				{
					CLI_OUTPUT_CHAR('\b');
					CLI_OUTPUT_CHAR(' ');
					CLI_OUTPUT_CHAR('\b');
					cli_line_size--;
				}
				break;

			default:
				/* append to send buffer */
				CLI_OUTPUT_CHAR(c);

				/* save to line buffer */
				if(cli_line_size < CLI_LINE_BUFFER_SIZE)
				{
					cli_line_buffer[cli_line_size] = c;
					cli_line_size++;
				};
		}
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  // static const uint32_t tmp[4] = {0xF0F0F0F0, 0xF0F0F0F0, 0xF0F0F0F0, 0xF0F0F0F0};
  // static const uint32_t tmp[4] = {0x0, 0x0, 0x0, 0x0};
  // static const uint32_t tmp[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0};
//  static const uint32_t tmp[] = {0xFFFFFFFF, 0, 0xFFFFFFFF, 0};
//  static uint32_t raw_ltc[3] = {0x1050006, 0x9030005, SYNC_WORD};
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_USB_DEVICE_Init();
  MX_I2S2_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  // start timer
  HAL_TIM_RegisterCallback(&htim1, HAL_TIM_PERIOD_ELAPSED_CB_ID, timer1_cb);
  HAL_TIM_Base_Start_IT(&htim1);

  ltc_decoder_init(&hi2s2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    cli_idle();
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
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV6;
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
