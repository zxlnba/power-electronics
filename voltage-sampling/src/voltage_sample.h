#ifndef VOLTAGE_SAMPLE_H
#define VOLTAGE_SAMPLE_H

#include "main.h"
#include <stdbool.h>

/* ============ 采样链路参数（最终设计：串阻1k + 反馈7.5k） ============ */
#define VS_ADC_VREF   3.3f     /* VDDA 满量程，须 >=3.15V */
#define VS_ADC_BITS   4096.0f  /* 12 位 ADC */
#define VS_VCM_DEF    1.645f  /* 实测基线：0V输入VADC */   /* 实测基线：0V输入时VADC=2.0V */   /* 实测：次级5V实际4.58V → 4.58/3 */   /* VCM 理论值：5V x 10k/(20k+10k) */
#define VS_K          0.0066f  /* 实测：(1.645-1.249)/60 */  /* 实测：0.037V/60V */  /* 实测：板子实际增益1.45倍 */  /* 灵敏度 V/V：G=0.68 x 0.4 x 0.02215 */

/* ============ ADC 通道（按接线改） ============ */
#define VS_CH_POS     ADC_CHANNEL_1   /* VADC1 -> PA0，正轨 +200V */
#define VS_CH_NEG     ADC_CHANNEL_2   /* VADC2 -> PA1，负轨 -200V */

extern uint16_t g_raw_pos;
extern uint16_t g_raw_neg;

/* ============ ADC 启动诊断（排障用） ============ */
extern volatile uint8_t  g_diag_adrdy;        /* 手动使能 ADC 后 ADRDY 是否置位(0/1) */
extern volatile uint8_t  g_diag_elapsed;      /* ADRDY 就绪耗时 ms(最多 50) */
extern volatile uint16_t g_diag_devid;        /* DBGMCU DEV_ID：G474 应为 0x469 */
void  vs_adc_startup_diag(void);

bool  vs_init(void);
bool  vs_read_rails(float *v_pos, float *v_neg);
bool  vs_read_rails_avg(float *v_pos, float *v_neg, uint16_t samples);
float vs_rail_voltage(float baseline_volts, uint16_t counts);

#endif /* VOLTAGE_SAMPLE_H */
