#include "blueSerial.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* 缓冲区大小配置 */
#define RX_BUFFER_SIZE   256
#define TX_BUFFER_SIZE   128
#define PACKET_BUFFER_SIZE 64

/* 驱动内部句柄 */
typedef struct {
    UART_HandleTypeDef *huart;          // 串口句柄
    uint8_t rx_buffer[RX_BUFFER_SIZE];  // 接收缓冲区
    uint8_t packet_buffer[PACKET_BUFFER_SIZE];  // 临时包缓冲区
    uint16_t packet_len;                // 当前包长度
    uint8_t in_packet;                  // 是否正在接收包中
    BlueSerial_Packet_t parsed_packet;  // 解析好的数据包
    uint8_t packet_ready;               // 数据包就绪标志
} BlueSerial_Context_t;

/* 静态上下文 */
static BlueSerial_Context_t g_ctx = {0};

/* 私有函数声明 */
static void BlueSerial_ParsePacket(void);
static int16_t BlueSerial_Atoi(const char *str);
static float BlueSerial_Atorf(const char *str);

/*============================================================================
 * 公共接口实现
 *============================================================================*/

HAL_StatusTypeDef BlueSerial_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return HAL_ERROR;
    }
    
    /* 保存句柄 */
    g_ctx.huart = huart;
    
    /* 重置状态 */
    g_ctx.packet_len = 0;
    g_ctx.in_packet = 0;
    g_ctx.packet_ready = 0;
    memset(g_ctx.rx_buffer, 0, RX_BUFFER_SIZE);
    memset(g_ctx.packet_buffer, 0, PACKET_BUFFER_SIZE);
    
    /* 启动空闲中断接收模式 - 利用HAL库的现成函数 */
    /* 使用 ReceiveToIdle_IT，当收到数据后遇到空闲线时会触发回调 */
    if (HAL_UARTEx_ReceiveToIdle_IT(huart, g_ctx.rx_buffer, RX_BUFFER_SIZE) != HAL_OK) {
        return HAL_ERROR;
    }
    
    return HAL_OK;
}

uint8_t BlueSerial_GetPacket(BlueSerial_Packet_t *packet)
{
    if (packet == NULL) {
        return 0;
    }
    
    if (g_ctx.packet_ready) {
        memcpy(packet, &g_ctx.parsed_packet, sizeof(BlueSerial_Packet_t));
        g_ctx.packet_ready = 0;
        return 1;
    }
    
    return 0;
}

HAL_StatusTypeDef BlueSerial_Printf(const char *format, ...)
{
    va_list args;
    int32_t len;
    uint8_t tx_buffer[TX_BUFFER_SIZE];
    
    if (g_ctx.huart == NULL) {
        return HAL_ERROR;
    }
    
    va_start(args, format);
    len = vsnprintf((char*)tx_buffer, TX_BUFFER_SIZE, format, args);
    va_end(args);
    
    if (len <= 0 || len >= TX_BUFFER_SIZE) {
        return HAL_ERROR;
    }
    
    /* 使用HAL库的阻塞发送 */
    return HAL_UART_Transmit(g_ctx.huart, tx_buffer, len, 100);
}

HAL_StatusTypeDef BlueSerial_Send(uint8_t *data, uint16_t len)
{
    if (g_ctx.huart == NULL || data == NULL || len == 0) {
        return HAL_ERROR;
    }
    
    /* 使用中断方式发送 */
    return HAL_UART_Transmit_IT(g_ctx.huart, data, len);
}

void BlueSerial_ClearBuffer(void)
{
    __disable_irq();
    g_ctx.packet_len = 0;
    g_ctx.in_packet = 0;
    g_ctx.packet_ready = 0;
    memset(g_ctx.rx_buffer, 0, RX_BUFFER_SIZE);
    memset(g_ctx.packet_buffer, 0, PACKET_BUFFER_SIZE);
    __enable_irq();
}

/*============================================================================
 * HAL回调处理 - 核心函数
 *============================================================================*/

void BlueSerial_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uint16_t i;
    
    /* 确保是同一个串口 */
    if (huart != g_ctx.huart) {
        return;
    }
    
    /* 处理刚接收到的数据 */
    for (i = 0; i < Size; i++) {
        uint8_t byte = g_ctx.rx_buffer[i];
        
        if (byte == '[') {
            /* 数据包开始 */
            g_ctx.in_packet = 1;
            g_ctx.packet_len = 0;
            if (g_ctx.packet_len < PACKET_BUFFER_SIZE - 1) {
                g_ctx.packet_buffer[g_ctx.packet_len++] = byte;
            }
        } 
        else if (byte == ']' && g_ctx.in_packet) {
            /* 数据包结束 */
            if (g_ctx.packet_len < PACKET_BUFFER_SIZE - 1) {
                g_ctx.packet_buffer[g_ctx.packet_len++] = byte;
                g_ctx.packet_buffer[g_ctx.packet_len] = '\0';
            }
            g_ctx.in_packet = 0;
            
            /* 立即解析数据包 */
            BlueSerial_ParsePacket();
        } 
        else if (g_ctx.in_packet) {
            /* 包内数据 */
            if (g_ctx.packet_len < PACKET_BUFFER_SIZE - 1) {
                g_ctx.packet_buffer[g_ctx.packet_len++] = byte;
            } else {
                /* 缓冲区溢出，丢弃当前包 */
                g_ctx.in_packet = 0;
            }
        }
    }
    
    /* 重新启动接收 */
    HAL_UARTEx_ReceiveToIdle_IT(g_ctx.huart, g_ctx.rx_buffer, RX_BUFFER_SIZE);
}

