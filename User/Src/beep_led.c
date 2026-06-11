#include "main.h"
#include "beep_led.h"

// 闪烁参数配置
typedef struct {
    uint16_t period;  // 周期(ms)
    uint16_t duty;    // 占空比(ms)
} BlinkConfig_t;

static const BlinkConfig_t BlinkParams[] = {
    [MODE_BLINK_SLOW] = {1000, 500},
    [MODE_BLINK_FAST] = {100, 50},
    [MODE_BLINK_SHORT] = {1000, 100}
};

// 设备实例结构体
typedef struct {
    WorkMode_t mode;
    uint16_t counter;
    GPIO_TypeDef* port;
    uint16_t pin;
    Polarity_t polarity;
} Device_Instance_t;

// 设备配置数组（混合LED和蜂鸣器）
static Device_Instance_t Devices[] = {
    // LED设备
    {MODE_OFF, 0, LED_GPIO_Port, LED_Pin, POLARITY_HIGH},   // LEDB
    
    // // 有源蜂鸣器
    // {MODE_OFF, 0, BEEP_GPIO_Port, BEEP_Pin, POLARITY_LOW}         // BEEPER
};

// 辅助函数：根据极性和目标状态计算实际GPIO电平
static inline GPIO_PinState GetGPIOState(Polarity_t polarity, uint8_t active) 
{
    // active: 1=想要激活（亮/响）, 0=想要关闭
    if (polarity == POLARITY_HIGH) {
        return active ? GPIO_PIN_SET : GPIO_PIN_RESET;
    } else {
        return active ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
}

// 设置设备模式
void Device_SetMode(uint8_t devIndex, WorkMode_t mode) 
{
    if (devIndex < sizeof(Devices)/sizeof(Devices[0])) {
        Devices[devIndex].mode = mode;
        Devices[devIndex].counter = 0;
    }
}

// 周期性处理函数（1ms中断调用）
void Device_Tick(void) 
{
    for (int i = 0; i < sizeof(Devices)/sizeof(Devices[0]); i++) {
        Device_Instance_t* dev = &Devices[i];
        uint8_t shouldBeActive = 0;  // 逻辑状态：1=激活（亮/响）, 0=关闭
        
        switch (dev->mode) {
            case MODE_OFF:
                shouldBeActive = 0;
                break;
                
            case MODE_ON:
                shouldBeActive = 1;
                break;
                
            default:
                // 处理所有闪烁/鸣响模式
                if (dev->mode >= MODE_BLINK_SLOW && dev->mode <= MODE_BLINK_SHORT) {
                    dev->counter = (dev->counter + 1) % BlinkParams[dev->mode].period;
                    shouldBeActive = (dev->counter < BlinkParams[dev->mode].duty) ? 1 : 0;
                }
                break;
        }
        
        // 根据极性转换为实际GPIO电平
        HAL_GPIO_WritePin(dev->port, dev->pin, GetGPIOState(dev->polarity, shouldBeActive));
    }
}
