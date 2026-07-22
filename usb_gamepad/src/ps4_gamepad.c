/*******************************************************************************
 * ps4_gamepad.c - PS4 Gamepad Driver for CH582M
 * 
 * Implements a PS4-compatible USB HID gamepad on the CH582M microcontroller.
 * 
 * USB Endpoint mapping:
 *   EP0: Control transfers (descriptors, feature reports)
 *   EP1 IN: HID Input Report (gamepad state → host, 64 bytes)
 *   EP2 OUT: HID Output Report (rumble/LED ← host, 32 bytes)
 * 
 * CH582M USB RAM layout:
 *   pEP0_RAM_Addr: 192 bytes (EP0 64B + EP4 OUT 64B + EP4 IN 64B)
 *   pEP1_RAM_Addr: 128 bytes (EP1 OUT 64B + EP1 IN 64B)
 *   pEP2_RAM_Addr: 128 bytes (EP2 OUT 64B + EP2 IN 64B)
 ******************************************************************************/

#include "ps4_gamepad.h"
#include <string.h>

/*******************************************************************************
 * USB Descriptors (Static Arrays)
 ******************************************************************************/

/* ---- Device Descriptor ---- */
static const uint8_t ps4_device_desc[] = {
    18,                             /* bLength */
    1,                              /* bDescriptorType (DEVICE) */
    0x00, 0x02,                     /* bcdUSB 2.0 */
    0,                              /* bDeviceClass (defined in interface) */
    0,                              /* bDeviceSubClass */
    0,                              /* bDeviceProtocol */
    PS4_ENDPOINT0_SIZE,             /* bMaxPacketSize0 */
    (uint8_t)(PS4_VENDOR_ID & 0xFF),       /* idVendor LSB */
    (uint8_t)(PS4_VENDOR_ID >> 8),         /* idVendor MSB */
    (uint8_t)(PS4_PRODUCT_ID & 0xFF),      /* idProduct LSB */
    (uint8_t)(PS4_PRODUCT_ID >> 8),        /* idProduct MSB */
    0x00, 0x01,                     /* bcdDevice 1.00 */
    1,                              /* iManufacturer (string index 1) */
    2,                              /* iProduct (string index 2) */
    0,                              /* iSerialNumber (none) */
    1                               /* bNumConfigurations */
};

/* ---- Configuration Descriptor ---- */
static const uint8_t ps4_config_desc[] = {
    /* Configuration Descriptor */
    9,                              /* bLength */
    2,                              /* bDescriptorType (CONFIGURATION) */
    (uint8_t)(PS4_CONFIG_DESC_SIZE & 0xFF),  /* wTotalLength LSB */
    (uint8_t)(PS4_CONFIG_DESC_SIZE >> 8),    /* wTotalLength MSB */
    1,                              /* bNumInterfaces */
    1,                              /* bConfigurationValue */
    0,                              /* iConfiguration */
    0x80,                           /* bmAttributes (Bus Powered) */
    50,                             /* bMaxPower (100mA) */
    
    /* Interface Descriptor */
    9,                              /* bLength */
    4,                              /* bDescriptorType (INTERFACE) */
    0,                              /* bInterfaceNumber */
    0,                              /* bAlternateSetting */
    2,                              /* bNumEndpoints */
    0x03,                           /* bInterfaceClass (HID) */
    0x00,                           /* bInterfaceSubClass (No Boot) */
    0x00,                           /* bInterfaceProtocol */
    0,                              /* iInterface */
    
    /* HID Descriptor */
    9,                              /* bLength */
    0x21,                           /* bDescriptorType (HID) */
    0x11, 0x01,                     /* bcdHID 1.11 */
    0,                              /* bCountryCode */
    1,                              /* bNumDescriptors */
    0x22,                           /* bDescriptorType (Report) */
    /* wDescriptorLength filled dynamically */
    0x00, 0x00,
    
    /* Endpoint Descriptor (IN) */
    7,                              /* bLength */
    5,                              /* bDescriptorType (ENDPOINT) */
    0x81,                           /* bEndpointAddress (EP1 IN) */
    0x03,                           /* bmAttributes (Interrupt) */
    PS4_ENDPOINT_SIZE, 0x00,        /* wMaxPacketSize (64) */
    1,                              /* bInterval (1ms) */
    
    /* Endpoint Descriptor (OUT) */
    7,                              /* bLength */
    5,                              /* bDescriptorType (ENDPOINT) */
    0x01,                           /* bEndpointAddress (EP1 OUT) */
    0x03,                           /* bmAttributes (Interrupt) */
    PS4_ENDPOINT_SIZE, 0x00,        /* wMaxPacketSize (64) */
    1                               /* bInterval (1ms) */
};

