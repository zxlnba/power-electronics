/**
  ******************************************************************************
  * @file    voltage_sample.c
  * @brief   双路电压采样（±200V，单端，经 AMC1350 + TLV9002）
  *          传递函数：VADC = VCM - K*Vrail  =>  Vrail = (VCM - VADC)/K
  *          参考电压按标准 3.3V，基线 VCM 固定（0V 输入时实测值，可调）
  ******************************************************************************
  */
#include "voltage_sample.h"

extern ADC_HandleTypeDef hadc1;

static float s_vcm = VS_VCM_DEF;   /* 0V 输入时 VADC 基线 */

uint16_t g_raw_pos = 0;   /* ADC1 原始值（诊断） */
uint16_t g_raw_neg = 0;   /* ADC2 原始值（诊断） */
extern volatile uint8_t g_adc_error;  /* 6=Start失败 7=第1次轮询超时 8=第2次轮询超时 */

/* ============ ADC 启动诊断 ============ */
volatile uint8_t  g_diag_adrdy       = 0;  /* 手动使能后 ADRDY 是否置位 */
volatile uint8_t  g_diag_elapsed     = 0;  /* ADRDY 就绪耗时 ms */
volatile uint16_t g_diag_devid       = 0;  /* DBGMCU DEV_ID */

/* 手动使能 ADC 并等 ADRDY（最长 50ms），返回 1=已就绪。
   能就绪则保持使能状态；无法就绪则关闭（ADDIS）后返回 0。
   elapsed_ms 可选，返回就绪耗时。 */
static uint8_t vs_manual_adc_ready(uint32_t *elapsed_ms)
{
  uint32_t t0 = HAL_GetTick();

  /* 若 ADC 残留已使能，直接等 ADRDY（官方流程先清标志再置位） */
  if (!(ADC1->CR & ADC_CR_ADEN))
  {
    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
  }

  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0)
  {
    if (HAL_GetTick() - t0 > 50) break;   /* 最多等 50ms */
  }

  if (elapsed_ms) *elapsed_ms = HAL_GetTick() - t0;

  if (ADC1->ISR & ADC_ISR_ADRDY) return 1;

  ADC1->CR |= ADC_CR_ADDIS;   /* 无法就绪：关闭恢复初始 */
  return 0;
}

/* 启动诊断：确保 ADC 就绪（规避 HAL 1ms 超时），并保持使能 */
void vs_adc_startup_diag(void)
{
  uint32_t elapsed = 0;

  g_diag_devid = (uint16_t)(DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID_Msk);
  g_diag_adrdy   = vs_manual_adc_ready(&elapsed);
  g_diag_elapsed = (uint8_t)(elapsed > 50 ? 50 : elapsed);
}

static bool vs_read_counts(uint16_t *c1, uint16_t *c2);  /* 前向声明 */

/* counts -> 母线电压（带符号）：正轨正数，负轨负数
   baseline = 0V 输入时的 VADC（固定值 VS_VCM_DEF）
   参考电压按标准 3.3V */
float vs_rail_voltage(float baseline_volts, uint16_t counts)
{
    float vadc = (float)counts * VS_ADC_VREF / VS_ADC_BITS;
    return (baseline_volts - vadc) / VS_K;   /* 反相：Vrail = (VCM - VADC)/K */
}

/* vref_volts = 当前参考电压（诊断当前固定用 VS_ADC_VREF=3.183）；
   k = 该通道灵敏度（分通道，见 VS_K_POS/VS_K_NEG）。 */
float vs_rail_voltage_vref(float baseline_volts, uint16_t counts, float vref_volts, float k)
{
    float vadc = (float)counts * vref_volts / VS_ADC_BITS;
    return (baseline_volts - vadc) / k;   /* 反相：Vrail = (VCM - VADC)/K */
}

bool vs_init(void)
{
    return true;
}

/* 阻塞读两路原始 counts（ADC1 两通道顺序转换）。
   若 HAL Start 因 1ms ADRDY 超时失败，手动等就绪后重试一次。 */
static bool vs_read_counts(uint16_t *c1, uint16_t *c2)
{
    /* 清残留 EOC/OVR 标志：上次 OVR 残留会阻断下次 EOC（第二次转换超时的元凶） */
    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_OVR;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        /* Start 失败=ADRDY 超时。多半是 HAL 1ms 超时太紧，手动等就绪后重试 */
        vs_manual_adc_ready(NULL);
        if (HAL_ADC_Start(&hadc1) != HAL_OK) { g_adc_error = 6; return false; }
    }
    /* 超时细分：OVR 置位=数据覆盖（读太慢/转换太快），否则=纯超时 */
    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
    {
        g_adc_error = (ADC1->ISR & ADC_ISR_OVR) ? 11 : 7;
        HAL_ADC_Stop(&hadc1); return false;
    }
    *c1 = HAL_ADC_GetValue(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
    {
        g_adc_error = (ADC1->ISR & ADC_ISR_OVR) ? 12 : 8;
        HAL_ADC_Stop(&hadc1); return false;
    }
    *c2 = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    g_adc_error = 0;
    return true;
}

bool vs_read_rails(float *v_pos, float *v_neg)
{
    uint16_t c1, c2;
    if (!vs_read_counts(&c1, &c2)) return false;
    g_raw_pos = c1;
    g_raw_neg = c2;
    if (v_pos) *v_pos = vs_rail_voltage(s_vcm, c1);
    if (v_neg) *v_neg = vs_rail_voltage(s_vcm, c2);
    return true;
}

/* 多次采样取平均，抑制噪声 */
bool vs_read_rails_avg(float *v_pos, float *v_neg, uint16_t samples)
{
    uint32_t sum1 = 0, sum2 = 0;
    uint16_t c1, c2;
    for (uint16_t i = 0; i < samples; i++) {
        if (!vs_read_counts(&c1, &c2)) return false;
        sum1 += c1; sum2 += c2;
    }
    g_raw_pos = (uint16_t)(sum1 / samples);   /* 更新 RAW 显示 */
    g_raw_neg = (uint16_t)(sum2 / samples);
    if (v_pos) *v_pos = vs_rail_voltage(s_vcm, (uint16_t)(sum1 / samples));
    if (v_neg) *v_neg = vs_rail_voltage(s_vcm, (uint16_t)(sum2 / samples));
    return true;
}
