#ifndef BLUE_SERIAL_H
#define BLUE_SERIAL_H

#include "main.h"

/* 数据包类型 */
typedef enum {
    PACKET_NONE = 0,
    PACKET_KEY,
    PACKET_SLIDER,
    PACKET_JOYSTICK,
    PACKET_CMD_RESP      /* 新增：命令响应包 [id,OK/NO] */
} PacketType_t;

/* 按键数据包 */
typedef struct {
    uint8_t id;      // 按键ID: 1-6
    uint8_t down;    // 1:按下, 0:释放
} KeyPacket_t;

/* 滑杆数据包 */
typedef struct {
    uint8_t id;      // 滑杆ID: 1-4
    float value;     // 滑杆值
} SliderPacket_t;

/* 摇杆数据包 */
typedef struct {
    int16_t left_x;   // -100 到 100
    int16_t left_y;   // -100 到 100
    int16_t right_x;  // -100 到 100
    int16_t right_y;  // -100 到 100
} JoystickPacket_t;

/* 新增：命令响应数据包 */
typedef struct {
    uint8_t id;      // 命令ID: 1-4
    uint8_t ok;      // 1:OK, 0:NO
} CmdRespPacket_t;

/* 统一数据包 */
typedef struct {
    PacketType_t type;
    union {
        KeyPacket_t key;
        SliderPacket_t slider;
        JoystickPacket_t joystick;
        CmdRespPacket_t cmd_resp;   /* 新增 */
    } data;
} BlueSerial_Packet_t;

/*============================================================================
 * 用户调用接口
 *============================================================================*/

/**
 * @brief 初始化蓝牙串口驱动（传入已配置好的串口句柄）
 * @param huart CubeMX初始化好的UART句柄指针
 * @retval HAL_OK 成功, HAL_ERROR 失败
 */
HAL_StatusTypeDef BlueSerial_Init(UART_HandleTypeDef *huart);

/**
 * @brief 获取一个完整解析好的数据包（非阻塞）
 * @param packet 输出参数，指向存储数据包的结构体
 * @retval 1: 成功获取数据包, 0: 无数据包
 */
uint8_t BlueSerial_GetPacket(BlueSerial_Packet_t *packet);

/**
 * @brief 发送格式化字符串（阻塞方式）
 * @param format 格式化字符串
 * @retval HAL状态
 */
HAL_StatusTypeDef BlueSerial_Printf(const char *format, ...);

/**
 * @brief 发送原始数据（中断方式）
 * @param data 数据指针
 * @param len 数据长度
 * @retval HAL状态
 */
HAL_StatusTypeDef BlueSerial_Send(uint8_t *data, uint16_t len);

/**
 * @brief 清空接收缓冲区
 */
void BlueSerial_ClearBuffer(void);

/*============================================================================
 * 以下函数需要在 HAL 回调中调用
 *============================================================================*/

/**
 * @brief 空闲中断接收完成回调处理函数
 * @note 在 HAL_UARTEx_RxEventCallback 中调用此函数
 */
void BlueSerial_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

#endif /* BLUE_SERIAL_H */