/* ---- HID Report Descriptor ---- */
static const uint8_t ps4_report_desc[] = {
    0x05, 0x01,        /* Usage Page (Generic Desktop Ctrls) */
    0x09, 0x05,        /* Usage (Game Pad) */
    0xA1, 0x01,        /* Collection (Application) */
    
    /* Report ID 1 - Input Report */
    0x85, 0x01,        /*   Report ID (1) */
    0x09, 0x30,        /*   Usage (X) */
    0x09, 0x31,        /*   Usage (Y) */
    0x09, 0x32,        /*   Usage (Z) */
    0x09, 0x35,        /*   Usage (Rz) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255) */
    0x75, 0x08,        /*   Report Size (8) */
    0x95, 0x04,        /*   Report Count (4) */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    
    0x09, 0x39,        /*   Usage (Hat switch) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x25, 0x07,        /*   Logical Maximum (7) */
    0x35, 0x00,        /*   Physical Minimum (0) */
    0x46, 0x3B, 0x01,  /*   Physical Maximum (315) */
    0x65, 0x14,        /*   Unit (English Rotation, cm) */
    0x75, 0x04,        /*   Report Size (4) */
    0x95, 0x01,        /*   Report Count (1) */
    0x81, 0x42,        /*   Input (Data,Var,Abs,Null State) */
    
    0x65, 0x00,        /*   Unit (None) */
    0x05, 0x09,        /*   Usage Page (Button) */
    0x19, 0x01,        /*   Usage Minimum (1) */
    0x29, 0x0E,        /*   Usage Maximum (14) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x25, 0x01,        /*   Logical Maximum (1) */
    0x75, 0x01,        /*   Report Size (1) */
    0x95, 0x0E,        /*   Report Count (14) */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    
    0x06, 0x00, 0xFF,  /*   Usage Page (Vendor 0xFF00) */
    0x09, 0x20,        /*   Usage (0x20) */
    0x75, 0x06,        /*   Report Size (6) */
    0x95, 0x01,        /*   Report Count (1) */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    
    0x05, 0x01,        /*   Usage Page (Generic Desktop) */
    0x09, 0x33,        /*   Usage (Rx) */
    0x09, 0x34,        /*   Usage (Ry) */
    0x15, 0x00,        /*   Logical Minimum (0) */
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255) */
    0x75, 0x08,        /*   Report Size (8) */
    0x95, 0x02,        /*   Report Count (2) */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    
    0x06, 0x00, 0xFF,  /*   Usage Page (Vendor 0xFF00) */
    0x09, 0x21,        /*   Usage (0x21) */
    0x95, 0x36,        /*   Report Count (54) */
    0x81, 0x02,        /*   Input (Data,Var,Abs) */
    
    /* Report ID 5 - Output Report (rumble/LED) */
    0x85, 0x05,        /*   Report ID (5) */
    0x09, 0x22,        /*   Usage (0x22) */
    0x95, 0x1F,        /*   Report Count (31) */
    0x91, 0x02,        /*   Output (Data,Var,Abs) */
    
    /* Report ID 3 - Feature Report (controller definition) */
    0x85, 0x03,        /*   Report ID (3) */
    0x0A, 0x21, 0x27,  /*   Usage (0x2721) */
    0x95, 0x2F,        /*   Report Count (47) */
    0xB1, 0x02,        /*   Feature (Data,Var,Abs) */
    
    /* Report ID 2 - Feature Report (calibration) */
    0x85, 0x02,        /*   Report ID (2) */
    0x09, 0x24,        /*   Usage (0x24) */
    0x95, 0x24,        /*   Report Count (36) */
    0xB1, 0x02,        /*   Feature (Data,Var,Abs) */
    
    /* Additional feature reports */
    0x85, 0x08, 0x09, 0x25, 0x95, 0x03, 0xB1, 0x02,
    0x85, 0x10, 0x09, 0x26, 0x95, 0x04, 0xB1, 0x02,
    0x85, 0x11, 0x09, 0x27, 0x95, 0x02, 0xB1, 0x02,
    0x85, 0x12, 0x06, 0x02, 0xFF, 0x09, 0x21, 0x95, 0x0F, 0xB1, 0x02,
    0x85, 0x13, 0x09, 0x22, 0x95, 0x16, 0xB1, 0x02,
    0x85, 0x14, 0x06, 0x05, 0xFF, 0x09, 0x20, 0x95, 0x10, 0xB1, 0x02,
    0x85, 0x15, 0x09, 0x21, 0x95, 0x2C, 0xB1, 0x02,
    0x06, 0x80, 0xFF,
    0x85, 0x80, 0x09, 0x20, 0x95, 0x06, 0xB1, 0x02,
    0x85, 0x81, 0x09, 0x21, 0x95, 0x06, 0xB1, 0x02,
    0x85, 0x82, 0x09, 0x22, 0x95, 0x05, 0xB1, 0x02,
    0x85, 0x83, 0x09, 0x23, 0x95, 0x01, 0xB1, 0x02,
    0x85, 0x84, 0x09, 0x24, 0x95, 0x04, 0xB1, 0x02,
    0x85, 0x85, 0x09, 0x25, 0x95, 0x06, 0xB1, 0x02,
    0x85, 0x86, 0x09, 0x26, 0x95, 0x06, 0xB1, 0x02,
    0x85, 0x87, 0x09, 0x27, 0x95, 0x23, 0xB1, 0x02,
    0x85, 0x88, 0x09, 0x28, 0x95, 0x22, 0xB1, 0x02,
    0x85, 0x89, 0x09, 0x29, 0x95, 0x02, 0xB1, 0x02,
    0x85, 0x90, 0x09, 0x30, 0x95, 0x05, 0xB1, 0x02,
    0xC0,              /* End Collection */
    
    /* Authentication Feature Reports (Usage Page 0xFFF0) */
    0x06, 0xF0, 0xFF,  /*   Usage Page (Vendor 0xFFF0) */
    0x09, 0x40,        /*   Usage (0x40) */
    0xA1, 0x01,        /*   Collection (Application) */
    0x85, 0xF0,        /*     Report ID (0xF0) Auth Payload */
    0x09, 0x47,        /*     Usage (0x47) */
    0x95, 0x3F,        /*     Report Count (63) */
    0xB1, 0x02,        /*     Feature (Data,Var,Abs) */
    0x85, 0xF1,        /*     Report ID (0xF1) Signature Nonce */
    0x09, 0x48,        /*     Usage (0x48) */
    0x95, 0x3F,        /*     Report Count (63) */
    0xB1, 0x02,        /*     Feature (Data,Var,Abs) */
    0x85, 0xF2,        /*     Report ID (0xF2) Signing State */
    0x09, 0x49,        /*     Usage (0x49) */
    0x95, 0x0F,        /*     Report Count (15) */
    0xB1, 0x02,        /*     Feature (Data,Var,Abs) */
    0x85, 0xF3,        /*     Report ID (0xF3) Reset Auth */
    0x0A, 0x01, 0x47,  /*     Usage (0x4701) */
    0x95, 0x07,        /*     Report Count (7) */
    0xB1, 0x02,        /*     Feature (Data,Var,Abs) */
    0xC0,              /*   End Collection */
};