/*============================================================================
 * 数据包解析
 *============================================================================*/

static void BlueSerial_ParsePacket(void)
{
    char *token;
    char *saveptr;
    char temp_buffer[PACKET_BUFFER_SIZE];
    
    /* 复制包内容，去掉首尾的'['和']' */
    if (g_ctx.packet_len < 3) {
        return;
    }
    
    memcpy(temp_buffer, g_ctx.packet_buffer + 1, g_ctx.packet_len - 2);
    temp_buffer[g_ctx.packet_len - 2] = '\0';
    
    /* 获取数据包类型 */
    token = strtok_r(temp_buffer, ",", &saveptr);
    if (token == NULL) {
        return;
    }
    
    /* 解析按键数据包: [key,按键ID,动作] */
    if (strcmp(token, "key") == 0) {
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.key.id = (uint8_t)atoi(token);
        
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        
        if (strcmp(token, "down") == 0) {
            g_ctx.parsed_packet.data.key.down = 1;
        } else if (strcmp(token, "up") == 0) {
            g_ctx.parsed_packet.data.key.down = 0;
        } else {
            return;
        }
        
        g_ctx.parsed_packet.type = PACKET_KEY;
        g_ctx.packet_ready = 1;
    }
    /* 解析滑杆数据包: [slider,滑杆ID,数值] */
    else if (strcmp(token, "slider") == 0) {
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.slider.id = (uint8_t)atoi(token);
        
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.slider.value = BlueSerial_Atorf(token);
        
        g_ctx.parsed_packet.type = PACKET_SLIDER;
        g_ctx.packet_ready = 1;
    }
    /* 解析摇杆数据包: [joystick,左X,左Y,右X,右Y] */
    else if (strcmp(token, "joystick") == 0) {
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.joystick.left_x = BlueSerial_Atoi(token);
        
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.joystick.left_y = BlueSerial_Atoi(token);
        
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.joystick.right_x = BlueSerial_Atoi(token);
        
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        g_ctx.parsed_packet.data.joystick.right_y = BlueSerial_Atoi(token);
        
        g_ctx.parsed_packet.type = PACKET_JOYSTICK;
        g_ctx.packet_ready = 1;
    }
    /* 新增：解析命令响应包 [id,OK/NO]   id范围1~4 */
    else {
        /* 检查token是否为纯数字（允许负号？协议不允许，但此处严格限定） */
        char *endptr;
        long id = strtol(token, &endptr, 10);
        /* 必须是全数字转换，且id在1~4之间 */
        if (*endptr != '\0' || id < 1 || id > 4) {
            return;
        }
        
        token = strtok_r(NULL, ",", &saveptr);
        if (token == NULL) return;
        
        uint8_t ok = 0;
        if (strcmp(token, "OK") == 0) {
            ok = 1;
        } else if (strcmp(token, "NO") == 0) {
            ok = 0;
        } else {
            return;  /* 无效状态 */
        }
        
        g_ctx.parsed_packet.type = PACKET_CMD_RESP;
        g_ctx.parsed_packet.data.cmd_resp.id = (uint8_t)id;
        g_ctx.parsed_packet.data.cmd_resp.ok = ok;
        g_ctx.packet_ready = 1;
    }
}

/*============================================================================
 * 字符串转换辅助函数
 *============================================================================*/

static int16_t BlueSerial_Atoi(const char *str)
{
    int16_t result = 0;
    uint8_t negative = 0;
    
    if (str == NULL) return 0;
    
    if (*str == '-') {
        negative = 1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return negative ? -result : result;
}

static float BlueSerial_Atorf(const char *str)
{
    float result = 0.0f;
    float fraction = 0.1f;
    uint8_t negative = 0;
    uint8_t is_fraction = 0;
    
    if (str == NULL) return 0.0f;
    
    if (*str == '-') {
        negative = 1;
        str++;
    }
    
    while (*str) {
        if (*str == '.') {
            is_fraction = 1;
            str++;
            continue;
        }
        
        if (*str >= '0' && *str <= '9') {
            if (is_fraction) {
                result += (float)(*str - '0') * fraction;
                fraction *= 0.1f;
            } else {
                result = result * 10.0f + (float)(*str - '0');
            }
        }
        str++;
    }
    
    return negative ? -result : result;
}
