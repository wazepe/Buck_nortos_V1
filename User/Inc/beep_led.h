#ifndef __BEEP_LED_H
#define __BEEP_LED_H

// 外设索引定义
#define LED_INDEX       0

// 工作模式定义
typedef enum {
    MODE_OFF = 0,       // 关闭
    MODE_ON,            // 常开/常响
    MODE_BLINK_SLOW,    // 慢闪/慢响 (500ms on, 500ms off)
    MODE_BLINK_FAST,    // 快闪/快响 (50ms on, 50ms off)
    MODE_BLINK_SHORT    // 短闪/短响 (100ms on, 900ms off)
} WorkMode_t;

// 极性定义
typedef enum {
    POLARITY_HIGH = 0,  // 高电平有效
    POLARITY_LOW        // 低电平有效
} Polarity_t;

// 通用接口函数
void Device_SetMode(uint8_t devIndex, WorkMode_t mode);
void Device_Tick(void);

#endif
