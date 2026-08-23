#ifndef VOLTAGE_SAMPLE_H
#define VOLTAGE_SAMPLE_H

#include "main.h"
#include <stdbool.h>

/* ============ 采样链路参数（最终设计：串阻1k + 反馈7.5k） ============ */
/* 2026-08-10 按 10 点实测标定（万用表：输入电压量采样端子处，VADC 量 ADC 引脚）：
   ① VDDA 在漂且在抖（08-09 3.183V → 08-10 ≈3.16V；抖动 ±0.3% → 固定参考读数跳 0.8V）。
      读数改为 VREFINT 归一化（见 llc_ctrl_period_isr）：VADC = counts×g_vref/4096，
      g_vref = 3.0×VREFINT_CAL/VREFINT_counts 精确跟踪 VDDA——出厂标定与实测同载 VREFINT，
      ratio 抵消 ±1% 容差，参考漂移/抖动自动消除，无放大问题。
   ② 0V 输入 → VADC 基线 ~1.626V（08-09 实测 1.625、08-10 实测 1.627；±11.64V
      对称中心 1.6265 亦印证）。VS_VCM_DEF 即读数零点=固定 1.627V——
      VREFINT 归一化后 counts→VADC 与 VDDA 无关，直接减固定 VCM 即得 Vrail。
      不依赖上电自抓基线（前端上电需建立时间，早抓是爬升中随机值；突变拒绝曾把
      错误值锁成永久零位）。⚠️ 前端共模每次上电可能不同（实测 1.626 与 2.15V）：
      若某次上电落到 2.15V，固定零点会显示该次偏移——硬件特性，非固件可补。
   ③ 两路均反相：Vrail↑ → VADC↓，VADC = 1.627 − 0.0058×Vrail
      （以基线 1.626 重拟合 10 点：两通道 K 均 = 0.0058，误差 ≤0.1V；
       早前"K+ 0.0056 / K- 0.0060"是基线取 1.625 造成的假象）
   旧值 VCM=1.616 / K=0.0066 与实测矛盾（8V 处差 ~2.3V），已废弃。 */
#define VS_ADC_VREF   3.16f    /* g_vref 初值/钳制兜底（实测派生；归一化后对读数影响趋零） */
#define VS_ADC_BITS   4096.0f  /* 12 位 ADC */
#define VS_VCM_DEF    1.627f   /* 0V 零点(V)：固定标定值，读数直接减它 */
#define VS_K          0.0058f  /* 灵敏度 V/V（两通道一致，10 点拟合） */
#define VS_K_POS      0.0058f  /* 正轨灵敏度 */
#define VS_K_NEG      0.0058f  /* 负轨灵敏度 */

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
float vs_rail_voltage_vref(float baseline_volts, uint16_t counts, float vref_volts, float k);

#endif /* VOLTAGE_SAMPLE_H */
