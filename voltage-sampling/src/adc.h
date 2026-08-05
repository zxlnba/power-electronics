/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for the ADC1.
  ******************************************************************************
  */
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern ADC_HandleTypeDef hadc1;
extern volatile uint8_t g_adc_error;

void MX_ADC1_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
