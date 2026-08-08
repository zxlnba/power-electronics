#ifndef VOLTAGE_SAMPLE_H
#define VOLTAGE_SAMPLE_H

#include "main.h"
#include <stdbool.h>

/* ============ 采样链路参数（最终设计：串阻1k + 反馈7.5k） ============ */
#define VS_ADC_VREF   3.3f     /* ADC 参考电压 = VDDA = 3.3V（标准） */
#define VS_ADC_BITS   4096.0f  /* 12 位 ADC */
#define VS_VCM_DEF    1.645f   /* 0V 输入时 VADC 基线（标准值） */
#define VS_K          0.0066f  /* 灵敏度 V/V：实测 (1.645-1.249)/60 */

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
