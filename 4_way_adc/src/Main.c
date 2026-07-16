/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : 4路ADC摇杆示例
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/* ----- 库配置 (必须在 #include "adc_joystick.h" 之前) ----- */
#define AJ_CHANGE_ONLY  1   /* 仅方向变化时打印 */

#include "CH58x_common.h"
#include "adc_joystick.h"
#include <stdio.h>

int main()
{
    char    buf[64];
    uint8_t prev = 0xFF;

    SetSysClock(CLK_SOURCE_PLL_60MHz);

    /* UART1: PA9=TX, PA8=RX */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    AJ_UART_SEND("CH582M 4-Ch ADC Joystick\r\n", 26);

    /* 初始化 + 校准 */
    AJ_Init();
    AJ_Calibrate();

    /* 主循环 */
    while(1)
    {
        uint16_t mv[4];
        AJ_Direction dir = AJ_Read(mv);

#if AJ_CHANGE_ONLY
        if(dir == prev) { mDelaymS(50); continue; }
        prev = dir;
#endif

        uint8_t len = sprintf(buf, "%s:%04umV %s:%04umV %s:%04umV %s:%04umV | %s\r\n",
                              AJ_ChNames[0], mv[0],
                              AJ_ChNames[1], mv[1],
                              AJ_ChNames[2], mv[2],
                              AJ_ChNames[3], mv[3],
                              AJ_DirName(dir));
        AJ_UART_SEND(buf, len);
        mDelaymS(50);
    }
}