/* ---- String Descriptors ---- */
static const uint8_t ps4_string_lang[]     = { 0x04, 0x03, 0x09, 0x04 };  /* EN-US */
static const uint8_t ps4_string_manuf[]    = "WCH Gamepad";
static const uint8_t ps4_string_product[]  = "PS4 Compatible Controller";

/*******************************************************************************
 * Fixed Response Data (from GP2040-CE)
 ******************************************************************************/

/* Calibration data for report 0x02 (36 bytes) */
static const uint8_t ps4_calibration[] = {
    0xFE, 0xFF, 0x0E, 0x00, 0x04, 0x00, 0xD4, 0x22,
    0x2A, 0xDD, 0xBB, 0x22, 0x5E, 0xDD, 0x81, 0x22,
    0x84, 0xDD, 0x1C, 0x02, 0x1C, 0x02, 0x85, 0x1F,
    0xB0, 0xE0, 0xC6, 0x20, 0xB5, 0xE0, 0xB1, 0x20,
    0x83, 0xDF, 0x0C, 0x00
};

/* MAC address report (15 bytes) */
static const uint8_t ps4_mac_addr[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* Device MAC */
    0x08, 0x25, 0x00,                         /* BT device class */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* Host MAC */
};

/* Firmware version (48 bytes) - "Jun  9 2017 12:36:41" */
static const uint8_t ps4_version[] = {
    0x4A, 0x75, 0x6E, 0x20, 0x20, 0x39, 0x20, 0x32,
    0x30, 0x31, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x31, 0x32, 0x3A, 0x33, 0x36, 0x3A, 0x34, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x08, 0xB4, 0x01, 0x00, 0x00, 0x00,
    0x07, 0xA0, 0x10, 0x20, 0x00, 0xA0, 0x02, 0x00
};

/* Auth reset data (7 bytes) - nonce page size info */
static const uint8_t ps4_auth_reset[] = {
    0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00
};

/*******************************************************************************
 * USB RAM Buffers (placed in USB-dedicated RAM by linker)
 ******************************************************************************/

/* USB DMA buffers - must be accessible by USB peripheral */
__attribute__((aligned(4))) uint8_t EP0_DMABUF[192];  /* EP0(64) + EP4OUT(64) + EP4IN(64) */
__attribute__((aligned(4))) uint8_t EP1_DMABUF[128];  /* EP1OUT(64) + EP1IN(64) */
__attribute__((aligned(4))) uint8_t EP2_DMABUF[128];  /* EP2OUT(64) + EP2IN(64) */
__attribute__((aligned(4))) uint8_t EP3_DMABUF[128];  /* EP3OUT(64) + EP3IN(64) */

/*******************************************************************************
 * Global Variables
 ******************************************************************************/

PS4_InputReport       g_ps4Report;
PS4_OutputReport      g_ps4Features;
PS4_ControllerConfig  g_ps4Config;
PS4_AuthData          g_ps4Auth;

/*******************************************************************************
 * USB State Tracking
 ******************************************************************************/

/* WCH-pattern USB state variables (used by USB_DevTransProcess) */
static uint8_t  s_usbConfigured    = 0;
static uint8_t  s_reportCounter    = 0;
static uint8_t  DevConfig       = 0;
static uint8_t  Ready           = 0;
static uint8_t  SetupReqCode;
static uint16_t SetupReqLen;
static const uint8_t *pDescr;
static uint8_t  USB_SleepStatus = 0;

