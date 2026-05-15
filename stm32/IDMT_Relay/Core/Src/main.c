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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "relay_curves.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* ── ADC / sampling ───────────────────────────────── */
#define ADC_SAMPLES     64
#define ADC_DC_OFFSET   2048
#define COUNTS_TO_AMPS  0.00488f
#define I_PICKUP_DEFAULT 5.0f
#define TMS_DEFAULT      0.5f
#define INST_M_MIN       1.01f

uint16_t adc_buf[ADC_SAMPLES];
volatile uint8_t sample_ready = 0;

/* ── Relay settings (changed by user via UART) ────── */
typedef enum { STD_IEC, STD_IEEE } Standard;

Standard  user_standard  = STD_IEC;
IEC_Curve  user_iec_curve = IEC_SI;
IEEE_Curve user_ieee_curve = IEEE_MOD_INV;
float     user_tms        = TMS_DEFAULT;
float     user_pickup     = I_PICKUP_DEFAULT;   /* amps */
float     user_inst_multiple = INST_MULTIPLE_DEFAULT;

/* ── UART receive ─────────────────────────────────── */
uint8_t  rx_byte       = 0;
uint8_t  rx_buf[32]    = {0};
uint8_t  rx_index      = 0;
volatile uint8_t rx_ready = 0;

/* ── State machine ────────────────────────────────── */
typedef enum {
    MENU_MAIN,
    MENU_STANDARD,
    MENU_CURVE,
    MENU_TMS,
    MENU_PICKUP,
    MENU_INST_MULTIPLE
} MenuState;

MenuState menu_state = MENU_MAIN;

typedef enum { RELAY_NORMAL, RELAY_FAULT_PENDING, RELAY_TRIPPED } RelayState;
RelayState relay_state     = RELAY_NORMAL;
uint32_t   fault_start_ms  = 0;   /* HAL_GetTick() when fault detected */
float      trip_time_ms    = 0;   /* calculated trip time in ms        */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void uart_print(const char *msg)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 200);
}

void show_main_menu(void)
{
    char buf[320];
    const char *std_str  = (user_standard == STD_IEC) ? "IEC" : "IEEE";
    const char *curve_str;

    if (user_standard == STD_IEC) {
        const char *iec_names[] = {"SI","VI","EI","LTI"};
        curve_str = iec_names[user_iec_curve];
    } else {
        const char *ieee_names[] = {"MOD_INV","VERY_INV","EXT_INV"};
        curve_str = ieee_names[user_ieee_curve];
    }

    sprintf(buf,
        "\r\n======= IDMT RELAY =======\r\n"
        " Standard : %s\r\n"
        " Curve    : %s\r\n"
        " TMS/TDS  : %.2f\r\n"
        " Pickup   : %.2f A\r\n"
        " Inst M   : %.2f\r\n"
        "--------------------------\r\n"
        " 1) Change standard\r\n"
        " 2) Change curve\r\n"
        " 3) Change TMS/TDS\r\n"
        " 4) Change pickup current\r\n"
        " 5) Change instant trip M\r\n"
        "==========================\r\n"
        "Enter choice: ",
        std_str, curve_str, user_tms, user_pickup, user_inst_multiple);

    uart_print(buf);
}

void show_standard_menu(void)
{
    uart_print("\r\nSelect standard:\r\n"
               " 1) IEC 60255\r\n"
               " 2) IEEE C37.112\r\n"
               "Enter choice: ");
}

