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
#include "current_sample.h"
#include "OLED.h"
#include "llc_ctrl.h"
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
  MX_ADC2_Init();

  /* USER CODE BEGIN 2 */
  OLED_Init();
  OLED_Clear();

  /* ADC 启动诊断：确保 ADC 就绪（也规避 HAL 1ms 超时），保持使能 */
  vs_adc_startup_diag();
  /* 电压零点起点 VS_VCM_DEF，由 llc_ctrl.c 的"0V 慢速校零"在确认两路 0V 时
     自动修正到当前真实前端共模；VREFINT 归一化跟踪 VDDA（见 llc_ctrl_period_isr） */

  /* 控制环初始化 */
  llc_ctrl_init(&g_ctrl);
  llc_ctrl_set_vref(&g_ctrl, 30.0f);   /* 低压测试：目标 ±30V/路（额定 200V 时改回 200.0f） */

#if LLC_TEST_MODE_OPENLOOP_SWEEP
  /* 开环频率扫描（极性验证）：从 130kHz 起步，main 里每 800ms 步进 */
  llc_ctrl_set_fixed_fs(&g_ctrl, 130000.0f);
#else
  /* 正常运行：等自动软启动（LLC_START_DELAY_MS 后，见主循环）。
     先开控制板 → 延时窗口内合母线 → 到时自动软启动。 */
#endif

  OLED_ShowString(1, 1, "V+=");
  OLED_ShowString(2, 1, "V-=");

  /* 启动互补输出 TD1 (PB14) 和 TD2 (PB15)：初始 130kHz（最低增益），无母线时无功率。
     软启动触发前已在 130kHz 就位，合母线即低压预充，浪涌最小。 */
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1);
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD2);
  /* 启动 Timer D 计数 + REP 周期中断（每开关周期采样+控制）。
     ⚠️ 必须用 *_IT 版：旧名 WaveformCounterStart 经 legacy 宏映射到不带中断的
     WaveformCountStart，只 MCR|=TDEN 启动计数，不写 REPIE → ISR 永不触发，
     g_raw_pos/neg 永不更新 → 电压恒 0000。 */
  HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_TIMER_D);
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    /* 电流采样（ADC2）+ 过流判定。
       ⚠️ 电压(ADC1) 由 REP 周期中断独占，主循环不要再访问 ADC1。 */
    static uint32_t s_tick = 0;
    if (HAL_GetTick() - s_tick > 100)
    {
      s_tick = HAL_GetTick();
      float i1, i2;
      if (cur_read_avg(&i1, &i2, 16))
      {
        g_curr1 = 0.9f * g_curr1 + 0.1f * i1;   /* 轻平滑 */
        g_curr2 = 0.9f * g_curr2 + 0.1f * i2;
        if (g_curr1 > LC_OC_THRESH_A || g_curr2 > LC_OC_THRESH_A)
          g_llc_fault = true;                   /* 置位后由 ISR 停止功率级并锁存 */
      }
    }

#if !LLC_TEST_MODE_OPENLOOP_SWEEP
    /* 软启动触发：延时到点自动启动；或调试器把 g_llc_start_req 置 1 立即启动 */
    static uint8_t s_started = 0;
    if (!s_started)
    {
      if (HAL_GetTick() > LLC_START_DELAY_MS || g_llc_start_req)
      {
        llc_ctrl_start(&g_ctrl);
        s_started = 1;
      }
    }
#else
    /* 开环频率扫描（极性验证）：130k → 95k 步进，观察 vmeas 随 fs 下降而上升。
       确认 fs↑⇒Vout↓（K_f<0）后，把 LLC_TEST_MODE_OPENLOOP_SWEEP 置 0 重编译进闭环。 */
    static uint8_t  s_idx = 0;
    static uint32_t s_sweep = 0;
    const float sweep_fs[] = {130000, 125000, 120000, 115000, 110000, 107000, 104000, 100000, 95000};
    if (HAL_GetTick() - s_sweep > 800)
    {
      s_sweep = HAL_GetTick();
      if (s_idx < sizeof(sweep_fs)/sizeof(sweep_fs[0]))
        llc_ctrl_set_fixed_fs(&g_ctrl, sweep_fs[s_idx++]);
    }
#endif

    /* 每 500ms 刷新 OLED：
       行1 V+=xxx  行2 V-=xxx（中断实时更新）
       行3 I1=xxx I2=xxx  电流(A)
       行4 状态：fs=xxxkHz（正常）或 FAULT OC（过流锁存） */
    static uint32_t s_oled = 0;
    if (HAL_GetTick() - s_oled > 500)
    {
      s_oled = HAL_GetTick();
      NVIC_DisableIRQ(HRTIM1_TIMD_IRQn);   /* 保护软件 I2C 位时序（REP 中断 95kHz） */
      OLED_ShowFloatV(1, 4, g_vpos);
      OLED_ShowFloatV(2, 4, g_vneg);
      OLED_ShowString(3, 1, "I1=");
      OLED_ShowSignedNum(3, 4, (int32_t)g_curr1, 3);
      OLED_ShowString(3, 8, "I2=");
      OLED_ShowSignedNum(3, 11, (int32_t)g_curr2, 3);
      if (g_llc_state == LLC_STATE_FAULT)
      {
        OLED_ShowString(4, 1, "FAULT OC");
      }
      else
      {
        OLED_ShowString(4, 1, "fs=");
        OLED_ShowNum(4, 4, (uint32_t)(g_llc_fs_cmd / 1000.0f), 3);
        OLED_ShowString(4, 8, "kHz");
      }
      NVIC_EnableIRQ(HRTIM1_TIMD_IRQn);   /* 恢复 REP 中断（刷新期间已屏蔽） */
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