/* Saved SET_REPORT parameters for OUT data phase */
static uint8_t  s_pendingReportType = 0;
static uint8_t  s_pendingReportId   = 0;

/* Controller type */
static PS4ControllerType s_ctrlType = PS4_CT_ARCADESTICK;

/*******************************************************************************
 * USB Standard Request Constants (not in CH58x headers)
 ******************************************************************************/
#define USB_GET_DESCRIPTOR    0x06
#define USB_SET_ADDRESS       0x05
#define USB_SET_CONFIGURATION 0x09
#define USB_GET_CONFIGURATION 0x08
#define USB_SET_FEATURE       0x03
#define USB_CLEAR_FEATURE     0x01
#define USB_GET_STATUS        0x00
#define USB_GET_INTERFACE     0x0A
#define USB_SET_INTERFACE     0x0B
#define USB_DESCR_TYP_HID      0x21
#define USB_DESCR_TYP_REPORT   0x22
#define USB_REQ_TYP_MASK       0x60
#define USB_REQ_TYP_STANDARD   0x00
#define USB_REQ_RECIP_MASK     0x1F
#define USB_REQ_RECIP_DEVICE   0x00
#define USB_REQ_RECIP_ENDP     0x02
#define DEF_USB_GET_REPORT     0x01  /* missing from CH58x_usbdev.h */

/*******************************************************************************
 * CRC32 Calculation (for auth packets)
 ******************************************************************************/

static uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/*******************************************************************************
 * ProcessSetReportData — Handle SET_REPORT data from EP0 OUT phase
 ******************************************************************************/
static void ProcessSetReportData(const uint8_t *buf, uint16_t len)
{
    if (s_pendingReportType == 2) {
        if (s_pendingReportId == 0x05)
            memcpy(&g_ps4Features, buf, len < sizeof(g_ps4Features) ? len : sizeof(g_ps4Features));
    } else if (s_pendingReportType == 3) {
        if (s_pendingReportId == PS4_AUTH_SET_AUTH_PAYLOAD) {
            uint8_t nid = buf[0], npg = buf[1];
            if (npg < 4)
                memcpy(&g_ps4Auth.auth_buffer[npg*56], &buf[3], 56);
            else if (npg == 4) {
                memcpy(&g_ps4Auth.auth_buffer[npg*56], &buf[3], 32);
                g_ps4Auth.nonce_id = nid; g_ps4Auth.auth_state = 1;
            }
        }
    }
}

/*******************************************************************************
 * PrepareGetReport — Build feature report response in pEP0_DataBuf
 ******************************************************************************/
static uint8_t PrepareGetReport(uint8_t reportType, uint8_t reportId)
{
    if (reportType == 1) {
        memcpy(pEP0_DataBuf, &g_ps4Report, sizeof(g_ps4Report));
        return sizeof(g_ps4Report);
    }
    if (reportType != 3) return 0;

    switch (reportId) {
    case PS4_AUTH_GET_CALIBRATION:
        memcpy(pEP0_DataBuf, ps4_calibration, sizeof(ps4_calibration));
        return sizeof(ps4_calibration);
    case PS4_AUTH_DEFINITION:
        memcpy(pEP0_DataBuf, &g_ps4Config, sizeof(g_ps4Config));
        return sizeof(g_ps4Config);
    case PS4_AUTH_GET_MAC_ADDRESS:
        memcpy(pEP0_DataBuf, ps4_mac_addr, sizeof(ps4_mac_addr));
        return sizeof(ps4_mac_addr);
    case PS4_AUTH_GET_VERSION_DATE:
        memcpy(pEP0_DataBuf, ps4_version, sizeof(ps4_version));
        return sizeof(ps4_version);
    case PS4_AUTH_GET_SIGNATURE_NONCE: {
        uint8_t d[64];
        d[0]=0xF1; d[1]=g_ps4Auth.nonce_id; d[2]=0; d[3]=0;
        memcpy(&d[4], g_ps4Auth.auth_buffer, 56);
        uint32_t crc = CRC32_Calculate(d, 60);
        memcpy(&d[60], &crc, 4);
        memcpy(pEP0_DataBuf, &d[1], 63);
        return 63;
    }
    case PS4_AUTH_GET_SIGNING_STATE: {
        uint8_t d[16];
        d[0]=0xF2; d[1]=g_ps4Auth.nonce_id; d[2]=16;
        memset(&d[3],0,9);
        uint32_t crc = CRC32_Calculate(d,12);
        memcpy(&d[12],&crc,4);
        memcpy(pEP0_DataBuf,&d[1],15);
        return 15;
    }
    case PS4_AUTH_RESET_AUTH:
        memcpy(pEP0_DataBuf, ps4_auth_reset, sizeof(ps4_auth_reset));
        g_ps4Auth.auth_state = 0;
        return sizeof(ps4_auth_reset);
    default: return 0;
    }
}

