/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "icache.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd_selftest.h"
#include "LCD_1in69.h"
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
static const UWORD CENTER_RECT_SIZE = 50U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void LCD_ShowCenterWhiteRect(void);
static void LCD_ShowBlackWhiteQuadrants(void);
static void LCD_ShowLargeLetterA(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void LCD_ShowCenterWhiteRect(void)
{
  UWORD rect_x0;
  UWORD rect_y0;
  UWORD rect_x1;
  UWORD rect_y1;

  rect_x0 = (LCD_1IN69.WIDTH - CENTER_RECT_SIZE) / 2U;
  rect_y0 = (LCD_1IN69.HEIGHT - CENTER_RECT_SIZE) / 2U;
  rect_x1 = rect_x0 + CENTER_RECT_SIZE - 1U;
  rect_y1 = rect_y0 + CENTER_RECT_SIZE - 1U;

  LCD_1IN69_FillRect_FastStatic(0, 0, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), LCD_COLOR_BLACK);
  LCD_1IN69_FillRect_FastStatic(rect_x0, rect_y0, rect_x1, rect_y1, LCD_COLOR_WHITE);
}

static void LCD_ShowBlackWhiteQuadrants(void)
{
  UWORD row;
  UWORD col;
  UWORD x0;
  UWORD y0;
  UWORD x1;
  UWORD y1;

  for (row = 0U; row < 10U; row++) {
    y0 = (UWORD)(((UDOUBLE)LCD_1IN69.HEIGHT * row) / 10U);
    y1 = (UWORD)((((UDOUBLE)LCD_1IN69.HEIGHT * (row + 1U)) / 10U) - 1U);

    for (col = 0U; col < 10U; col++) {
      x0 = (UWORD)(((UDOUBLE)LCD_1IN69.WIDTH * col) / 10U);
      x1 = (UWORD)((((UDOUBLE)LCD_1IN69.WIDTH * (col + 1U)) / 10U) - 1U);

      LCD_1IN69_FillRect_FastStatic(x0, y0, x1, y1, (((row + col) & 1U) == 0U) ? LCD_COLOR_WHITE : LCD_COLOR_BLACK);
    }
  }
}

static void LCD_ShowLargeLetterA(void)
{
  static const UBYTE letter_a[9] = {
      0x1CU,
      0x22U,
      0x41U,
      0x41U,
      0x7FU,
      0x41U,
      0x41U,
      0x41U,
      0x41U,
  };
  const UWORD scale = 20U;
  const UWORD glyph_width = 7U;
  const UWORD glyph_height = 9U;
  UWORD origin_x = (UWORD)((LCD_1IN69.WIDTH - (glyph_width * scale)) / 2U);
  UWORD origin_y = (UWORD)((LCD_1IN69.HEIGHT - (glyph_height * scale)) / 2U);
  UWORD row;
  UWORD col;

  LCD_1IN69_FillRect_FastStatic(0, 0, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), LCD_COLOR_BLACK);

  for (row = 0U; row < glyph_height; row++) {
    for (col = 0U; col < glyph_width; col++) {
      if ((letter_a[row] & (1U << (glyph_width - 1U - col))) != 0U) {
        UWORD x0 = (UWORD)(origin_x + (col * scale));
        UWORD y0 = (UWORD)(origin_y + (row * scale));
        UWORD x1 = (UWORD)(x0 + scale - 1U);
        UWORD y1 = (UWORD)(y0 + scale - 1U);

        LCD_1IN69_FillRect_FastStatic(x0, y0, x1, y1, LCD_COLOR_WHITE);
      }
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
  MX_SPI1_Init();
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */

  DEV_Module_Init();
  LCD_1IN69_Init(VERTICAL);

  LCD_ShowCenterWhiteRect();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(3000);
    LCD_ShowBlackWhiteQuadrants();
    HAL_Delay(3000);
    LCD_ShowLargeLetterA();
    HAL_Delay(3000);
    LCD_ShowCenterWhiteRect();
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
  __disable_irq();

  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = LCD_BLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BLK_GPIO_Port, &GPIO_InitStruct);

  while (1)
  {
    HAL_GPIO_TogglePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin);

    for (volatile uint32_t i = 0; i < 3000000; i++)
    {
      __NOP();
    }
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
