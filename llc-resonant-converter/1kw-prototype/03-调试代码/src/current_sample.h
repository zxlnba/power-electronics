#ifndef CURRENT_SAMPLE_H
#define CURRENT_SAMPLE_H

#include "main.h"
#include <stdbool.h>

/* ============ 电流采样参数（TMCS1101A1 + 分压） ============ */
#define CUR_ADC_VREF     3.3f     /* ADC 参考电压名义值（VDDA≈3.3V，兜底）；
                                    实际换算用 VREFINT 归一化 g_vref_volts（见 cur_ampere） */
#define CUR_ADC_BITS     4096.0f  /* 12 位 ADC */
#define CUR_SENS_VPERA   0.050f   /* TMCS1101A1 灵敏度 50mV/A */
#define CUR_SENS_VOS     2.5f     /* 零点输出 = 0.5 × 供电(5V) */
#define CUR_DIV_RATIO    0.607f   /* 分压比 = 5.1/(3.3+5.1)，按实际改 */

/* ADC2 通道：PA2=IN3(电流1)，PA3=IN4(电流2) */
#define CUR_CH_1         ADC_CHANNEL_3
#define CUR_CH_2         ADC_CHANNEL_4

extern volatile float    g_curr1;     /* 电流1 (A) */
extern volatile float    g_curr2;     /* 电流2 (A) */
extern volatile uint16_t g_cur_raw1;  /* 电流1 原始值 */
extern volatile uint16_t g_cur_raw2;  /* 电流2 原始值 */

bool  cur_init(void);
float cur_ampere(uint16_t counts);
bool  cur_read_avg(float *i1, float *i2, uint16_t samples);

#endif /* CURRENT_SAMPLE_H */