/*******************************************************************************
 * USB_DevTransProcess — Core USB handler (WCH official pattern)
 ******************************************************************************/

#define DevEP0SIZE  0x40

void USB_DevTransProcess(void)
{
    uint16_t len;       /* MUST be uint16_t — report descriptor is 321 bytes */
    uint8_t chtype;
    uint8_t intflag, errflag = 0;

    intflag = R8_USB_INT_FG;

    if (intflag & RB_UIF_TRANSFER)
    {
        if ((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN)
        {
            switch (R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP))
            {
            case UIS_TOKEN_IN:  /* EP0 IN */
                switch (SetupReqCode) {
                case USB_GET_DESCRIPTOR:
                    len = SetupReqLen >= DevEP0SIZE ? DevEP0SIZE : SetupReqLen;
                    memcpy(pEP0_DataBuf, pDescr, len);
                    SetupReqLen -= len; pDescr += len;
                    R8_UEP0_T_LEN = (uint8_t)len;
                    R8_UEP0_CTRL ^= RB_UEP_T_TOG;
                    break;
                case USB_SET_ADDRESS:
                    R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | SetupReqLen;
                    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    break;
                default:
                    R8_UEP0_T_LEN = 0;
                    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                    Ready = 1;
                    break;
                }
                break;

            case UIS_TOKEN_OUT:  /* EP0 OUT (SET_REPORT data) */
                len = R8_USB_RX_LEN;
                if (SetupReqCode == DEF_USB_SET_REPORT) {
                    ProcessSetReportData(pEP0_DataBuf, len);
                }
                R8_UEP0_T_LEN = 0;
                R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                break;

            case UIS_TOKEN_OUT | 1:  /* EP1 OUT (output report) */
                if (R8_USB_INT_ST & RB_UIS_TOG_OK) {
                    R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                    len = R8_USB_RX_LEN;
                    if (len >= sizeof(PS4_OutputReport))
                        memcpy(&g_ps4Features, pEP1_OUT_DataBuf, sizeof(PS4_OutputReport));
                }
                break;

            case UIS_TOKEN_IN | 1:  /* EP1 IN done — ready for next */
                R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                Ready = 1;
                break;
            }
            R8_USB_INT_FG = RB_UIF_TRANSFER;
        }

        if (R8_USB_INT_ST & RB_UIS_SETUP_ACT)
        {
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
            SetupReqLen  = pSetupReqPak->wLength;
            SetupReqCode = pSetupReqPak->bRequest;
            chtype       = pSetupReqPak->bRequestType;
            len = 0; errflag = 0;

            if ((pSetupReqPak->bRequestType & USB_REQ_TYP_MASK) != USB_REQ_TYP_STANDARD) {
                if (pSetupReqPak->bRequestType & 0x20) {
                    switch (SetupReqCode) {
                    case DEF_USB_GET_IDLE:    pEP0_DataBuf[0]=0; len=1; break;
                    case DEF_USB_SET_IDLE:    break;
                    case DEF_USB_SET_REPORT:
                        s_pendingReportType = (uint8_t)(pSetupReqPak->wValue>>8);
                        s_pendingReportId   = (uint8_t)(pSetupReqPak->wValue);
                        break;
                    case DEF_USB_SET_PROTOCOL: break;
                    case DEF_USB_GET_PROTOCOL: pEP0_DataBuf[0]=0; len=1; break;
                    case DEF_USB_GET_REPORT:
                        len = PrepareGetReport((uint8_t)(pSetupReqPak->wValue>>8),
                                               (uint8_t)(pSetupReqPak->wValue));
                        break;
                    default: errflag=0xFF; break;
                    }
                }
            } else {
                switch (SetupReqCode) {
                case USB_GET_DESCRIPTOR:
                    switch ((pSetupReqPak->wValue)>>8) {
                    case USB_DESCR_TYP_DEVICE: pDescr=ps4_device_desc; len=ps4_device_desc[0]; break;
                    case USB_DESCR_TYP_CONFIG: {
                        static uint8_t c[sizeof(ps4_config_desc)];
                        memcpy(c,ps4_config_desc,sizeof(ps4_config_desc));
                        uint16_t rl=sizeof(ps4_report_desc);
                        c[25]=(uint8_t)rl; c[26]=(uint8_t)(rl>>8);
                        pDescr=c; len=c[2];
                    } break;
                    case USB_DESCR_TYP_HID: pDescr=(uint8_t*)(&ps4_config_desc[18]); len=9; break;
                    case USB_DESCR_TYP_REPORT: pDescr=ps4_report_desc; len=sizeof(ps4_report_desc); break;
                    case USB_DESCR_TYP_STRING:
                        switch((pSetupReqPak->wValue)&0xFF) {
                        case 0: pDescr=ps4_string_lang; len=ps4_string_lang[0]; break;
                        case 1: {
                            static uint8_t b[64]; uint8_t sl=(uint8_t)strlen((const char*)ps4_string_manuf);
                            b[0]=sl*2+2; b[1]=3;
                            for(uint8_t i=0;i<sl;i++){b[2+i*2]=ps4_string_manuf[i];b[3+i*2]=0;}
                            pDescr=b; len=b[0];
                        } break;
                        case 2: {
                            static uint8_t b[64]; uint8_t sl=(uint8_t)strlen((const char*)ps4_string_product);
                            b[0]=sl*2+2; b[1]=3;
                            for(uint8_t i=0;i<sl;i++){b[2+i*2]=ps4_string_product[i];b[3+i*2]=0;}
                            pDescr=b; len=b[0];
                        } break;
                        default: errflag=0xFF; break;
                        }
                        break;
                    default: errflag=0xFF; break;
                    }
                    if(SetupReqLen>len)SetupReqLen=len;
                    len=(SetupReqLen>=DevEP0SIZE)?DevEP0SIZE:SetupReqLen;
                    memcpy(pEP0_DataBuf,pDescr,len); pDescr+=len;
                    break;
                case USB_SET_ADDRESS: SetupReqLen=(pSetupReqPak->wValue)&0xFF; break;
                case USB_GET_CONFIGURATION: pEP0_DataBuf[0]=DevConfig; if(SetupReqLen>1)SetupReqLen=1; break;
                case USB_SET_CONFIGURATION:
                    DevConfig=(pSetupReqPak->wValue)&0xFF; s_usbConfigured=(DevConfig!=0);
                    if(s_usbConfigured){
                        R8_UEP1_CTRL=UEP_R_RES_ACK|UEP_T_RES_NAK|RB_UEP_AUTO_TOG;
                        R8_UEP2_CTRL=UEP_R_RES_ACK|UEP_T_RES_NAK|RB_UEP_AUTO_TOG;
                    }
                    break;
                case USB_CLEAR_FEATURE: case USB_SET_FEATURE:
                    if((pSetupReqPak->bRequestType&USB_REQ_RECIP_MASK)==USB_REQ_RECIP_ENDP){
                        switch(pSetupReqPak->wIndex&0xFF){
                        case 0x81: R8_UEP1_CTRL=(R8_UEP1_CTRL&~(RB_UEP_T_TOG|MASK_UEP_T_RES))|UEP_T_RES_NAK; break;
                        case 0x01: R8_UEP1_CTRL=(R8_UEP1_CTRL&~(RB_UEP_R_TOG|MASK_UEP_R_RES))|UEP_R_RES_ACK; break;
                        default: errflag=0xFF; break;
                        }
                    }else if((pSetupReqPak->bRequestType&USB_REQ_RECIP_MASK)!=USB_REQ_RECIP_DEVICE)errflag=0xFF;
                    break;
                case USB_GET_STATUS:
                    pEP0_DataBuf[0]=(USB_SleepStatus)?0x02:0x00; pEP0_DataBuf[1]=0;
                    if(SetupReqLen>=2)SetupReqLen=2;
                    break;
                case USB_GET_INTERFACE: pEP0_DataBuf[0]=0; if(SetupReqLen>1)SetupReqLen=1; break;
                case USB_SET_INTERFACE: break;
                default: errflag=0xFF; break;
                }
            }

            if(errflag==0xFF){
                R8_UEP0_CTRL=RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_STALL|UEP_T_RES_STALL; Ready=1;
            }else{
                if(chtype&0x80){ len=(SetupReqLen>DevEP0SIZE)?DevEP0SIZE:SetupReqLen; SetupReqLen-=len; }
                else len=0;
                R8_UEP0_T_LEN=(uint8_t)len;
                R8_UEP0_CTRL=RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_ACK|UEP_T_RES_ACK;
            }
            R8_USB_INT_FG=RB_UIF_TRANSFER;
        }
    }
    else if(intflag&RB_UIF_BUS_RST){
        R8_USB_DEV_AD=0;
        R8_UEP0_CTRL=UEP_R_RES_ACK|UEP_T_RES_NAK;
        R8_UEP1_CTRL=UEP_R_RES_ACK|UEP_T_RES_NAK;
        R8_UEP2_CTRL=UEP_R_RES_ACK|UEP_T_RES_NAK;
        R8_UEP3_CTRL=UEP_R_RES_ACK|UEP_T_RES_NAK;
        s_usbConfigured=0;
        R8_USB_INT_FG=RB_UIF_BUS_RST;
    }
    else if(intflag&RB_UIF_SUSPEND){
        Ready=(R8_USB_MIS_ST&RB_UMS_SUSPEND)?0:1;
        R8_USB_INT_FG=RB_UIF_SUSPEND;
    }
    else R8_USB_INT_FG=intflag;
}

