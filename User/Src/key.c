#include "key.h"

// ========== 按键配置参数 ==========
typedef struct {
    GPIO_TypeDef* port;     // GPIO端口
    uint16_t pin;           // GPIO引脚
    KeyPolarity_t polarity; // 电平极性
} KeyConfig_t;

// 按键配置表（根据实际硬件修改）
static const KeyConfig_t KeyConfig[KEY_COUNT] = {
    [KEY1] = {KEY1_GPIO_Port, KEY1_Pin, KEY_POLARITY_ACTIVE_LOW},
    [KEY2]   = {KEY2_GPIO_Port, KEY2_Pin,   KEY_POLARITY_ACTIVE_LOW},
    [KEY3]   = {KEY3_GPIO_Port, KEY3_Pin,   KEY_POLARITY_ACTIVE_LOW},
    [KEY4]   = {KEY4_GPIO_Port, KEY4_Pin,   KEY_POLARITY_ACTIVE_LOW}
};

// 时间参数配置
typedef struct {
    uint16_t doubleClickMs;  // 双击检测窗口(ms)
    uint16_t longPressMs;    // 长按判定时间(ms)
    uint16_t repeatMs;       // 长按重复间隔(ms)
    uint8_t debounceMs;      // 消抖时间(ms)
} KeyTimeConfig_t;

static const KeyTimeConfig_t KeyTime = {
    .doubleClickMs = 200,
    .longPressMs   = 1000,
    .repeatMs      = 100,
    .debounceMs    = 20      // 20ms消抖
};

// ========== 按键实例结构体 ==========
typedef struct {
    uint8_t eventFlags;          // 事件标志位
    uint8_t pressState;          // 当前按下状态（消抖后）
    uint8_t lastPressState;      // 上次按下状态
    uint8_t rawState;            // 原始采样状态
    uint8_t debounceCounter;     // 消抖计数器
    uint8_t stateMachine;        // 状态机状态
    uint16_t timer;              // 计时器（递减）
} KeyInstance_t;

static KeyInstance_t Keys[KEY_COUNT];

// ========== 辅助函数 ==========
// 获取按键原始电平状态（考虑极性）
static inline uint8_t GetRawKeyState(uint8_t keyIndex)
{
    GPIO_PinState pinState = HAL_GPIO_ReadPin(KeyConfig[keyIndex].port, KeyConfig[keyIndex].pin);
    
    if (KeyConfig[keyIndex].polarity == KEY_POLARITY_ACTIVE_LOW) {
        return (pinState == GPIO_PIN_RESET) ? 1 : 0;
    } else {
        return (pinState == GPIO_PIN_SET) ? 1 : 0;
    }
}

// ========== 公共接口 ==========
void Key_Init(void)
{
    for (int i = 0; i < KEY_COUNT; i++) {
        Keys[i].eventFlags = 0;
        Keys[i].pressState = 0;
        Keys[i].lastPressState = 0;
        Keys[i].rawState = 0;
        Keys[i].debounceCounter = 0;
        Keys[i].stateMachine = 0;
        Keys[i].timer = 0;
    }
}

// 检查按键事件（检查后自动清除该事件位）
uint8_t Key_GetEvent(uint8_t keyIndex, uint8_t event)
{
    if (keyIndex >= KEY_COUNT) return 0;
    
    if (Keys[keyIndex].eventFlags & event) {
        if (event != KEY_EVENT_HOLD) {
            Keys[keyIndex].eventFlags &= ~event;
        }
        return 1;
    }
    return 0;
}

// 清除指定事件
void Key_ClearEvent(uint8_t keyIndex, uint8_t event)
{
    if (keyIndex < KEY_COUNT) {
        Keys[keyIndex].eventFlags &= ~event;
    }
}

// 清除所有按键事件
void Key_ClearAll(void)
{
    for (int i = 0; i < KEY_COUNT; i++) {
        Keys[i].eventFlags = 0;
        Keys[i].stateMachine = 0;
        Keys[i].timer = 0;
    }
}