void show_curve_menu(void)
{
    if (user_standard == STD_IEC) {
        uart_print("\r\nSelect IEC curve:\r\n"
                   " 1) Standard Inverse  (SI)\r\n"
                   " 2) Very Inverse      (VI)\r\n"
                   " 3) Extremely Inverse (EI)\r\n"
                   " 4) Long-Time Inverse (LTI)\r\n"
                   "Enter choice: ");
    } else {
        uart_print("\r\nSelect IEEE curve:\r\n"
                   " 1) Moderately Inverse\r\n"
                   " 2) Very Inverse\r\n"
                   " 3) Extremely Inverse\r\n"
                   "Enter choice: ");
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, ADC_SAMPLES);
  HAL_TIM_Base_Start(&htim3);
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  /* start UART interrupt */
  show_main_menu();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      /* ── UART input handler ───────────────────────── */
      if (rx_ready)
      {
          rx_ready = 0;
          char c = (char)rx_buf[0];

          /* echo character back so user sees what they typed */
          HAL_UART_Transmit(&huart1, (uint8_t*)&c, 1, 50);

          switch (menu_state)
          {
              case MENU_MAIN:
                  switch (c) {
                      case '1': menu_state = MENU_STANDARD; show_standard_menu(); break;
                      case '2': menu_state = MENU_CURVE;    show_curve_menu();    break;
                      case '3':
                          menu_state = MENU_TMS;
                          uart_print("\r\nEnter TMS (e.g. 0.50): ");
                          rx_index = 0;
                          break;
                      case '4':
                          menu_state = MENU_PICKUP;
                          uart_print("\r\nEnter pickup amps (e.g. 5.00): ");
                          rx_index = 0;
                          break;
                      case '5':
                          menu_state = MENU_INST_MULTIPLE;
                          uart_print("\r\nEnter instant trip M (e.g. 10.00): ");
                          rx_index = 0;
                          break;
                      default:
                          show_main_menu();
                          break;
                  }
                  break;

              case MENU_STANDARD:
                  if (c == '1')      { user_standard = STD_IEC;  uart_print("\r\nSet to IEC.");  }
                  else if (c == '2') { user_standard = STD_IEEE; uart_print("\r\nSet to IEEE."); }
                  else {
					  uart_print("\r\nInvalid. Enter 1 or 2.");
					  show_standard_menu();
					  break;
				  }
                  menu_state = MENU_MAIN;
                  show_main_menu();
                  break;

              case MENU_CURVE:
                  if (user_standard == STD_IEC) {
                      if      (c == '1') user_iec_curve = IEC_SI;
                      else if (c == '2') user_iec_curve = IEC_VI;
                      else if (c == '3') user_iec_curve = IEC_EI;
                      else if (c == '4') user_iec_curve = IEC_LTI;
                      else {
                    	  uart_print("\r\nInvalid. Enter 1-4.");
						  show_curve_menu();   /* reprint without going back to main */
						  break;
					  }
                  } else {
                      if      (c == '1') user_ieee_curve = IEEE_MOD_INV;
                      else if (c == '2') user_ieee_curve = IEEE_VERY_INV;
                      else if (c == '3') user_ieee_curve = IEEE_EXT_INV;
                      else {
						  uart_print("\r\nInvalid. Enter 1-3.");
						  show_curve_menu();
						  break;
                      }
                  }
                  menu_state = MENU_MAIN;
                  show_main_menu();
                  break;

              case MENU_TMS:
                  /* collect chars until Enter */
                  if (c == '\r' || c == '\n') {
                      rx_buf[rx_index] = '\0';
                      float val = atof((char*)rx_buf);
                      if (val > 0.0f && val <= 10.0f) {
                          user_tms = val;
                          uart_print("\r\nTMS updated.");
                      } else {
                          uart_print("\r\nInvalid. Range: 0.01 - 10.0");
                      }
                      rx_index = 0;
                      menu_state = MENU_MAIN;
                      show_main_menu();
                  } else {
                      if (rx_index < 10) rx_buf[rx_index++] = c;
                  }
                  break;

              case MENU_PICKUP:
                  if (c == '\r' || c == '\n') {
                      rx_buf[rx_index] = '\0';
                      float val = atof((char*)rx_buf);
                      if (val > 0.0f) {
                          user_pickup = val;
                          uart_print("\r\nPickup updated.");
                      } else {
                          uart_print("\r\nInvalid value.");
                      }
                      rx_index = 0;
                      menu_state = MENU_MAIN;
                      show_main_menu();
                  } else {
                      if (rx_index < 10) rx_buf[rx_index++] = c;
                  }
                  break;

              case MENU_INST_MULTIPLE:
                  if (c == '\r' || c == '\n') {
                      rx_buf[rx_index] = '\0';
                      float val = atof((char*)rx_buf);
                      if (val >= INST_M_MIN) {
                          user_inst_multiple = val;
                          uart_print("\r\nInstant trip M updated.");
                      } else {
                          uart_print("\r\nInvalid. Range: 1.01 and above");
                      }
                      rx_index = 0;
                      menu_state = MENU_MAIN;
                      show_main_menu();
                  } else {
                      if (rx_index < 10) rx_buf[rx_index++] = c;
                  }
                  break;
          }
      }

      /* ── ADC / trip logic ─────────────────────────── */
      if (sample_ready)
      {
          sample_ready = 0;

          float rms_counts = calc_rms(adc_buf, ADC_SAMPLES, ADC_DC_OFFSET);
          float I_fault    = rms_counts * COUNTS_TO_AMPS;
          float M          = I_fault / user_pickup;

          char msg[96];

          if (M > 1.0f)
          {
              float t_trip;
              if (user_standard == STD_IEC)
                  t_trip = trip_time_iec(M, user_tms, user_inst_multiple, user_iec_curve);
              else
                  t_trip = trip_time_ieee(M, user_tms, user_inst_multiple, user_ieee_curve);
              switch (relay_state)
              {
              	  case RELAY_NORMAL:
              		  /* ── NEW FAULT DETECTED ── */
                      relay_state    = RELAY_FAULT_PENDING;
                      fault_start_ms = HAL_GetTick();
                      trip_time_ms   = t_trip * 1000.0f;

					  /* T1 timestamp — GUI logs this */
                      sprintf(msg, "FAULT_START t=%.3f M=%.2f Ttrip_theory=%.3fs\r\n",
                              HAL_GetTick()/1000.0f, M, t_trip);
					  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
					  break;
              	  case RELAY_FAULT_PENDING:
					  /* ── CHECK IF TRIP TIME ELAPSED ── */
					  if ((HAL_GetTick() - fault_start_ms) >= (uint32_t)trip_time_ms)
					  {
						  relay_state = RELAY_TRIPPED;

						  /* T2 timestamp — GUI calculates actual = T2 - T1 */
						  HAL_GPIO_WritePin(RELAY_TRIP_GPIO_Port,
											RELAY_TRIP_Pin, GPIO_PIN_SET);
						  uart_print("TRIP_EXECUTED\r\n");
					  }
					  else
					  {
						  /* still counting — report progress */
						  uint32_t elapsed_ms = HAL_GetTick() - fault_start_ms;
						  float remaining = (trip_time_ms - elapsed_ms) / 1000.0f;
						  // FAULT countdown
						  sprintf(msg, "FAULT t=%.3f M=%.2f Tremain=%.3fs\r\n",
						          HAL_GetTick()/1000.0f, M, remaining);
						  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
					  }
					  break;

				  case RELAY_TRIPPED:
					  /* already tripped — just report */
					  sprintf(msg, "TRIPPED t=%.3f M=%.2f\r\n", HAL_GetTick()/1000.0f, M);
					  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
					  break;
			  }
		  }
		  else
		  {
			  /* ── FAULT CLEARED ── */
			  if (relay_state != RELAY_NORMAL)
			  {
				  relay_state = RELAY_NORMAL;
				  HAL_GPIO_WritePin(RELAY_TRIP_GPIO_Port,
									RELAY_TRIP_Pin, GPIO_PIN_RESET);
				  uart_print("FAULT_CLEARED\r\n");
			  }
			  else
			  {
				  sprintf(msg, "OK t=%.3f M=%.2f\r\n", HAL_GetTick()/1000.0f, M);
				  HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
			  }
		  }
	  }

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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_41CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 24;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 899;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RELAY_TRIP_GPIO_Port, RELAY_TRIP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : RELAY_TRIP_Pin */
  GPIO_InitStruct.Pin = RELAY_TRIP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RELAY_TRIP_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
        sample_ready = 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        rx_buf[0] = rx_byte;
        rx_ready  = 1;
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  /* re-arm */
    }
}

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
