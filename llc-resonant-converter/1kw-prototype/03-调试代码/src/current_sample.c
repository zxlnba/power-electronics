/**
  ******************************************************************************
  * @file    current_sample.c
  * @brief   电流采样（TMCS1101A1 霍尔电流传感器 ×2）
  *          ADC2: PA2=IN3(电流1), PA3=IN4(电流2)
  *          传递函数：
  *            VOUT(传感器) = 0.5×VCC + 0.050×I   (VCC=5V → 零点2.5V)
  *            分压后 VADC = VOUT × 0.607         (5.1/(3.3+5.1))
  *            =>  I = (VADC/0.607 - 2.5) / 0.050
  ******************************************************************************
  */
#include "current_sample.h"

extern ADC_HandleTypeDef hadc2;

volatile float    g_curr1 = 0.0f;
volatile float    g_curr2 = 0.0f;
volatile uint16_t g_cur_raw1 = 0;
volatile uint16_t g_cur_raw2 = 0;

bool cur_init(void)
{
    return true;
}

/* counts -> 电流(A) */
float cur_ampere(uint16_t counts)
{
    float vadc = (float)counts * CUR_ADC_VREF / CUR_ADC_BITS;
    float vout = vadc / CUR_DIV_RATIO;        /* 还原传感器输出 */
    return (vout - CUR_SENS_VOS) / CUR_SENS_VPERA;
}

/* 阻塞读两路电流 counts，若 Start 超时重试一次 */
static bool cur_read_counts(uint16_t *r1, uint16_t *r2)
{
    if (HAL_ADC_Start(&hadc2) != HAL_OK)
    {
        /* 同电压采样：手动等 ADRDY 后重试 */
        ADC1->ISR = ADC_ISR_ADRDY;
        ADC2->CR |= ADC_CR_ADEN;
        uint32_t t0 = HAL_GetTick();
        while ((ADC2->ISR & ADC_ISR_ADRDY) == 0)
        {
            if (HAL_GetTick() - t0 > 50) break;
        }
        if (HAL_ADC_Start(&hadc2) != HAL_OK) return false;
    }
    if (HAL_ADC_PollForConversion(&hadc2, 100) != HAL_OK) { HAL_ADC_Stop(&hadc2); return false; }
    *r1 = HAL_ADC_GetValue(&hadc2);
    if (HAL_ADC_PollForConversion(&hadc2, 100) != HAL_OK) { HAL_ADC_Stop(&hadc2); return false; }
    *r2 = HAL_ADC_GetValue(&hadc2);
    HAL_ADC_Stop(&hadc2);
    return true;
}

/* 多次采样取平均，结果在 g_curr1 / g_curr2 */
bool cur_read_avg(float *i1, float *i2, uint16_t samples)
{
    uint32_t sum1 = 0, sum2 = 0;
    uint16_t r1, r2;
    for (uint16_t i = 0; i < samples; i++)
    {
        if (!cur_read_counts(&r1, &r2)) return false;
        sum1 += r1; sum2 += r2;
    }
    g_cur_raw1 = (uint16_t)(sum1 / samples);
    g_cur_raw2 = (uint16_t)(sum2 / samples);
    g_curr1 = cur_ampere(g_cur_raw1);
    g_curr2 = cur_ampere(g_cur_raw2);
    if (i1) *i1 = g_curr1;
    if (i2) *i2 = g_curr2;
    return true;
}
