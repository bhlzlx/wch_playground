/*******************************************************************************
 * Main.c - PS4 Compatible Gamepad Firmware for CH582M
 * 
 * Based on analysis of GP2040-CE PS4 driver.
 * Emulates a PS4-compatible HID gamepad on the CH582M.
 * 
 * GPIO Mapping (CH582M, customize for your PCB):
 *   PA0  - D-Pad Up       PA7  - Triangle
 *   PA1  - D-Pad Down     PA8  - L1
 *   PA2  - D-Pad Left     PA9  - R1
 *   PA3  - D-Pad Right    PA10 - L2
 *   PA4  - Cross (x)      PA11 - R2
 *   PA5  - Circle (o)     PA12 - Select/Share
 *   PA6  - Square ([])    PA13 - Start/Options
 *                         PA14 - L3 (Left stick)
 *                         PA15 - R3 (Right stick)
 *   PB0  - PS Home        PB1  - Touchpad
 * 
 * All buttons: active LOW (internal pull-up, connect to GND when pressed)
 ******************************************************************************/

#include "ps4_gamepad.h"

/* =========================================================================
 * GPIO Button Pin Definitions
 * ========================================================================= */
#define BTN_DPAD_UP        GPIO_Pin_0
#define BTN_DPAD_DOWN      GPIO_Pin_1
#define BTN_DPAD_LEFT      GPIO_Pin_2
#define BTN_DPAD_RIGHT     GPIO_Pin_3
#define BTN_CROSS          GPIO_Pin_4
#define BTN_CIRCLE         GPIO_Pin_5
#define BTN_SQUARE         GPIO_Pin_6
#define BTN_TRIANGLE       GPIO_Pin_7
#define BTN_L1             GPIO_Pin_8
#define BTN_R1             GPIO_Pin_9
#define BTN_L2             GPIO_Pin_10
#define BTN_R2             GPIO_Pin_11
#define BTN_SELECT         GPIO_Pin_12
#define BTN_START          GPIO_Pin_13
#define BTN_L3             GPIO_Pin_14
#define BTN_R3             GPIO_Pin_15

#define BTN_HOME           GPIO_Pin_0
#define BTN_TOUCHPAD       GPIO_Pin_1

#define BTN_PORT_A_MASK    (BTN_DPAD_UP    | BTN_DPAD_DOWN  | BTN_DPAD_LEFT  | \
                            BTN_DPAD_RIGHT | BTN_CROSS      | BTN_CIRCLE     | \
                            BTN_SQUARE     | BTN_TRIANGLE   | BTN_L1         | \
                            BTN_R1         | BTN_L2         | BTN_R2         | \
                            BTN_SELECT     | BTN_START      | BTN_L3         | \
                            BTN_R3)

#define BTN_PORT_B_MASK    (BTN_HOME | BTN_TOUCHPAD)

/* =========================================================================
 * Timing
 * ========================================================================= */
#define MAIN_LOOP_DELAY_MS 1

/* =========================================================================
 * Helper: Check if button is pressed (active LOW with pull-up)
 * ========================================================================= */
#define IS_PRESSED(port, pin)  (((port) & (pin)) == 0)

/* =========================================================================
 * Read all buttons and update PS4 report
 * ========================================================================= */
