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
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "beep_led.h"
#include "key.h"
#include "encoder.h"
#include "buck_control.h"
#include "pid.h"
#include "blueSerial.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  // 固定输出模式
  OUTPUT_MODE_PRESET,
  // 自定义输出模式
  OUTPUT_MODE_CUSTOM,
} SysOutState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 蓝牙滑杆ID定义 */
#define SLIDER_ID_TAR   1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
int16_t encoderVlue;
float inputVoltage = 20.0f, voltageStep = 0.1f;
float currOut;

PID_t volt = {
  .Kp = 0.8f,
  .Ki = 0.05f,
  .Kd = 0.0f,

  .OutMax = 20.0f,
  .OutMin = 0.0f,
  .ErrorIntMax = 400.0f,
  .ErrorIntMin = 0.0f,
};


SysOutState sysMode = OUTPUT_MODE_PRESET;
// 0 表示旋钮控制 1 表示蓝牙滑杆控制
_Bool ctrlMode;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void oledProcess(void)
{
  if (volt.Target < 10.0f) {
    OLED_Printf(00, 32, OLED_8X16, "T:%-.2fV ", volt.Target);
  } else {
    OLED_Printf(00, 32, OLED_8X16, "T:%-.1fV ", volt.Target);
  }
  if (volt.Actual < 10.0f) {
    OLED_Printf(64, 32, OLED_8X16, "A:%-.2fV ", volt.Actual);
  } else {
    OLED_Printf(64, 32, OLED_8X16, "A:%-.1fV ", volt.Actual);
  }

  if (sysMode == OUTPUT_MODE_PRESET) {
    OLED_Printf(40, 00, OLED_8X16, "PRESET");
  } else if (sysMode == OUTPUT_MODE_CUSTOM) {
    OLED_Printf(40, 00, OLED_8X16, "CUSTOM");

    if (ctrlMode == 1) {
      OLED_Printf(00, 48, OLED_8X16, "Ctrl:BLE ");
    } else {
      OLED_Printf(00, 48, OLED_8X16, "Ctrl:KNOB");
    }
  }

  OLED_Update();
}

void keyProcess(void)
{
  if (Key_GetEvent(KEY1, KEY_EVENT_SINGLE)) {
    // 单击按键1
    sysMode = (SysOutState)((sysMode + 1) % 2);
    OLED_Clear();

    // 进入自定义输出模式 默认为旋钮控制
    if (sysMode == OUTPUT_MODE_CUSTOM) {
      ctrlMode = 0;
    }
  }

  if (Key_GetEvent(KEY2, KEY_EVENT_SINGLE)) {
    // 单击按键2
    if (sysMode == OUTPUT_MODE_PRESET) {
      volt.Target = 3.3f;
    }
  }

  if (Key_GetEvent(KEY3, KEY_EVENT_SINGLE)) {
    // 单击按键3
    if (sysMode == OUTPUT_MODE_PRESET) {
      volt.Target = 5.0f;
    }
  }

  if (Key_GetEvent(KEY4, KEY_EVENT_SINGLE)) {
    // 单击按键4
    if (sysMode == OUTPUT_MODE_PRESET) {
      volt.Target = 12.0f;
    } else if (sysMode == OUTPUT_MODE_CUSTOM) {
      ctrlMode = !ctrlMode;
    }
  }
}

void btProcess(void)
{
  BlueSerial_Packet_t packet;

  /* 读取并处理蓝牙数据包 */
  if (BlueSerial_GetPacket(&packet)) {
    if (packet.type == PACKET_SLIDER) {
      switch (packet.data.slider.id) {
        case SLIDER_ID_TAR:
          if (sysMode == OUTPUT_MODE_CUSTOM) {
            if (ctrlMode == 1) {
              volt.Target = packet.data.slider.value;
            }
          }
          break;

        default:
          break;
      }
    }
  }

  /* 每50ms发送一次plot数据 */
  static uint32_t btTick = 0;
  if (uwTick - btTick >= 50) {
    btTick = uwTick;
    BlueSerial_Printf("[plot,%.3f,%.3f]",
                      volt.Target, volt.Actual);
  }
}


void sysProcess(void)
{
  encoderVlue = Encoder_GetCount();

  if (sysMode == OUTPUT_MODE_CUSTOM) {
    if (ctrlMode == 0) {
      volt.Target = encoderVlue * voltageStep;
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
  HAL_Delay(50);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_TIM2_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init(&hi2c1);
  Key_Init();
  Buck_Init();
  PID_Init(&volt);
  BlueSerial_Init(&huart1);
  Encoder_Init((uint16_t)(volt.OutMax / voltageStep));

  HAL_TIM_Base_Start_IT(&htim4);
  HAL_TIM_Base_Start_IT(&htim3);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    oledProcess();
    keyProcess();
    btProcess();
    sysProcess();
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

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3) {
    // 如果 DMA 已完成一轮 10 次采样，读取平均电压并运行 PID
    if (Buck_ReadVoltageCurrent(&volt.Actual, &currOut)) {
      PID_Update(&volt);
      Buck_SetOutputVoltage(volt.Out);
    }
  }

  if (htim->Instance == TIM4) {
    Device_Tick();
    Key_Tick();
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART1) {
    BlueSerial_RxEventCallback(huart, Size);
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
