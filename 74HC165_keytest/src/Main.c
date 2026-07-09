/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : ����1�շ���ʾ
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "CH58x_common.h"
#include "hc165.h"

uint8_t TxBuff[] = "This is a tx exam\r\n";
uint8_t RxBuff[100];
uint8_t trigB;

/*********************************************************************
 * @fn      main
 *
 * @brief   ������
 *
 * @return  none
 */
int main()
{
    uint8_t len;

    SetSysClock(CLK_SOURCE_PLL_60MHz);

    /* ���ô���1��������IO��ģʽ�������ô��� */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);      // RXD-������������
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA); // TXD-�������������ע������IO������ߵ�ƽ
    UART1_DefInit();

#if 1 // ���Դ��ڷ����ַ���
    UART1_SendString(TxBuff, sizeof(TxBuff));

#endif

#if 1 // ��ѯ��ʽ���������ݺ��ͳ�ȥ
    /* ����ɨ�軺���� */
    static uint32_t last_keys = 0;
    uint8_t first_read = 1;
    char key_buf[32];

    HC165_Init();

    while(1)
    {
        /* ���ڻ��� */
        len = UART1_RecvString(RxBuff);
        if(len)
        {
            UART1_SendString(RxBuff, len);
        }

        /* ����ɨ�� */
        uint32_t keys = HC165_Read24();
        if(first_read || keys != last_keys)
        {
            first_read = 0;
            last_keys = keys;
            /* �������������λ��ǰ (bit23 ... bit0) */
            for(uint8_t i = 0; i < 24; i++)
            {
                key_buf[i] = (keys & (1UL << (23 - i))) ? '0' : '1';
            }
            key_buf[24] = 0x0D;
            key_buf[25] = 0x0A;
            key_buf[26] = 0x00;
            UART1_SendString((uint8_t*)key_buf, 26);
        }

        DelayMs(10);
    }

#endif

#if 0 // �жϷ�ʽ���������ݺ��ͳ�ȥ
    UART1_ByteTrigCfg(UART_7BYTE_TRIG);
    trigB = 7;
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
#endif

    while(1);
}

/*********************************************************************
 * @fn      UART1_IRQHandler
 *
 * @brief   UART1�жϺ���
 *
 * @return  none
 */
__INTERRUPT
__HIGH_CODE
void UART1_IRQHandler(void)
{
    volatile uint8_t i;

    switch(UART1_GetITFlag())
    {
        case UART_II_LINE_STAT: // ��·״̬����
        {
            UART1_GetLinSTA();
            break;
        }

        case UART_II_RECV_RDY: // ���ݴﵽ���ô�����
            for(i = 0; i != trigB; i++)
            {
                RxBuff[i] = UART1_RecvByte();
                UART1_SendByte(RxBuff[i]);
            }
            break;

        case UART_II_RECV_TOUT: // ���ճ�ʱ����ʱһ֡���ݽ������
            i = UART1_RecvString(RxBuff);
            UART1_SendString(RxBuff, i);
            break;

        case UART_II_THR_EMPTY: // ���ͻ������գ��ɼ�������
            break;

        case UART_II_MODEM_CHG: // ֻ֧�ִ���0
            break;

        default:
            break;
    }
}
