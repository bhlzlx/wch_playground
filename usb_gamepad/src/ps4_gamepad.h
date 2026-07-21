/*******************************************************************************
 * ps4_gamepad.h - PS4 Gamepad Driver for CH582M
 * 
 * Based on analysis of GP2040-CE PS4 driver implementation
 * https://github.com/OpenStickCommunity/GP2040-CE
 * 
 * This driver emulates a PS4-compatible HID gamepad via USB.
 * Supports: Gamepad mode, Arcade Stick mode (PS5 compatible)
 * 
 * CH582M USB Device endpoints used:
 *   EP0: Control (descriptors, feature reports)
 *   EP1 IN: HID Input Report (gamepad state, 64 bytes)
 *   EP2 OUT: HID Output Report (rumble/LED, 32 bytes)
 ******************************************************************************/

#ifndef __PS4_GAMEPAD_H__
#define __PS4_GAMEPAD_H__

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * USB Identification
 ******************************************************************************/

/* Razer Panthera (fighting stick - works well with PS4) */
#define PS4_VENDOR_ID           0x1532
#define PS4_PRODUCT_ID          0x0401

/* Original DS4 - requires full authentication */
#define DS4_VENDOR_ID           0x054C
#define DS4_PRODUCT_ID          0x09CC

/* G29 Racing Wheel */
#define PS4_WHEEL_VENDOR_ID     0x046D
#define PS4_WHEEL_PRODUCT_ID    0xC260

/*******************************************************************************
 * USB Descriptor Sizes
 ******************************************************************************/

#define PS4_ENDPOINT0_SIZE      64
#define PS4_ENDPOINT_SIZE       64
#define PS4_FEATURES_SIZE       32
#define PS4_CONFIG_DESC_SIZE    41

/*******************************************************************************
 * HID Report Constants
 ******************************************************************************/

/* D-Pad (Hat Switch) values */
#define PS4_HAT_UP              0x00
#define PS4_HAT_UPRIGHT         0x01
#define PS4_HAT_RIGHT           0x02
#define PS4_HAT_DOWNRIGHT       0x03
#define PS4_HAT_DOWN            0x04
#define PS4_HAT_DOWNLEFT        0x05
#define PS4_HAT_LEFT            0x06
#define PS4_HAT_UPLEFT          0x07
#define PS4_HAT_NOTHING         0x0F

/* Joystick values (8-bit) */
#define PS4_JOYSTICK_MIN        0x00
#define PS4_JOYSTICK_MID        0x80
#define PS4_JOYSTICK_MAX        0xFF

/* 16-bit analog values (for wheel/HOTAS) */
#define PS4_NAV_JOYSTICK_MIN    0x0000
#define PS4_NAV_JOYSTICK_MID    0x8000
#define PS4_NAV_JOYSTICK_MAX    0xFFFF

/* Touchpad resolution */
#define PS4_TP_X_MIN            0
#define PS4_TP_X_MAX            1920
#define PS4_TP_Y_MIN            0
#define PS4_TP_Y_MAX            943
#define PS4_TP_MAX_COUNT        128

/* Max gears for wheel mode */
#define PS4_MAX_GEARS           8

/* Sensor constants */
#define PS4_ACCEL_RES           8192
#define PS4_GYRO_RES            1024

/* Keepalive: force report every N ms */
#define PS4_KEEPALIVE_MS        5

/*******************************************************************************
 * Bit Masks for Buttons
 ******************************************************************************/

#define PS4_MASK_SQUARE         (1U <<  0)  /* West  □ */
#define PS4_MASK_CROSS          (1U <<  1)  /* South × */
#define PS4_MASK_CIRCLE         (1U <<  2)  /* East  ○ */
#define PS4_MASK_TRIANGLE       (1U <<  3)  /* North △ */
#define PS4_MASK_L1             (1U <<  4)
#define PS4_MASK_R1             (1U <<  5)
#define PS4_MASK_L2             (1U <<  6)
#define PS4_MASK_R2             (1U <<  7)
#define PS4_MASK_SELECT         (1U <<  8)  /* Share */
#define PS4_MASK_START          (1U <<  9)  /* Options */
#define PS4_MASK_L3             (1U << 10)
#define PS4_MASK_R3             (1U << 11)
#define PS4_MASK_PS             (1U << 12)
#define PS4_MASK_TP             (1U << 13)

/*******************************************************************************
 * PS4 Controller Types
 ******************************************************************************/