static void Read_Buttons(void)
{
    uint16_t portA = GPIOA_ReadPort();
    uint16_t portB = GPIOB_ReadPort();
    uint8_t dpad = PS4_HAT_NOTHING;
    
    /* Decode D-Pad (8-directional) */
    uint8_t up    = IS_PRESSED(portA, BTN_DPAD_UP);
    uint8_t down  = IS_PRESSED(portA, BTN_DPAD_DOWN);
    uint8_t left  = IS_PRESSED(portA, BTN_DPAD_LEFT);
    uint8_t right = IS_PRESSED(portA, BTN_DPAD_RIGHT);
    
         if (up && right)      dpad = PS4_HAT_UPRIGHT;
    else if (up && left)       dpad = PS4_HAT_UPLEFT;
    else if (down && right)    dpad = PS4_HAT_DOWNRIGHT;
    else if (down && left)     dpad = PS4_HAT_DOWNLEFT;
    else if (up)               dpad = PS4_HAT_UP;
    else if (down)             dpad = PS4_HAT_DOWN;
    else if (left)             dpad = PS4_HAT_LEFT;
    else if (right)            dpad = PS4_HAT_RIGHT;
    
    PS4_SetDpad(dpad);
    
    /* Action buttons */
    PS4_SetButton(PS4_MASK_CROSS,    IS_PRESSED(portA, BTN_CROSS));
    PS4_SetButton(PS4_MASK_CIRCLE,   IS_PRESSED(portA, BTN_CIRCLE));
    PS4_SetButton(PS4_MASK_SQUARE,   IS_PRESSED(portA, BTN_SQUARE));
    PS4_SetButton(PS4_MASK_TRIANGLE, IS_PRESSED(portA, BTN_TRIANGLE));
    
    /* Shoulder buttons */
    PS4_SetButton(PS4_MASK_L1, IS_PRESSED(portA, BTN_L1));
    PS4_SetButton(PS4_MASK_R1, IS_PRESSED(portA, BTN_R1));
    PS4_SetButton(PS4_MASK_L2, IS_PRESSED(portA, BTN_L2));
    PS4_SetButton(PS4_MASK_R2, IS_PRESSED(portA, BTN_R2));
    
    /* System buttons */
    PS4_SetButton(PS4_MASK_SELECT, IS_PRESSED(portA, BTN_SELECT));
    PS4_SetButton(PS4_MASK_START,  IS_PRESSED(portA, BTN_START));
    PS4_SetButton(PS4_MASK_L3,     IS_PRESSED(portA, BTN_L3));
    PS4_SetButton(PS4_MASK_R3,     IS_PRESSED(portA, BTN_R3));
    
    /* Home & Touchpad (Port B) */
    PS4_SetButton(PS4_MASK_PS, IS_PRESSED(portB, BTN_HOME));
    PS4_SetButton(PS4_MASK_TP, IS_PRESSED(portB, BTN_TOUCHPAD));
    
    /* Digital triggers from L2/R2 */
    uint8_t lt = IS_PRESSED(portA, BTN_L2) ? 0xFF : 0x00;
    uint8_t rt = IS_PRESSED(portA, BTN_R2) ? 0xFF : 0x00;
    PS4_SetTriggers(lt, rt);
}

/* =========================================================================
 * Main Entry Point
 * ========================================================================= */
int main(void)
{
    /* System clock: 60MHz PLL */
    SetSysClock(CLK_SOURCE_PLL_60MHz);
    
    /* Configure button GPIOs: input with pull-up (active LOW) */
    GPIOA_ModeCfg(BTN_PORT_A_MASK, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(BTN_PORT_B_MASK, GPIO_ModeIN_PU);
    
    /* Initialize PS4 gamepad as Arcade Stick mode (PS5 compatible) */
    PS4_Init(PS4_CT_ARCADESTICK);
    
    /* Initialize USB device */
    PS4_USB_DeviceInit();
    
    /* Enable USB interrupt */
    PFIC_EnableIRQ(USB_IRQn);
    
    /* Main loop */
    while (1)
    {
        Read_Buttons();                  /* Read physical inputs */
        PS4_SendReport();                /* Send HID report via USB */
        PS4_USB_DevTransProcess();       /* USB background processing */
        DelayMs(MAIN_LOOP_DELAY_MS);     /* 1ms loop timing */
    }
}

/* =========================================================================
 * USB Interrupt Handler
 * ========================================================================= */
__INTERRUPT
__HIGH_CODE
void USB_IRQHandler(void)
{
    PS4_USB_IRQHandler();
}