/*******************************************************************************
 * Public API Implementation
 ******************************************************************************/

void PS4_Init(PS4ControllerType ctrlType)
{
    s_ctrlType = ctrlType;
    
    /* Initialize input report with defaults */
    memset(&g_ps4Report, 0, sizeof(g_ps4Report));
    g_ps4Report.reportID       = 0x01;
    g_ps4Report.leftStickX     = PS4_JOYSTICK_MID;
    g_ps4Report.leftStickY     = PS4_JOYSTICK_MID;
    g_ps4Report.rightStickX    = PS4_JOYSTICK_MID;
    g_ps4Report.rightStickY    = PS4_JOYSTICK_MID;
    g_ps4Report.dpad           = PS4_HAT_NOTHING;
    
    /* Initialize misc data for gamepad mode */
    memset(g_ps4Report.misc.raw, 0, 54);
    g_ps4Report.misc.gamepad.sensorData.powerLevel = 0xB;
    g_ps4Report.misc.gamepad.sensorData.charging   = 1;
    
    /* Initialize feature output report */
    memset(&g_ps4Features, 0, sizeof(g_ps4Features));
    
    /* Initialize controller configuration */
    memset(&g_ps4Config, 0, sizeof(g_ps4Config));
    g_ps4Config.hidUsage  = 0x2721;
    g_ps4Config.mystery0  = 0x04;
    
    if (ctrlType == PS4_CT_ARCADESTICK) {
        /* Arcade stick / PS5 mode: minimal features */
        g_ps4Config.features.enableController = 1;
        g_ps4Config.features.enableMotion     = 0;
        g_ps4Config.features.enableLED        = 0;
        g_ps4Config.features.enableRumble     = 0;
        g_ps4Config.features.enableAnalog     = 0;
        g_ps4Config.features.enableUnknown0   = 1;
        g_ps4Config.features.enableTouchpad   = 0;
        g_ps4Config.features.enableUnknown1   = 1;
        g_ps4Config.controllerType = PS4_CT_ARCADESTICK;
    } else {
        /* Full DS4 gamepad mode */
        g_ps4Config.features.enableController = 1;
        g_ps4Config.features.enableMotion     = 1;
        g_ps4Config.features.enableLED        = 1;
        g_ps4Config.features.enableRumble     = 1;
        g_ps4Config.features.enableAnalog     = 0;
        g_ps4Config.features.enableUnknown0   = 1;
        g_ps4Config.features.enableTouchpad   = 1;
        g_ps4Config.features.enableUnknown1   = 1;
        g_ps4Config.controllerType = PS4_CT_CONTROLLER;
    }
    
    g_ps4Config.touchpadParam[0] = 0x2C;
    g_ps4Config.touchpadParam[1] = 0x56;
    g_ps4Config.imuConfig.gyroRange           = 0x0008;
    g_ps4Config.imuConfig.gyroResPerDegDenom  = 0x003D;
    g_ps4Config.imuConfig.gyroResPerDegNumer  = 0x03E8;
    g_ps4Config.imuConfig.accelRange          = 0x0004;
    g_ps4Config.imuConfig.accelResPerG        = 0x7FFF;
    g_ps4Config.magicID                       = 0x0D0D;
    g_ps4Config.wheelParam[0] = 0x0D;
    g_ps4Config.wheelParam[1] = 0x84;
    g_ps4Config.wheelParam[2] = 0x03;
    
    /* Initialize auth data */
    memset(&g_ps4Auth, 0, sizeof(g_ps4Auth));
    
    /* Initialize state */
    s_reportCounter = 0;
}

