/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *                    双路 ±200V 电压采样（ADC1: PA0 正轨, PA1 负轨）
  *                    结果在全局变量 g_vpos / g_vneg（Keil Watch 查看）
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"
#include "hrtim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "adc.h"
#include "voltage_sample.h"
#include "OLED.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile float g_vpos = 0.0f;   /* 正轨电压（+） */
volatile float g_vneg = 0.0f;   /* 负轨电压（-） */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 显示带 1 位小数的电压：+123.4 */
void OLED_ShowFloatV(uint8_t Line, uint8_t Column, float val)
{
  int32_t n = (int32_t)val;
  int32_t d = (int32_t)((val - n) * 10.0f);
  if (d < 0) d = -d;
  if (d > 9) d = 9;
  OLED_ShowSignedNum(Line, Column, n, 3);
  OLED_ShowChar(Line, Column + 4, '.');
  OLED_ShowNum(Line, Column + 5, d, 1);
}

/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_HRTIM1_Init();
  MX_ADC1_Init();

  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();

  /* ADC 启动诊断：确保 ADC 就绪（也规避 HAL 1ms 超时），保持使能 */
  vs_adc_startup_diag();
  /* 基线用固定值 VS_VCM_DEF，与上电顺序无关；VREFINT 每轮自动校准参考电压 */

  OLED_ShowString(1, 1, "V+=");
  OLED_ShowString(2, 1, "V-=");

  /* 启动互补输出 TD1 (PB14) 和 TD2 (PB15) */
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1);
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD2);
  /* 启动 Timer D 计数 */
  HAL_HRTIM_WaveformCounterStart(&hhrtim1, HRTIM_TIMERID_TIMER_D);
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    /* 每 100ms 采样 64 次平均，结果在 g_vpos / g_vneg（加平滑滤波压住显示跳动） */
    static uint32_t s_tick = 0;
    if (HAL_GetTick() - s_tick > 100)
    {
      s_tick = HAL_GetTick();
      float vp, vn;
      if (vs_read_rails_avg(&vp, &vn, 64))
      {
        /* 轻指数平滑：时间常数约0.3s，响应快又稳（供电稳定后可放宽） */
        g_vpos = 0.7f * g_vpos + 0.3f * vp;
        g_vneg = 0.7f * g_vneg + 0.3f * vn;
      }    }
    /* 每 500ms 刷新 OLED（标准显示）：
       行1 V+=xxx  行2 V-=xxx  母线电压
       行3 RAW=xxxx ADC原始值
       行4 E<e> 错误码（0=正常） */
    static uint32_t s_oled = 0;
    if (HAL_GetTick() - s_oled > 500)
    {
      s_oled = HAL_GetTick();
      OLED_ShowFloatV(1, 4, g_vpos);
      OLED_ShowFloatV(2, 4, g_vneg);
      OLED_ShowString(3, 1, "RAW=");
      OLED_ShowNum(3, 5, g_raw_pos, 4);
      OLED_ShowChar(4, 1, 'E');
      OLED_ShowNum(4, 2, g_adc_error, 1);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV3;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    Error_Handler();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* 用户可添加自定义的断言输出 */
}
#endif /* USE_FULL_ASSERT */
