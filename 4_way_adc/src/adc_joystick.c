/*******************************************************************************
 * adc_joystick.c - CH58x 4路ADC摇杆驱动库实现
 *******************************************************************************/

#include "adc_joystick.h"
#include <stdio.h>

/* ===================================================================
 *  内部常量 (由用户宏初始化)
 * =================================================================== */

static const uint8_t  aj_adc_ch[]   = AJ_ADC_CHANNELS;
static const uint32_t aj_gpio_pin[] = AJ_GPIO_PINS;
const char *const    AJ_ChNames[]  = AJ_CH_NAMES;

/* 通道数量 = 数组元素个数 */
#define AJ_CH_COUNT  (sizeof(aj_adc_ch) / sizeof(aj_adc_ch[0]))

/* 校准后的触发阈值 (mV) */
static uint16_t aj_trig_mv[4];
static uint16_t aj_diag_mv[4];

/* 方向名称 */
static const char *aj_dir_names[] = {
    "N", "L", "U", "R", "D", "LU", "RU", "LD", "RD"
};

/* 斜方向定义: {idx_a, idx_b, 方向码} */
static const uint8_t aj_diag_pairs[4][3] = {
    {0, 1, AJ_DIR_LU},  /* L + U */
    {2, 1, AJ_DIR_RU},  /* R + U */
    {0, 3, AJ_DIR_LD},  /* L + D */
    {2, 3, AJ_DIR_RD},  /* R + D */
};

/* ===================================================================
 *  内部辅助
 * =================================================================== */

/* 读取全部通道, 输出 mV */
static void aj_sample(uint16_t *mv)
{
    uint8_t i;
    for(i = 0; i < AJ_CH_COUNT; i++)
    {
        ADC_ChannelCfg(aj_adc_ch[i]);
        uint16_t val = ADC_ExcutSingleConver();
        mv[i] = (uint32_t)val * AJ_VREF_MV / AJ_ADC_RES;
    }
}

/* ===================================================================
 *  公共 API
 * =================================================================== */

/*********************************************************************
 * @fn      AJ_Init
 *
 * @brief   硬件初始化
 */
void AJ_Init(void)
{
    uint8_t i;

    for(i = 0; i < AJ_CH_COUNT; i++)
    {
        GPIOA_ModeCfg(aj_gpio_pin[i], GPIO_ModeIN_Floating);
    }

    ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_0);
}

/*********************************************************************
 * @fn      AJ_Calibrate
 *
 * @brief   自动校准
 */
void AJ_Calibrate(void)
{
    uint16_t mv[4];
    uint16_t calib_max[4] = {0};
    char     buf[64];
    uint8_t  i;

    /* 等待任意通道超过校准启动阈值 */
#if AJ_DEBUG
    AJ_UART_SEND("AJ: waiting for input...\r\n", 25);
#endif

    while(1)
    {
        aj_sample(mv);
        for(i = 0; i < AJ_CH_COUNT; i++)
        {
            if(mv[i] > AJ_CALIB_START_MV)
            {
#if AJ_DEBUG
                uint8_t len = sprintf(buf, "AJ: triggered by %s (%umV)\r\n",
                                      AJ_ChNames[i], mv[i]);
                AJ_UART_SEND(buf, len);
#endif
                goto start_calib;
            }
        }
        mDelaymS(20);
    }

start_calib:
    /* 校准窗口: 记录各通道最大值 */
    {
        uint16_t elapsed;
        for(elapsed = 0; elapsed < AJ_CALIB_TIME_MS; elapsed += 10)
        {
            aj_sample(mv);
            for(i = 0; i < AJ_CH_COUNT; i++)
            {
                if(mv[i] > calib_max[i]) calib_max[i] = mv[i];
            }
            mDelaymS(10);
        }
    }

    /* 计算阈值 */
    for(i = 0; i < AJ_CH_COUNT; i++)
    {
        aj_trig_mv[i] = (uint32_t)calib_max[i] * AJ_TRIG_MUL / AJ_TRIG_DIV;
        aj_diag_mv[i] = (uint32_t)calib_max[i] * AJ_DIAG_MUL / AJ_DIAG_DIV;
    }

#if AJ_DEBUG
    AJ_UART_SEND("AJ: calibration done.\r\n", 22);
    for(i = 0; i < AJ_CH_COUNT; i++)
    {
        uint8_t len = sprintf(buf,
            "AJ: %s max:%04umV trig:%04umV diag:%04umV\r\n",
            AJ_ChNames[i], calib_max[i], aj_trig_mv[i], aj_diag_mv[i]);
        AJ_UART_SEND(buf, len);
    }
    AJ_UART_SEND("---\r\n", 5);
#endif
}

/*********************************************************************
 * @fn      AJ_Read
 *
 * @brief   读取方向
 */
AJ_Direction AJ_Read(uint16_t *mv_out)
{
    uint16_t mv[4];
    uint8_t  cardinal = 0;
    uint8_t  state = AJ_DIR_N;
    uint8_t  i;

    aj_sample(mv);

    if(mv_out) {
        for(i = 0; i < AJ_CH_COUNT; i++) mv_out[i] = mv[i];
    }

    /* 正方向触发 */
    for(i = 0; i < AJ_CH_COUNT; i++)
    {
        if(mv[i] > aj_trig_mv[i]) cardinal |= (1 << i);
    }

    /* 斜方向: 有且仅有一对满足时触发 */
    {
        uint8_t diag_count = 0;
        for(i = 0; i < 4; i++)
        {
            uint8_t a = aj_diag_pairs[i][0];
            uint8_t b = aj_diag_pairs[i][1];
            if(mv[a] > aj_diag_mv[a] && mv[b] > aj_diag_mv[b])
            {
                diag_count++;
                state = aj_diag_pairs[i][2];
            }
        }
        if(diag_count != 1)
            state = AJ_DIR_N;
    }

    /* 无斜方向时判断正方向 (L > U > R > D 优先级) */
    if(state == AJ_DIR_N)
    {
        if(cardinal == 0)
            state = AJ_DIR_N;
        else if(cardinal & (1 << 0))
            state = AJ_DIR_L;
        else if(cardinal & (1 << 1))
            state = AJ_DIR_U;
        else if(cardinal & (1 << 2))
            state = AJ_DIR_R;
        else if(cardinal & (1 << 3))
            state = AJ_DIR_D;
    }

    return (AJ_Direction)state;
}

/*********************************************************************
 * @fn      AJ_GetThresholds
 *
 * @brief   获取校准后的触发阈值
 */
void AJ_GetThresholds(uint16_t *trig_out, uint16_t *diag_out)
{
    uint8_t i;
    for(i = 0; i < AJ_CH_COUNT; i++)
    {
        if(trig_out) trig_out[i] = aj_trig_mv[i];
        if(diag_out) diag_out[i] = aj_diag_mv[i];
    }
}

/*********************************************************************
 * @fn      AJ_DirName
 *
 * @brief   方向码 → 字符串
 */
const char *AJ_DirName(AJ_Direction dir)
{
    if(dir <= AJ_DIR_RD)
        return aj_dir_names[dir];
    return "?";
}
