#ifndef __KEY_H
#define __KEY_H

#include "main.h"

// 按键索引定义
typedef enum {
    KEY1 = 0,
    KEY2,
    KEY3,
    KEY4,
    KEY_COUNT      // 保留作为按键个数，不是枚举值
} KeyIndex_t;

// 按键事件标志位
typedef enum {
    KEY_EVENT_HOLD    = 0x01,  // 按住中
    KEY_EVENT_DOWN    = 0x02,  // 按下瞬间
    KEY_EVENT_UP      = 0x04,  // 释放瞬间
    KEY_EVENT_SINGLE  = 0x08,  // 单击
    KEY_EVENT_DOUBLE  = 0x10,  // 双击
    KEY_EVENT_LONG    = 0x20,  // 长按
    KEY_EVENT_REPEAT  = 0x40   // 重复（长按连发）
} KeyEvent_t;

// 按键极性定义
typedef enum {
    KEY_POLARITY_ACTIVE_LOW = 0,   // 低电平有效（按下为低电平）
    KEY_POLARITY_ACTIVE_HIGH       // 高电平有效（按下为高电平）
} KeyPolarity_t;

// 公共接口
void Key_Init(void);                    // 初始化所有按键
void Key_Tick(void);                    // 周期性处理（1ms调用一次）
uint8_t Key_GetEvent(uint8_t keyIndex, uint8_t event);  // 检查按键事件
void Key_ClearEvent(uint8_t keyIndex, uint8_t event);   // 清除指定事件
void Key_ClearAll(void);                // 清除所有按键事件

#endif
