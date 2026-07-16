/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2020/08/06
 * Description        : XInput 摇杆: 4路 ADC 方向检测 → USB XInput 报告
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/

#include "CH58x_common.h"
#include "usb_xinput.h"
#include <stdio.h>

int main()
{
    char    buf[64];
    uint8_t seq = 0;

    SetSysClock(CLK_SOURCE_PLL_60MHz);

    /* UART1: PA9=TX, PA8=RX */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    AJ_UART_SEND("XInput joystick (ADC)\r\n", 22);

    /* ADC 摇杆初始化 + 校准 */
    AJ_Init();
    AJ_Calibrate();

    /* USB XInput 初始化 + 等待握手 */
    InitUSBXinput();
    mDelaymS(1000);

    /* 主循环: 读取摇杆 → 发送 USB 报告 */
    AJ_Direction prev = AJ_DIR_N;

    while(1)
    {
        uint16_t mv[4];
        AJ_Direction dir = AJ_Read(mv);

        /* 方向变化时发送报告 (AJ_CHANGE_ONLY 默认开启) */
        if(dir != prev)
        {
            prev = dir;
            seq++;

            /* 构建并发送 USB 报告 */
            ReportDataXinput report;
            AJ_BuildXInputReport(dir, &report);
            SendUSBXinputReport(&report);

            /* 串口打印 */
            uint8_t len = sprintf(buf,
                "[%02u] %s lx:%+6d ly:%+6d  | %s:%4u %s:%4u %s:%4u %s:%4u mV\r\n",
                seq, AJ_DirName(dir), report.l_x, report.l_y,
                AJ_ChNames[0], mv[0], AJ_ChNames[1], mv[1],
                AJ_ChNames[2], mv[2], AJ_ChNames[3], mv[3]);
            AJ_UART_SEND(buf, len);
        }

        mDelaymS(10);
    }
}
