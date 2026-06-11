#include "buck_control.h"
#include "adc.h"
#include "tim.h"
#include "pid.h"

extern float inputVoltage;

static uint16_t adcData[10];
volatile uint8_t adcConvComplete = 0;

// 初始化Buck变换器
void Buck_Init(void)
{
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);

  // 启动 TIM1 CH3（CubeMX 已配置好参数，这里只使能通道输出）
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);

  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adcData, 10);
}

// 关闭输出电压
void Buck_DisableOutput(void)
{
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
  HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_3);
}

// 设置目标输出电压
void Buck_SetOutputVoltage(float targetVoltage)
{
  uint16_t dutyCycle;
  float outMax = inputVoltage * 0.951f;
  float outMin = inputVoltage * 0.049f;

  if (targetVoltage > outMax) {
    dutyCycle = 684;   // 最大占空比对应的比较值
  } else if (targetVoltage < outMin) {
    dutyCycle = 0;
  } else {
    // 占空比 = Vout / Vin，映射到TIM比较值（720 = 100%占空比）
    dutyCycle = (uint16_t)(targetVoltage * 720.0f / inputVoltage);
  }

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, dutyCycle);

  // CH3 触发点设在 PWM 高电平中点
  if (dutyCycle >= 2) {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, dutyCycle / 2);
  } else {
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 1);  // 占空比太小时给个最小值
  }
}

// ADC DMA 完成中断回调
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc == &hadc1) {
    adcConvComplete = 1;
  }
}

// 读取电压和电流，返回 1 表示读取成功，0 表示 DMA 尚未完成
uint8_t Buck_ReadVoltageCurrent(float *voltage, float *current)
{
  if (!adcConvComplete) {
    return 0;
  }
  adcConvComplete = 0;

  uint32_t sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += adcData[i];
  }

  *voltage = ((float)sum / 10.0f) * 33.0f / 4095.0f;
  *current = 0.0f;

  return 1;
}
