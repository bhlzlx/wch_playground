/*******************************************************************************
 * usb_xinput.h - USB XInput 手柄协议适配
 *
 * 从 ch5xx_gamepad 项目精简移植
 *******************************************************************************/
#ifndef __USB_XINPUT_H__
#define __USB_XINPUT_H__

#include "CH58x_common.h"
#include "adc_joystick.h"

#define DevEP0SIZE    0x40

/* 支持的最大接口数量 */
#define USB_INTERFACE_MAX_NUM       1
#define USB_INTERFACE_MAX_INDEX     0

/* XInput 报告结构 (20 字节, packed)
 *
 * 对应 Xbox 360 XInput USB 线协议:
 *   [0]    消息类型 (0x00=数据, 0x01=心跳)
 *   [1]    包大小 (0x14=20)
 *   [2]    低位按键: D-pad(b0-3) + Start(b4) + Back(b5) + LS(b6) + RS(b7)
 *   [3]    高位按键: A(b0)+B(b1)+X(b2)+Y(b3)+LB(b4)+RB(b5)+Guide(b6)
 *   [4]    左扳机 (0-255)
 *   [5]    右扳机 (0-255)
 *   [6-7]  左摇杆 X (int16, -32768..32767)
 *   [8-9]  左摇杆 Y
 *   [10-11] 右摇杆 X
 *   [12-13] 右摇杆 Y
 *   [14-19] 保留
 */
typedef struct __attribute__((packed, aligned(1))) {
    uint8_t  rid;                   /* 消息类型: 0x00 */
    uint8_t  rsize;                 /* 包大小: 0x14 */
    uint8_t  digital_buttons_1;     /* D-pad + Start/Back/LS/RS */
    uint8_t  digital_buttons_2;     /* A/B/X/Y/LB/RB/Guide */
    uint8_t  lt;                    /* 左扳机 0-255 */
    uint8_t  rt;                    /* 右扳机 0-255 */
    int16_t  l_x;                   /* 左摇杆 X (-32768..32767) */
    int16_t  l_y;                   /* 左摇杆 Y */
    int16_t  r_x;                   /* 右摇杆 X */
    int16_t  r_y;                   /* 右摇杆 Y */
    uint8_t  reserved_1[6];         /* 保留 */
} ReportDataXinput;

/* D-pad 位定义 (在 digital_buttons_1 中) */
#define XINPUT_DPAD_UP    0x01
#define XINPUT_DPAD_DOWN  0x02
#define XINPUT_DPAD_LEFT  0x04
#define XINPUT_DPAD_RIGHT 0x08

/* API */
void  SendUSBXinputReport(ReportDataXinput const *report);
int   InitUSBXinput(void);
void  USB_DevTransProcess_Xinput(void);

/* AJ_Direction → XInput 报告填充 */
void  AJ_BuildXInputReport(AJ_Direction dir, ReportDataXinput *out);

/* USB 连接状态: bit0=已分配地址, bit1=收到总线复位 */
static inline uint8_t USBConnected(void)
{
    uint8_t result = 0;
    if(R8_USB_DEV_AD)            result |= 1;
    if(R8_USB_INT_FG & RB_UIF_BUS_RST) result |= 2;
    return result;
}

#endif /* __USB_XINPUT_H__ */
