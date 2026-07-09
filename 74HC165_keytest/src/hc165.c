#include "hc165.h"

/**
 * @brief 初始化74HC165相关GPIO
 */
void HC165_Init(void)
{
    // PA4 - PL (输出)
    GPIOA_ModeCfg(HC165_PL_PIN, GPIO_ModeOut_PP_5mA);
    // PA5 - CLK (输出)
    GPIOA_ModeCfg(HC165_CLK_PIN, GPIO_ModeOut_PP_5mA);
    // PA15 - Q7 (输入，带上拉)
    GPIOA_ModeCfg(HC165_DATA_PIN, GPIO_ModeIN_PU);
    
    // 初始状态：PL和CLK拉高
    HC165_PL_High();
    HC165_CLK_High();
}

/**
 * @brief 读取全部24个按键状态
 * @return uint32_t 低24位有效，bit0对应第一片D0，bit23对应第三片D7
 *         注意：数据排列取决于级联顺序，下面将说明
 */
uint32_t HC165_Read24(void)
{
    uint8_t i;
    uint32_t data = 0;
    
    // 1. 加载并行数据（所有芯片同时锁存）
    HC165_PL_Low();
    DelayUs(1);
    HC165_PL_High();
    DelayUs(1);
    
    // 2. 连续移位读取24位
    for(i = 0; i < HC165_TOTAL_BITS; i++)
    {
        data <<= 1;                     // 先左移，为当前位腾出位置
        
        if(HC165_DATA_Read())
            data |= 0x01;               // 当前位为1
        
        // 时钟上升沿，移出下一位
        HC165_CLK_Low();
        DelayUs(1);
        HC165_CLK_High();
        DelayUs(1);
    }
    
    return data;
}

/**
 * @brief 示例：获取某个按键的状态（按下为0，松开为1）
 * @param bit_index 按键编号（0~23），0对应第一片D0，23对应第三片D7
 * @return 1=松开，0=按下
 */
uint8_t HC165_GetKey(uint32_t key_status, uint8_t bit_index)
{
    if(bit_index >= HC165_TOTAL_BITS) return 1;
    // 注意：按键按下时对应位为0，所以取反
    return ((key_status >> bit_index) & 0x01) ^ 0x01;
}