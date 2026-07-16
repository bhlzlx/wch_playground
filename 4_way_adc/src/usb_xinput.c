/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_xinput.c
 * Author             : WCH (adapted from ch5xx_gamepad)
 * Version            : V1.0
 * Date               : 2024/01/01
 * Description        : USB XInput 手柄协议栈 (Vendor-class)
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/

#include "usb_xinput.h"

/* ===================================================================
 *  USB 描述符
 * =================================================================== */

/* 设备描述符 - XInput 手柄 (VID=0x045E Microsoft, PID=0x028E Xbox 360) */
const uint8_t MyDevDescr[] = {
    0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, DevEP0SIZE,
    0x5e, 0x04, 0x8e, 0x02, 0x00, 0x00,
    0x01, 0x02, 0x00, 0x01
};

/* 配置描述符 */
const uint8_t MyCfgDescr[] = {
    /* Configuration Descriptor */
    0x09, 0x02, 0x30, 0x00, 0x01, 0x01, 0x00, 0x80, 0xFA,

    /* Interface Descriptor: Vendor-specific, 2 endpoints */
    0x09, 0x04, 0x00, 0x00, 0x02, 0xFF, 0x5D, 0x01, 0x00,

    /* XInput Security Descriptor (type 0x21) */
    0x10, 0x21, 0x10, 0x01, 0x01, 0x24, 0x81, 0x14,
    0x03, 0x00, 0x03, 0x13, 0x02, 0x00, 0x03, 0x00,

    /* Endpoint 1 IN (Interrupt, 32 bytes, 4ms) */
    0x07, 0x05, 0x81, 0x03, 0x20, 0x00, 0x04,

    /* Endpoint 2 OUT (Interrupt, 32 bytes, 8ms) */
    0x07, 0x05, 0x02, 0x03, 0x20, 0x00, 0x08,
};

/* USB 速度匹配描述符 */
const uint8_t My_QueDescr[] = {
    0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00
};

/* 全速模式其他速度配置描述符 */
uint8_t USB_FS_OSC_DESC[sizeof(MyCfgDescr)] = { 0x09, 0x07 };

/* 语言描述符 */
const uint8_t MyLangDescr[] = { 0x04, 0x03, 0x09, 0x04 };

/* 厂家信息 "XInput" */
const uint8_t MyManuInfo[] = {
    0x0E, 0x03, 'X',0, 'I',0, 'n',0, 'p',0, 'u',0, 't',0
};

/* 产品信息 "CH58x-Gamepad" */
const uint8_t MyProdInfo[] = {
    28, 0x03,
    'C',0, 'H',0, '5',0, '8',0, 'x',0,
    '-',0, 'G',0, 'a',0, 'm',0, 'e',0, 'p',0, 'a',0, 'd',0
};

/* HID 报告描述符 - XInput 手柄 */
const uint8_t XInputRepDesc[] =
{
    0x05, 0x01,                 // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                 // USAGE (Game Pad)
    0xA1, 0x01,                 // COLLECTION (Application)
    0x85, 0x01,                 // REPORT_ID (1)

    /* 按钮 1-8 */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x08,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08,
    0x81, 0x02,

    /* 按钮 9-16 (含 D-pad) */
    0x19, 0x09, 0x29, 0x10,
    0x75, 0x01, 0x95, 0x08,
    0x81, 0x02,

    /* 左扳机 */
    0x05, 0x02, 0x09, 0xC5,
    0x15, 0x00, 0x26, 0xFF, 0x00, 0x75, 0x08, 0x95, 0x01,
    0x81, 0x02,

    /* 右扳机 */
    0x09, 0xC4,
    0x75, 0x08, 0x95, 0x01,
    0x81, 0x02,

    /* 左摇杆 X */
    0x05, 0x01, 0x09, 0x30,
    0x16, 0x00, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x01,
    0x81, 0x02,

    /* 左摇杆 Y */
    0x09, 0x31,
    0x75, 0x10, 0x95, 0x01,
    0x81, 0x02,

    /* 右摇杆 X */
    0x09, 0x32,
    0x75, 0x10, 0x95, 0x01,
    0x81, 0x02,

    /* 右摇杆 Y */
    0x09, 0x35,
    0x75, 0x10, 0x95, 0x01,
    0x81, 0x02,

    /* 保留 6 字节 */
    0x15, 0x00, 0x25, 0x00, 0x75, 0x08, 0x95, 0x06,
    0x81, 0x03,

    0xC0                        // END_COLLECTION
};

/* ===================================================================
 *  USB 内部状态
 * =================================================================== */

uint8_t        DevConfig, Ready;
uint8_t        SetupReqCode;
uint16_t       SetupReqLen;
const uint8_t *pDescr;
uint8_t        Report_Value[USB_INTERFACE_MAX_INDEX + 1] = {0x00};
uint8_t        Idle_Value[USB_INTERFACE_MAX_INDEX + 1]  = {0x00};
uint8_t        USB_SleepStatus = 0x00;

