/*******************************************************************************
 * adc_joystick.h - CH58x 4路ADC摇杆驱动库
 *
 * 可配置项: 在 #include "adc_joystick.h" 之前 #define 对应宏即可覆盖默认值
 *******************************************************************************/
#ifndef __ADC_JOYSTICK_H__
#define __ADC_JOYSTICK_H__

#include "CH58x_common.h"

/* ===================================================================
 *  硬件引脚配置 (按 L / U / R / D 顺序)
 * =================================================================== */

/* ADC 通道: {CH_EXTIN_0, CH_EXTIN_1, CH_EXTIN_8, CH_EXTIN_7} */
#ifndef AJ_ADC_CHANNELS
#define AJ_ADC_CHANNELS  {CH_EXTIN_0, CH_EXTIN_1, CH_EXTIN_8, CH_EXTIN_7}
#endif

/* GPIO 引脚: {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_1, GPIO_Pin_2} */
#ifndef AJ_GPIO_PINS
#define AJ_GPIO_PINS  {GPIO_Pin_4, GPIO_Pin_5, GPIO_Pin_1, GPIO_Pin_2}
#endif

/* 通道名称 (调试打印用): {"PA4", "PA5", "PA1", "PA2"} */
#ifndef AJ_CH_NAMES
#define AJ_CH_NAMES  {"PA4", "PA5", "PA1", "PA2"}
#endif

/* ===================================================================
 *  校准参数
 * =================================================================== */

/* 校准启动阈值 (mV) —— 任意通道超过此值触发校准 */
#ifndef AJ_CALIB_START_MV
#define AJ_CALIB_START_MV  1500
#endif

/* 校准窗口时长 (ms) */
#ifndef AJ_CALIB_TIME_MS
#define AJ_CALIB_TIME_MS   2000
#endif

/* ===================================================================
 *  触发倍率 (max * MUL / DIV)
 * =================================================================== */

/* 正方向触发倍率, 默认 8/10 = 0.8 */
#ifndef AJ_TRIG_MUL
#define AJ_TRIG_MUL  8
#endif
#ifndef AJ_TRIG_DIV
#define AJ_TRIG_DIV  10
#endif

/* 斜方向触发倍率, 默认 6/10 = 0.6 */
#ifndef AJ_DIAG_MUL
#define AJ_DIAG_MUL  6
#endif
#ifndef AJ_DIAG_DIV
#define AJ_DIAG_DIV  10
#endif

/* ===================================================================
 *  调试输出
 * =================================================================== */

/* 调试打印开关: 1=打印, 0=静默 */
#ifndef AJ_DEBUG
#define AJ_DEBUG  1
#endif

/* UART 发送函数 (默认 UART1) */
#ifndef AJ_UART_SEND
#define AJ_UART_SEND(p, l)  UART1_SendString((uint8_t *)(p), (l))
#endif

/* 仅方向变化时打印 (需 AJ_DEBUG=1) */
#ifndef AJ_CHANGE_ONLY
#define AJ_CHANGE_ONLY  1
#endif

/* ADC 参考电压 (mV) */
#ifndef AJ_VREF_MV
#define AJ_VREF_MV  3300
#endif

/* ADC 分辨率 */
#ifndef AJ_ADC_RES
#define AJ_ADC_RES  4096
#endif

/* ===================================================================
 *  公共类型
 * =================================================================== */

/* 方向枚举 */
typedef enum {
    AJ_DIR_N = 0,
    AJ_DIR_L,
    AJ_DIR_U,
    AJ_DIR_R,
    AJ_DIR_D,
    AJ_DIR_LU,
    AJ_DIR_RU,
    AJ_DIR_LD,
    AJ_DIR_RD
} AJ_Direction;

/* ===================================================================
 *  公共 API
 * =================================================================== */

/* 硬件初始化: 配置 GPIO 为浮空输入, 使能 ADC */
void AJ_Init(void);

/* 运行校准: 等待用户输入 → 记录窗口最大值 → 计算阈值 */
void AJ_Calibrate(void);

/* 单次读取: 返回当前方向, mv[4] 输出各通道 mV 值 (可为 NULL) */
AJ_Direction AJ_Read(uint16_t *mv_out);

/* 获取校准后的触发阈值: trig[4]=正方向, diag[4]=斜方向 */
void AJ_GetThresholds(uint16_t *trig_out, uint16_t *diag_out);

/* 通道名称数组 (长度 = 通道数) */
extern const char *const AJ_ChNames[];

/* 方向码 → 字符串 */
const char *AJ_DirName(AJ_Direction dir);

#endif /* __ADC_JOYSTICK_H__ */