typedef enum {
    PS4_CT_NONE         = 0,
    PS4_CT_CONTROLLER   = 1,    /* DS4 standard controller */
    PS4_CT_HOTAS        = 2,    /* Flight stick */
    PS4_CT_WHEEL        = 3,    /* Racing wheel */
    PS4_CT_GAMEPAD      = 4,    /* Generic gamepad */
    PS4_CT_DRUMS        = 5,    /* Drum kit */
    PS4_CT_GUITAR       = 6,    /* Guitar */
    PS4_CT_ARCADESTICK  = 7     /* Arcade stick (also PS5 mode) */
} PS4ControllerType;

/*******************************************************************************
 * PS4 Authentication Report IDs
 ******************************************************************************/

typedef enum {
    PS4_AUTH_GET_CALIBRATION    = 0x02,
    PS4_AUTH_DEFINITION         = 0x03,
    PS4_AUTH_SET_FEATURE_STATE  = 0x05,
    PS4_AUTH_GET_MAC_ADDRESS    = 0x12,
    PS4_AUTH_SET_HOST_MAC       = 0x13,
    PS4_AUTH_SET_USB_BT_CONTROL = 0x14,
    PS4_AUTH_GET_VERSION_DATE   = 0xA3,
    PS4_AUTH_SET_AUTH_PAYLOAD   = 0xF0,
    PS4_AUTH_GET_SIGNATURE_NONCE = 0xF1,
    PS4_AUTH_GET_SIGNING_STATE  = 0xF2,
    PS4_AUTH_RESET_AUTH         = 0xF3
} PS4AuthReport;

/*******************************************************************************
 * Data Structures
 ******************************************************************************/

/* Touchpad finger data (4 bytes, packed into 3 bytes for X/Y) */
typedef struct __attribute__((packed)) {
    uint8_t counter   : 7;
    uint8_t unpressed : 1;
    uint8_t data[3];               /* 12-bit X, then 12-bit Y */
} PS4_TouchpadXY;

/* Touchpad data (2 fingers) */
typedef struct __attribute__((packed)) {
    PS4_TouchpadXY p1;
    PS4_TouchpadXY p2;
} PS4_TouchpadData;

/* 3-axis sensor (gyro/accel) */
typedef struct __attribute__((packed)) {
    int16_t x;
    int16_t y;
    int16_t z;
} PS4_Sensor;

/* Sensor data block inside gamepad misc data */
typedef struct __attribute__((packed)) {
    uint16_t  battery;
    PS4_Sensor gyroscope;
    PS4_Sensor accelerometer;
    uint8_t   misc[4];
    uint8_t   powerLevel   : 4;
    uint8_t   charging      : 1;
    uint8_t   headphones    : 1;
    uint8_t   microphone    : 1;
    uint8_t   extension     : 1;
    uint8_t   extData0      : 1;
    uint8_t   extData1      : 1;
    uint8_t   notConnected  : 1;
    uint8_t   extData3      : 5;
    uint8_t   misc2;
} PS4_SensorData;

/* Gamepad-specific misc data */
typedef struct __attribute__((packed)) {
    uint16_t        axisTiming;
    PS4_SensorData  sensorData;
    uint8_t         touchpadActive : 2;
    uint8_t         padding        : 6;
    uint8_t         tpadIncrement;
    PS4_TouchpadData touchpadData;
    uint8_t         mystery2[21];
} PS4_GamepadMisc;

/* Guitar-specific misc data */
typedef struct __attribute__((packed)) {
    uint8_t mystery0[22];
    uint8_t powerLevel  : 4;
    uint8_t             : 4;
    uint8_t mystery1[10];
    uint8_t pickup;
    uint8_t whammy;
    uint8_t tilt;
    union {
        uint8_t fretValue;
        struct __attribute__((packed)) {
            uint8_t green  : 1;
            uint8_t red    : 1;
            uint8_t yellow : 1;
            uint8_t blue   : 1;
            uint8_t orange : 1;
            uint8_t        : 3;
        } frets;
    };
    union {
        uint8_t soloFretValue;
        struct __attribute__((packed)) {
            uint8_t green  : 1;
            uint8_t red    : 1;
            uint8_t yellow : 1;
            uint8_t blue   : 1;
            uint8_t orange : 1;
            uint8_t        : 3;
        } soloFrets;
    };
    uint8_t mystery2[14];
} PS4_GuitarMisc;

/* Drum-specific misc data */
typedef struct __attribute__((packed)) {
    uint8_t mystery0[22];
    uint8_t powerLevel  : 4;
    uint8_t             : 4;
    uint8_t mystery1[10];
    uint8_t velocityDrumRed;
    uint8_t velocityDrumBlue;
    uint8_t velocityDrumYellow;
    uint8_t velocityDrumGreen;
    uint8_t velocityCymbalYellow;
    uint8_t velocityCymbalBlue;
    uint8_t velocityCymbalGreen;
    uint8_t mystery2[12];
} PS4_DrumMisc;

