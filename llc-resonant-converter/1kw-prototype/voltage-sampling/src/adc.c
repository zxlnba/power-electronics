/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration of the ADC1.
  *          VADC1 -> PA0 (ADC1_IN1)   正轨 +200V
  *          VADC2 -> PA1 (ADC1_IN2)   负轨 -200V
  ******************************************************************************
  */
#include "adc.h"

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

volatile uint8_t g_adc_error = 0;  /* 1=Init失败 2=校准失败 3=通道配置失败 */

void MX_ADC1_Init(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  hadc1.Instance = ADC1;

  /* G4 必须配置 ADC 时钟源（否则 ADC 无时钟，使能失败） */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    g_adc_error = 5;
    return;
  }

  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV16;   /* 170MHz/16=10.6MHz，安全 */
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.NbrOfConversion = 2;   /* 正轨 + 负轨 */
  hadc1.Init.NbrOfDiscConversion = 0;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.SamplingMode = ADC_SAMPLING_MODE_NORMAL;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    g_adc_error = 1;
    return; /* ADC 初始化失败，不阻塞 OLED */
  }

  /* 测试：暂不校准（校准可能卡住 ADC 状态） */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* VADC1 -> PA0 (ADC1_IN1)
     采样时间 12.5 周期（~2.4µs）供控制环用——640 周期(60µs) 比一个开关周期还长，
     无法每周期闭环；且窗口覆盖非整数个周期，变频时残留纹波随 fs 波动（拍频）。
     现在采样时刻 = 开关周期边界（REP 中断内软件触发），相位锁定，无需长采样平均。 */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    g_adc_error = 3;
    return; /* 通道1配置失败 */
  }

  /* VADC2 -> PA1 (ADC1_IN2) */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  sConfig.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    g_adc_error = 3;
    return; /* 通道2配置失败 */
  }
}

/* ADC2: 电流采样（TMCS1101 ×2）
   PA2 -> ADC2_IN3  电流1
   PA3 -> ADC2_IN4  电流2 */
void MX_ADC2_Init(void)
{
  hadc2.Instance = ADC2;

  /* ADC1/ADC2 共用 ADC12 时钟源（已在 MX_ADC1_Init 配好 SYSCLK） */
  hadc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV16;   /* 与 ADC1 一致，10.6MHz */
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.NbrOfConversion = 2;   /* 电流1 + 电流2 */
  hadc2.Init.NbrOfDiscConversion = 0;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.SamplingMode = ADC_SAMPLING_MODE_NORMAL;
  hadc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    return;
  }

  ADC_ChannelConfTypeDef sConfig = {0};

  /* 电流1 -> PA2 (ADC2_IN3)
     同样缩短到 12.5 周期：主循环 100ms 轮询两通道约 4.7µs，避免 120µs 阻塞。 */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_12CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  HAL_ADC_ConfigChannel(&hadc2, &sConfig);

  /* 电流2 -> PA3 (ADC2_IN4) */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  HAL_ADC_ConfigChannel(&hadc2, &sConfig);
}