/* 端点 RAM 分配 */
__attribute__((aligned(4))) uint8_t EP0_Databuf[64 + 64 + 64]; // ep0 + ep4_out + ep4_in
__attribute__((aligned(4))) uint8_t EP1_Databuf[64 + 64];      // ep1_out + ep1_in
__attribute__((aligned(4))) uint8_t EP2_Databuf[64 + 64];      // ep2_out + ep2_in
__attribute__((aligned(4))) uint8_t EP3_Databuf[64 + 64];      // ep3_out + ep3_in

/* ===================================================================
 *  USB 传输处理
 * =================================================================== */

void USB_DevTransProcess_Xinput(void)
{
    uint8_t len, chtype;
    uint8_t intflag, errflag = 0;

    intflag = R8_USB_INT_FG;
    if(intflag & RB_UIF_TRANSFER)
    {
        if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            switch(R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
            {
                /* ---- EP0 IN ---- */
                case UIS_TOKEN_IN:
                    switch(SetupReqCode)
                    {
                        case USB_GET_DESCRIPTOR:
                            len = SetupReqLen >= DevEP0SIZE ? DevEP0SIZE : SetupReqLen;
                            memcpy(pEP0_DataBuf, pDescr, len);
                            SetupReqLen -= len;
                            pDescr += len;
                            R8_UEP0_T_LEN = len;
                            R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                            break;
                        case USB_SET_ADDRESS:
                            R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | SetupReqLen;
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;
                        case USB_SET_FEATURE:
                            break;
                        default:
                            R8_UEP0_T_LEN = 0;
                            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                            break;
                    }
                    break;

                /* ---- EP0 OUT ---- */
                case UIS_TOKEN_OUT:
                    len = R8_USB_RX_LEN;
                    (void)len;
                    break;

                /* ---- EP1 OUT ---- */
                case UIS_TOKEN_OUT | 1:
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP1_OUT_Deal(len);
                    }
                    break;

                /* ---- EP1 IN ---- */
                case UIS_TOKEN_IN | 1:
                    R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                /* ---- EP2 OUT ---- */
                case UIS_TOKEN_OUT | 2:
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP2_OUT_Deal(len);
                    }
                    break;

                /* ---- EP2 IN ---- */
                case UIS_TOKEN_IN | 2:
                    R8_UEP2_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                /* ---- EP3 OUT ---- */
                case UIS_TOKEN_OUT | 3:
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        R8_UEP3_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP3_OUT_Deal(len);
                    }
                    break;

                /* ---- EP3 IN ---- */
                case UIS_TOKEN_IN | 3:
                    R8_UEP3_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                /* ---- EP4 OUT ---- */
                case UIS_TOKEN_OUT | 4:
                    if(R8_USB_INT_ST & RB_UIS_TOG_OK)
                    {
                        R8_UEP4_CTRL ^= RB_UEP_R_TOG;
                        len = R8_USB_RX_LEN;
                        DevEP4_OUT_Deal(len);
                    }
                    break;

                /* ---- EP4 IN ---- */
                case UIS_TOKEN_IN | 4:
                    R8_UEP4_CTRL ^= RB_UEP_T_TOG;
                    R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    break;

                default:
                    break;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        /* ---- Setup 包处理 ---- */
        if(R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            SetupReqLen  = pSetupReqPak->wLength;
            SetupReqCode = pSetupReqPak->bRequest;
            chtype       = pSetupReqPak->bRequestType;
            len = 0;
            errflag = 0;

            if((pSetupReqPak->bRequestType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD)
            {
                /* 非标准请求 (类/厂商) */
                if(pSetupReqPak->bRequestType & 0x40)
                {
                    /* 厂商请求 */
                }
                else if(pSetupReqPak->bRequestType & 0x20)
                {
                    /* 类请求 */
                    switch(SetupReqCode)
                    {
                        case DEF_USB_SET_IDLE:
                            Idle_Value[pSetupReqPak->wIndex] = (uint8_t)(pSetupReqPak->wValue >> 8);
                            break;
                        case DEF_USB_SET_REPORT:
                            break;
                        case DEF_USB_SET_PROTOCOL:
                            Report_Value[pSetupReqPak->wIndex] = (uint8_t)(pSetupReqPak->wValue);
                            break;
                        case DEF_USB_GET_IDLE:
                            EP0_Databuf[0] = Idle_Value[pSetupReqPak->wIndex];
                            len = 1;
                            break;
                        case DEF_USB_GET_PROTOCOL:
                            EP0_Databuf[0] = Report_Value[pSetupReqPak->wIndex];
                            len = 1;
                            break;
                        default:
                            errflag = 0xFF;
                    }
                }
            }
            else
            {
                /* 标准请求 */
                switch(SetupReqCode)
                {
                    case USB_GET_DESCRIPTOR:
                        switch(((pSetupReqPak->wValue) >> 8))
                        {
                            case USB_DESCR_TYP_DEVICE:
                                pDescr = MyDevDescr;
                                len    = MyDevDescr[0];
                                break;
                            case USB_DESCR_TYP_CONFIG:
                                pDescr = MyCfgDescr;
                                len    = MyCfgDescr[2];
                                break;
                            case USB_DESCR_TYP_HID:
                                switch((pSetupReqPak->wIndex) & 0xff)
                                {
                                    case 0:
                                        pDescr = (uint8_t *)(&MyCfgDescr[18]);
                                        len = 9;
                                        break;
                                    default:
                                        errflag = 0xFF;
                                        break;
                                }
                                break;
                            case USB_DESCR_TYP_REPORT:
                                if(((pSetupReqPak->wIndex) & 0xff) == 0)
                                {
                                    pDescr = XInputRepDesc;
                                    len    = sizeof(XInputRepDesc);
                                    Ready  = 1;
                                }
                                else
                                    len = 0xFF;
                                break;
                            case USB_DESCR_TYP_STRING:
                                switch((pSetupReqPak->wValue) & 0xff)
                                {
                                    case 1: pDescr = MyManuInfo; len = MyManuInfo[0]; break;
                                    case 2: pDescr = MyProdInfo; len = MyProdInfo[0]; break;
                                    case 0: pDescr = MyLangDescr; len = MyLangDescr[0]; break;
                                    default: errflag = 0xFF; break;
                                }
                                break;
                            case 0x06:
                                pDescr = (uint8_t *)(&My_QueDescr[0]);
                                len    = sizeof(My_QueDescr);
                                break;
                            case 0x07:
                                memcpy(&USB_FS_OSC_DESC[2], &MyCfgDescr[2], sizeof(MyCfgDescr) - 2);
                                pDescr = (uint8_t *)(&USB_FS_OSC_DESC[0]);
                                len    = sizeof(USB_FS_OSC_DESC);
                                break;
                            default:
                                errflag = 0xFF;
                                break;
                        }
                        if(SetupReqLen > len) SetupReqLen = len;
                        len = (SetupReqLen >= DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                        memcpy(pEP0_DataBuf, pDescr, len);
                        pDescr += len;
                        break;

                    case USB_SET_ADDRESS:
                        SetupReqLen = (pSetupReqPak->wValue) & 0xff;
                        break;

                    case USB_GET_CONFIGURATION:
                        pEP0_DataBuf[0] = DevConfig;
                        if(SetupReqLen > 1) SetupReqLen = 1;
                        break;

                    case USB_SET_CONFIGURATION:
                        DevConfig = (pSetupReqPak->wValue) & 0xff;
                        break;

                    case USB_CLEAR_FEATURE:
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            switch((pSetupReqPak->wIndex) & 0xff)
                            {
                                case 0x83:
                                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x03:
                                    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                case 0x82:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x02:
                                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                case 0x81:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_NAK;
                                    break;
                                case 0x01:
                                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_ACK;
                                    break;
                                default:
                                    errflag = 0xFF;
                                    break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            if(pSetupReqPak->wValue == 1) USB_SleepStatus &= ~0x01;
                        }
                        else errflag = 0xFF;
                        break;

                    case USB_SET_FEATURE:
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            switch(pSetupReqPak->wIndex)
                            {
                                case 0x83: R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL; break;
                                case 0x03: R8_UEP3_CTRL = (R8_UEP3_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL; break;
                                case 0x82: R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL; break;
                                case 0x02: R8_UEP2_CTRL = (R8_UEP2_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL; break;
                                case 0x81: R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_T_TOG | MASK_UEP_T_RES)) | UEP_T_RES_STALL; break;
                                case 0x01: R8_UEP1_CTRL = (R8_UEP1_CTRL & ~(RB_UEP_R_TOG | MASK_UEP_R_RES)) | UEP_R_RES_STALL; break;
                                default: errflag = 0xFF; break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            if(pSetupReqPak->wValue == 1) USB_SleepStatus |= 0x01;
                        }
                        else errflag = 0xFF;
                        break;

                    case USB_GET_INTERFACE:
                        pEP0_DataBuf[0] = 0x00;
                        if(SetupReqLen > 1) SetupReqLen = 1;
                        break;

                    case USB_SET_INTERFACE:
                        break;

                    case USB_GET_STATUS:
                        if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_ENDP)
                        {
                            pEP0_DataBuf[0] = 0x00;
                            switch(pSetupReqPak->wIndex)
                            {
                                case 0x83:
                                    if((R8_UEP3_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                                        pEP0_DataBuf[0] = 0x01;
                                    break;
                                case 0x03:
                                    if((R8_UEP3_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                                        pEP0_DataBuf[0] = 0x01;
                                    break;
                                case 0x82:
                                    if((R8_UEP2_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                                        pEP0_DataBuf[0] = 0x01;
                                    break;
                                case 0x02:
                                    if((R8_UEP2_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                                        pEP0_DataBuf[0] = 0x01;
                                    break;
                                case 0x81:
                                    if((R8_UEP1_CTRL & (RB_UEP_T_TOG | MASK_UEP_T_RES)) == UEP_T_RES_STALL)
                                        pEP0_DataBuf[0] = 0x01;
                                    break;
                                case 0x01:
                                    if((R8_UEP1_CTRL & (RB_UEP_R_TOG | MASK_UEP_R_RES)) == UEP_R_RES_STALL)
                                        pEP0_DataBuf[0] = 0x01;
                                    break;
                            }
                        }
                        else if((pSetupReqPak->bRequestType & USB_REQ_RECIP_MASK) == USB_REQ_RECIP_DEVICE)
                        {
                            pEP0_DataBuf[0] = USB_SleepStatus ? 0x02 : 0x00;
                        }
                        pEP0_DataBuf[1] = 0;
                        if(SetupReqLen >= 2) SetupReqLen = 2;
                        break;

                    default:
                        errflag = 0xFF;
                        break;
                }
            }

            if(errflag == 0xFF)
            {
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
            }
            else
            {
                if(chtype & 0x80)
                {
                    len = (SetupReqLen > DevEP0SIZE) ? DevEP0SIZE : SetupReqLen;
                    SetupReqLen -= len;
                }
                else
                    len = 0;
                R8_UEP0_T_LEN = len;
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }
    }
    else if(intflag & RB_UIF_BUS_RST)
    {
        R8_USB_DEV_AD = 0;
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    else if(intflag & RB_UIF_SUSPEND)
    {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
    else
    {
        R8_USB_INT_FG = intflag;
    }
}

/* ===================================================================
 *  USB IRQ 处理
 * =================================================================== */

__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void)
{
    USB_DevTransProcess_Xinput();
}

/* ===================================================================
 *  公共 API
 * =================================================================== */

void SendUSBXinputReport(ReportDataXinput const *report)
{
    memcpy(pEP1_IN_DataBuf, report, sizeof(ReportDataXinput));
    DevEP1_IN_Deal(sizeof(ReportDataXinput));
}

int InitUSBXinput(void)
{
    pEP0_RAM_Addr = EP0_Databuf;
    pEP1_RAM_Addr = EP1_Databuf;
    pEP2_RAM_Addr = EP2_Databuf;
    pEP3_RAM_Addr = EP3_Databuf;

    USB_DeviceInit();
    PFIC_EnableIRQ(USB_IRQn);
    return 0;
}

void AJ_BuildXInputReport(AJ_Direction dir, ReportDataXinput *out)
{
    memset(out, 0, sizeof(ReportDataXinput));
    out->rid   = 0x00;     /* XInput 消息类型: 0x00=游戏数据 */
    out->rsize = 0x14;     /* 总大小 20 字节 */

    /* 方向 → 左摇杆模拟值 (±32767, 对应 16 位满量程) */
    switch(dir)
    {
        case AJ_DIR_L:  out->l_x = -32767;                       break;
        case AJ_DIR_U:  out->l_y =  32767;                       break;
        case AJ_DIR_R:  out->l_x =  32767;                       break;
        case AJ_DIR_D:  out->l_y = -32767;                       break;
        case AJ_DIR_LU: out->l_x = -32767; out->l_y =  32767;    break;
        case AJ_DIR_RU: out->l_x =  32767; out->l_y =  32767;    break;
        case AJ_DIR_LD: out->l_x = -32767; out->l_y = -32767;    break;
        case AJ_DIR_RD: out->l_x =  32767; out->l_y = -32767;    break;
        default:                                                 break;
    }
}

/* ===================================================================
 *  端点 OUT 处理 (占位)
 * =================================================================== */

void DevEP1_OUT_Deal(uint8_t l)
{
    if(l > 0) DevEP1_IN_Deal(0);  /* 发送空包确认 */
}

void DevEP2_OUT_Deal(uint8_t l)
{
    (void)l;
}

void DevEP3_OUT_Deal(uint8_t l)
{
    uint8_t i;
    for(i = 0; i < l; i++)
        pEP3_IN_DataBuf[i] = ~pEP3_OUT_DataBuf[i];
    DevEP3_IN_Deal(l);
}

void DevEP4_OUT_Deal(uint8_t l)
{
    uint8_t i;
    for(i = 0; i < l; i++)
        pEP4_IN_DataBuf[i] = ~pEP4_OUT_DataBuf[i];
    DevEP4_IN_Deal(l);
}