/* HOTAS-specific misc data */
typedef struct __attribute__((packed)) {
    uint8_t  mystery0[22];
    uint8_t  powerLevel   : 4;
    uint8_t               : 4;
    uint8_t  mystery1[10];
    uint16_t joystickX;
    uint16_t joystickY;
    uint8_t  twistRudder;
    uint8_t  throttle;
    uint8_t  rockerSwitch;
    uint8_t  pedalRudder;
    uint8_t  pedalLeft;
    uint8_t  pedalRight;
} PS4_HotasMisc;

/* Wheel-specific misc data */
typedef struct __attribute__((packed)) {
    uint8_t  mystery0[22];
    uint8_t  powerLevel   : 4;
    uint8_t               : 4;
    uint8_t  mystery1[10];
    uint16_t steeringWheel;
    uint16_t gasPedal;
    uint16_t brakePedal;
    uint16_t clutchPedal;
    union {
        uint8_t shifterValue;
        struct __attribute__((packed)) {
            uint8_t shifterGear1 : 1;
            uint8_t shifterGear2 : 1;
            uint8_t shifterGear3 : 1;
            uint8_t shifterGear4 : 1;
            uint8_t shifterGear5 : 1;
            uint8_t shifterGear6 : 1;
            uint8_t shifterGearR : 1;
            uint8_t              : 1;
        } shifter;
    };
    uint16_t unknownVal;
    uint8_t  buttonDialEnter : 1;
    uint8_t  buttonDialDown  : 1;
    uint8_t  buttonDialUp    : 1;
    uint8_t  buttonMinus     : 1;
    uint8_t  buttonPlus      : 1;
    uint8_t                  : 3;
    uint8_t  mystery2[7];
} PS4_WheelMisc;

/* Main PS4 HID Input Report (64 bytes total) */
typedef struct __attribute__((packed)) {
    uint8_t  reportID;          /* [0] Always 0x01 */
    uint8_t  leftStickX;        /* [1] */
    uint8_t  leftStickY;        /* [2] */
    uint8_t  rightStickX;       /* [3] */
    uint8_t  rightStickY;       /* [4] */
    
    /* D-pad (4 bits) + Buttons (14 bits) + Counter (6 bits) = 24 bits = 3 bytes */
    uint8_t  dpad          : 4;  /* [5] bits 0-3 */
    uint16_t buttonWest    : 1;  /* [5] bit 4 */
    uint16_t buttonSouth   : 1;
    uint16_t buttonEast    : 1;
    uint16_t buttonNorth   : 1;
    uint16_t buttonL1      : 1;
    uint16_t buttonR1      : 1;
    uint16_t buttonL2      : 1;
    uint16_t buttonR2      : 1;
    uint16_t buttonSelect  : 1;
    uint16_t buttonStart   : 1;
    uint16_t buttonL3      : 1;
    uint16_t buttonR3      : 1;
    uint16_t buttonHome    : 1;  /* [6] bit 1 */
    uint16_t buttonTouchpad: 1;  /* [6] bit 2 */
    uint8_t  reportCounter : 6;  /* [6] bits 2-7 */
    
    uint8_t  leftTrigger;       /* [7] */
    uint8_t  rightTrigger;      /* [8] */
    
    /* Vendor-specific misc data (device type dependent) */
    union {
        uint8_t         raw[54];
        PS4_GamepadMisc gamepad;
        PS4_GuitarMisc  guitar;
        PS4_DrumMisc    drums;
        PS4_HotasMisc   hotas;
        PS4_WheelMisc   wheel;
    } misc;
} PS4_InputReport;

/* PS4 Feature Output Report (from console to device, 32 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t reportID;                             /* [0]  0x05 */
    uint8_t enableUpdateRumble    : 1;            /* [1] */
    uint8_t enableUpdateLED       : 1;
    uint8_t enableUpdateLEDBlink  : 1;
    uint8_t enableUpdateExtData   : 1;
    uint8_t enableUpdateVolLeft   : 1;
    uint8_t enableUpdateVolRight  : 1;
    uint8_t enableUpdateVolMic    : 1;
    uint8_t enableUpdateVolSpeaker: 1;
    uint8_t reserved;                             /* [2] */
    uint8_t unknown0;                             /* [3] */
    uint8_t rumbleRight;                          /* [4] */
    uint8_t rumbleLeft;                           /* [5] */
    uint8_t ledRed;                               /* [6] */
    uint8_t ledGreen;                             /* [7] */
    uint8_t ledBlue;                              /* [8] */
    uint8_t ledBlinkOn;                           /* [9] */
    uint8_t ledBlinkOff;                          /* [10] */
    uint8_t extData[8];                           /* [11-18] */
    uint8_t volumeLeft;                           /* [19] */
    uint8_t volumeRight;                          /* [20] */
    uint8_t volumeMic;                            /* [21] */
    uint8_t volumeSpeaker;                        /* [22] */
    uint8_t unknownAudio;                         /* [23] */
    uint8_t padding[8];                           /* [24-31] */
} PS4_OutputReport;

