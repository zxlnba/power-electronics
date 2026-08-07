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

  /* VADC1 -> PA0 (ADC1_IN1) */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;  /* 60us，平均掉 LLC 开关纹波 */
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
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    g_adc_error = 3;
    return; /* 通道2配置失败 */
  }
}