void PS4_SetButton(uint16_t mask, uint8_t pressed)
{
    if (pressed) {
        /* Map mask to the bit positions in PS4Report */
        /* Button bits: [14:0] in bits 4-17 of the 3-byte dpad+button field */
        /* The structure uses bit fields, so we manipulate via raw access */
        uint8_t *btnBase = (uint8_t *)&g_ps4Report + 5;
        /* Button bits start at bit 4 of byte 5 */
        uint16_t current = btnBase[0] | ((uint16_t)btnBase[1] << 8);
        
        /* Map PS4_MASK_* to bit positions */
        /* mask: bit 0=Square(W), 1=Cross(S), 2=Circle(E), 3=Triangle(N),
           bit 4=L1, 5=R1, 6=L2, 7=R2, 8=Select, 9=Start, 10=L3, 11=R3,
           12=Home, 13=Touchpad */
        /* These start at bit 4 of the combined 24-bit field */
        uint16_t shiftedMask = mask << 4;
        current |= shiftedMask;
        
        btnBase[0] = (uint8_t)(current & 0xFF);
        btnBase[1] = (uint8_t)((current >> 8) & 0xFF);
    } else {
        /* Clear bits: read-modify-write */
        uint8_t *btnBase = (uint8_t *)&g_ps4Report + 5;
        uint16_t current = btnBase[0] | ((uint16_t)btnBase[1] << 8);
        uint16_t shiftedMask = mask << 4;
        current &= ~shiftedMask;
        btnBase[0] = (uint8_t)(current & 0xFF);
        btnBase[1] = (uint8_t)((current >> 8) & 0xFF);
    }
}

void PS4_SetLeftStick(uint8_t x, uint8_t y)
{
    g_ps4Report.leftStickX = x;
    g_ps4Report.leftStickY = y;
}

void PS4_SetRightStick(uint8_t x, uint8_t y)
{
    g_ps4Report.rightStickX = x;
    g_ps4Report.rightStickY = y;
}

