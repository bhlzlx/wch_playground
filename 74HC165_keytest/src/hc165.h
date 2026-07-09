#pragma once

#include "CH58x_common.h"

// 引脚定义
#define HC165_PL_PIN    GPIO_Pin_4      // PA4
#define HC165_CLK_PIN   GPIO_Pin_5      // PA5
#define HC165_DATA_PIN  GPIO_Pin_15     // PA15

// 引脚操作宏
#define HC165_PL_High()   GPIOA_SetBits(HC165_PL_PIN)
#define HC165_PL_Low()    GPIOA_ResetBits(HC165_PL_PIN)
#define HC165_CLK_High()  GPIOA_SetBits(HC165_CLK_PIN)
#define HC165_CLK_Low()   GPIOA_ResetBits(HC165_CLK_PIN)
#define HC165_DATA_Read() GPIOA_ReadPortPin(HC165_DATA_PIN)

// 总按键数（3片 * 8位）
#define HC165_TOTAL_BITS  24


/**
 * @brief 初始化74HC165相关GPIO
 */
void HC165_Init(void);

/**
 * @brief 读取全部24个按键状态
 * @return uint32_t 低24位有效，bit0对应第一片D0，bit23对应第三片D7
 *         注意：数据排列取决于级联顺序，下面将说明
 */
uint32_t HC165_Read24(void);

/**
 * @brief 示例：获取某个按键的状态（按下为0，松开为1）
 * @param bit_index 按键编号（0~23），0对应第一片D0，23对应第三片D7
 * @return 1=松开，0=按下
 */
uint8_t HC165_GetKey(uint32_t key_status, uint8_t bit_index);