/* IMU Configuration (10 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t gyroRange;
    uint16_t gyroResPerDegDenom;
    uint16_t gyroResPerDegNumer;
    uint16_t accelRange;
    uint16_t accelResPerG;
} PS4_IMUConfig;

/* Controller Configuration (Feature Report 0x03, 48 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t hidUsage;                            /* [0-1]   0x2721 */
    uint8_t  mystery0;                            /* [2]     0x04 */
    union {                                       /* [3] */
        struct __attribute__((packed)) {
            uint8_t enableController : 1;
            uint8_t enableMotion     : 1;
            uint8_t enableLED        : 1;
            uint8_t enableRumble     : 1;
            uint8_t enableAnalog     : 1;
            uint8_t enableUnknown0   : 1;
            uint8_t enableTouchpad   : 1;
            uint8_t enableUnknown1   : 1;
        } features;
    };
    uint8_t  controllerType;                      /* [4] */
    uint8_t  touchpadParam[2];                    /* [5-6]  */
    PS4_IMUConfig imuConfig;                      /* [7-16] */
    uint16_t magicID;                             /* [17-18] 0x0D0D */
    uint8_t  mystery1[4];                         /* [19-22] */
    uint8_t  wheelParam[3];                       /* [23-25] */
    uint8_t  mystery2[21];                        /* [26-47] */
} PS4_ControllerConfig;

/* Authentication data buffer (1064 bytes for RSA response) */
typedef struct {
    uint8_t  auth_buffer[1064];
    uint8_t  nonce_id;
    uint8_t  auth_state;         /* 0=idle, 1=sending to dongle, 2=sending to console */
    uint8_t  valid;              /* Authentication keys loaded? */
} PS4_AuthData;

/*******************************************************************************
 * Global Variables
 ******************************************************************************/

extern PS4_InputReport       g_ps4Report;
extern PS4_OutputReport      g_ps4Features;
extern PS4_ControllerConfig  g_ps4Config;
extern PS4_AuthData          g_ps4Auth;

/*******************************************************************************
 * Function Declarations
 ******************************************************************************/

/**
 * @brief  Initialize the PS4 gamepad driver
 * @param  ctrlType  Controller type (PS4_CT_CONTROLLER or PS4_CT_ARCADESTICK)
 */
void PS4_Init(PS4ControllerType ctrlType);

/**
 * @brief  Process gamepad inputs and prepare HID report
 * 
 * Call this in main loop. Reads GPIOs, fills input report,
 * and handles output report processing.
 */
void PS4_Process(void);

/**
 * @brief  Set a button state in the PS4 report
 * @param  mask   Button mask (e.g., PS4_MASK_CROSS)
 * @param  pressed 1 = pressed, 0 = released
 */
void PS4_SetButton(uint16_t mask, uint8_t pressed);

/**
 * @brief  Set left analog stick position
 * @param  x  X axis (0x00-0xFF, mid=0x80)
 * @param  y  Y axis (0x00-0xFF, mid=0x80)
 */
void PS4_SetLeftStick(uint8_t x, uint8_t y);

/**
 * @brief  Set right analog stick position
 */
void PS4_SetRightStick(uint8_t x, uint8_t y);

/**
 * @brief  Set D-Pad direction
 * @param  hat  PS4_HAT_* value
 */
void PS4_SetDpad(uint8_t hat);

/**
 * @brief  Set analog trigger values
 * @param  left   Left trigger (0x00-0xFF)
 * @param  right  Right trigger (0x00-0xFF)
 */
void PS4_SetTriggers(uint8_t left, uint8_t right);

/**
 * @brief  Send the current HID input report via USB
 * @return 1 if report was sent, 0 if not ready
 */
uint8_t PS4_SendReport(void);

/**
 * @brief  Get the latest output report (rumble/LED)
 * @return Pointer to the output report
 */
PS4_OutputReport* PS4_GetOutputReport(void);

/**
 * @brief  USB device initialization (called once at startup)
 */
void PS4_USB_DeviceInit(void);

/**
 * @brief  USB device transaction processing (call in main loop regularly)
 */
void PS4_USB_DevTransProcess(void);

/**
 * @brief  USB interrupt handler (call from USB_IRQHandler)
 */
void PS4_USB_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __PS4_GAMEPAD_H__ */