void PS4_SetDpad(uint8_t hat)
{
    g_ps4Report.dpad = hat & 0x0F;
}

void PS4_SetTriggers(uint8_t left, uint8_t right)
{
    g_ps4Report.leftTrigger  = left;
    g_ps4Report.rightTrigger = right;
}

uint8_t PS4_SendReport(void)
{
    if (!s_usbConfigured || !Ready) return 0;
    
    Ready = 0;
    s_reportCounter = (s_reportCounter + 1) & 0x3F;
    g_ps4Report.reportCounter = s_reportCounter;
    
    memcpy(pEP1_IN_DataBuf, &g_ps4Report, sizeof(g_ps4Report));
    DevEP1_IN_Deal(sizeof(g_ps4Report));
    return 1;
}

PS4_OutputReport* PS4_GetOutputReport(void)
{
    return &g_ps4Features;
}

void PS4_Process(void)
{
    /* Check if EP1 IN is ready and we have data to send */
    /* CH582M auto-NAK is enabled (RB_UC_INT_BUSY), so the USB
       peripheral will automatically NAK while we are not actively
       sending. We just need to call PS4_SendReport when ready. */
}

/*******************************************************************************
 * USB Device Initialization
 ******************************************************************************/

void PS4_USB_DeviceInit(void)
{
    /* Configure USB RAM buffer pointers for CH582M */
    pEP0_RAM_Addr = EP0_DMABUF;
    pEP1_RAM_Addr = EP1_DMABUF;
    pEP2_RAM_Addr = EP2_DMABUF;
    pEP3_RAM_Addr = EP3_DMABUF;
    
    /* Reset USB controller */
    R8_USB_CTRL = 0x00;
    
    /* Enable endpoints:
     * EP0: Control (always enabled)
     * EP1: IN + OUT (IN for HID reports, OUT for optional data)
     * EP2: IN + OUT (OUT for feature/output reports) 
     * EP3-4: Disabled
     */
    R8_UEP4_1_MOD = RB_UEP4_RX_EN | RB_UEP4_TX_EN | RB_UEP1_RX_EN | RB_UEP1_TX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN | RB_UEP3_RX_EN | RB_UEP3_TX_EN;
    
    /* Set DMA buffer addresses */
    R16_UEP0_DMA = (uint16_t)(uint32_t)pEP0_RAM_Addr;
    R16_UEP1_DMA = (uint16_t)(uint32_t)pEP1_RAM_Addr;
    R16_UEP2_DMA = (uint16_t)(uint32_t)pEP2_RAM_Addr;
    R16_UEP3_DMA = (uint16_t)(uint32_t)pEP3_RAM_Addr;
    
    /* Configure endpoint control registers:
     * EP0: ACK for SETUP, NAK for IN/OUT (manual control)
     * EP1-4: Auto-toggle, ACK for OUT, NAK for IN (until data ready)
     */
    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP4_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    
    /* Clear device address */
    R8_USB_DEV_AD = 0x00;
    
    /* Enable USB: Device mode, pull-up, interrupt on busy, DMA */
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    
    /* Enable USB IO pin pull-ups */
    R16_PIN_ANALOG_IE |= RB_PIN_USB_IE | RB_PIN_USB_DP_PU;
    
    /* Clear all interrupt flags */
    R8_USB_INT_FG = 0xFF;
    
    /* Enable USB port */
    R8_UDEV_CTRL = RB_UD_PD_DIS | RB_UD_PORT_EN;
    
    /* Enable USB interrupts: Suspend, Bus Reset, Transfer */
    R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;
}

/*******************************************************************************
 * USB Interrupt Handler
 * 
 * Handles USB events directly at the register level.
 * The CH582M USB library does NOT provide a setup/transaction processor —
 * we must handle all USB requests ourselves in this ISR.
 * 
 * This function is the single entry point for all USB interrupt handling.
 * It follows the official WCH USB Device example pattern.
 ******************************************************************************/

void PS4_USB_IRQHandler(void)
{
    USB_DevTransProcess();
}

/*******************************************************************************
 * USB Device Transaction Process (call in main loop)
 ******************************************************************************/

void PS4_USB_DevTransProcess(void)
{
    /* The CH582M handles most USB transactions in hardware/ISR.
     * This function is a placeholder for any background USB processing.
     * The actual data transfer happens in the ISR and in PS4_SendReport().
     */
}

/*******************************************************************************
 * USB EP2 OUT Data Processing
 * 
 * When the host sends an output report (rumble/LED), it arrives on EP2 OUT.
 * The data is captured in the ISR (UIS_TOKEN_OUT + endpoint 2).
 * 
 * This function can be called from the main loop to process it.
 ******************************************************************************/
void DevEP2_OUT_Deal(uint8_t l)
{
    /* The CH582M USB library calls endpoint-specific handlers.
     * For EP2 OUT, the data at pEP2_OUT_DataBuf is our output report. */
    if (l >= sizeof(PS4_OutputReport)) {
        memcpy(&g_ps4Features, pEP2_OUT_DataBuf, sizeof(PS4_OutputReport));
    }
}