// 周期性处理函数（1ms调用一次）
void Key_Tick(void)
{
    static uint8_t scanCounter = 0;
    
    // 每1ms递减所有定时器
    for (int i = 0; i < KEY_COUNT; i++) {
        if (Keys[i].timer > 0) {
            Keys[i].timer--;
        }
    }
    
    // 每1ms执行一次消抖采样（使用延迟计数器实现分频）
    for (int i = 0; i < KEY_COUNT; i++) {
        KeyInstance_t* key = &Keys[i];
        uint8_t currentRaw = GetRawKeyState(i);
        
        // 消抖处理
        if (currentRaw != key->rawState) {
            // 状态变化，重置消抖计数器
            key->rawState = currentRaw;
            key->debounceCounter = 0;
        } else {
            // 状态稳定，增加计数器
            if (key->debounceCounter < KeyTime.debounceMs) {
                key->debounceCounter++;
                // 消抖完成，更新有效状态
                if (key->debounceCounter == KeyTime.debounceMs) {
                    key->pressState = key->rawState;
                }
            }
        }
    }
    
    // 每20ms执行一次按键状态机（20ms正好是消抖时间）
    scanCounter++;
    if (scanCounter < KeyTime.debounceMs) return;
    scanCounter = 0;
    
    for (int i = 0; i < KEY_COUNT; i++) {
        KeyInstance_t* key = &Keys[i];
        
        // 保存上次状态
        key->lastPressState = key->pressState;
        
        // 更新HOLD标志（按住中）
        if (key->pressState) {
            key->eventFlags |= KEY_EVENT_HOLD;
        } else {
            key->eventFlags &= ~KEY_EVENT_HOLD;
        }
        
        // 检测按下和释放事件
        if (key->pressState && !key->lastPressState) {
            key->eventFlags |= KEY_EVENT_DOWN;   // 按下瞬间
        }
        if (!key->pressState && key->lastPressState) {
            key->eventFlags |= KEY_EVENT_UP;     // 释放瞬间
        }
        
        // 状态机：识别单击、双击、长按、重复
        // 状态0: 等待按键
        // 状态1: 按键已按下，等待释放或长按
        // 状态2: 按键已释放，等待双击
        // 状态3: 双击确认，等待释放
        // 状态4: 长按激活，等待释放或重复
        
        switch (key->stateMachine) {
            case 0:  // 空闲状态
                if (key->pressState) {
                    key->timer = KeyTime.longPressMs;
                    key->stateMachine = 1;
                }
                break;
                
            case 1:  // 按键已按下
                if (!key->pressState) {
                    // 按键释放，进入双击等待窗口
                    key->timer = KeyTime.doubleClickMs;
                    key->stateMachine = 2;
                } else if (key->timer == 0) {
                    // 长按触发
                    key->timer = KeyTime.repeatMs;
                    key->eventFlags |= KEY_EVENT_LONG;
                    key->stateMachine = 4;
                }
                break;
                
            case 2:  // 等待双击
                if (key->pressState) {
                    // 检测到再次按下，触发双击
                    key->eventFlags |= KEY_EVENT_DOUBLE;
                    key->stateMachine = 3;
                } else if (key->timer == 0) {
                    // 超时，触发单击
                    key->eventFlags |= KEY_EVENT_SINGLE;
                    key->stateMachine = 0;
                }
                break;
                
            case 3:  // 双击确认，等待释放
                if (!key->pressState) {
                    key->stateMachine = 0;
                }
                break;
                
            case 4:  // 长按激活状态
                if (!key->pressState) {
                    key->stateMachine = 0;
                } else if (key->timer == 0) {
                    // 长按重复触发
                    key->timer = KeyTime.repeatMs;
                    key->eventFlags |= KEY_EVENT_REPEAT;
                    // 保持状态4，继续等待下一次重复
                }
                break;
        }
    }
}

// void keyProcess(void)
// {
//     if (Key_GetEvent(KEY_BSP, KEY_EVENT_SINGLE)) {
//         // 单击BSP按键
//         Device_SetMode(BSP_LED_INDEX, MODE_BLINK_FAST);
//     }
    
//     if (Key_GetEvent(KEY_BSP, KEY_EVENT_DOUBLE)) {
//         // 双击BSP按键
//         Device_SetMode(BEEPER_INDEX, MODE_BLINK_SHORT);
//     }
    
//     if (Key_GetEvent(KEY_BSP, KEY_EVENT_LONG)) {
//         // 长按BSP按键
//         Device_SetMode(BEEPER_INDEX, MODE_OFF);
//     }
// }
