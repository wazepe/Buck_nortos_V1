#include "encoder.h"
#include "tim.h"

uint16_t stepCount;

void Encoder_Init(uint16_t stepCnt)
{
  stepCount = stepCnt * 2;
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
}

void Encoder_ClearCount(void)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
}

void Encoder_SetCount(uint16_t cnt) 
{
    __HAL_TIM_SetCounter(&htim2, cnt);
}

int16_t Encoder_GetCount(void)
{
    int16_t temp = __HAL_TIM_GetCounter(&htim2);

    if (temp <= 0) {
        Encoder_ClearCount();
        temp = 0;
    } else if (temp > stepCount) {
        __HAL_TIM_SET_COUNTER(&htim2, stepCount);
        temp = stepCount;
    }
    
    return temp / 2;
}
