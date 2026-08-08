/**
  ******************************************************************************
  * @file    llc_ctrl.c
  * @brief   1kW 全桥 LLC —— 数字电压环闭环控制实现（2p2z / Type II + 软启动）
  *
  *   离散 2p2z（直接 II 型，误差->频率）：
  *       w   = b0*e + x1
  *       x1  = b1*e - a1*w + x2
  *       x2  = b2*e - a2*w
  *       fs  = clamp( fs_nom + w, fs_min, fs_max )
  *   极性：fs↑ -> Vout↓（K_f<0），err>0（电压偏低）-> w<0 -> fs 下降 -> 升压。
  *
  *   状态机：OPENLOOP（等待/扫描）→ SOFTSTART（130k 斜坡到 107k）→ CLOSED（2p2z），
  *   任意状态检测到过流（g_llc_fault，main 主循环按电流判定）→ FAULT（停功率级，锁存）。
  *
  *   中断集成：REP 周期中断（HRTIM_TIM_IT_REP）→ HAL_HRTIM_IRQHandler(TimerD)
  *   → HAL_HRTIM_RepetitionEventCallback（本文件重写）→ llc_ctrl_period_isr()。
  ******************************************************************************
  */
#include "llc_ctrl.h"
#include "main.h"            /* HAL + HRTIM */
#include "hrtim.h"           /* hhrtim1 */
#include "voltage_sample.h"  /* VS_VCM_DEF, vs_rail_voltage */
#include "current_sample.h"  /* cur_ampere（本版仅在 main 主循环用，未在此调用） */

extern ADC_HandleTypeDef hadc1;
extern volatile float g_vpos, g_vneg;   /* main.c */
extern uint16_t g_raw_pos, g_raw_neg;   /* voltage_sample.c（定义非 volatile） */
extern volatile uint8_t g_adc_error;    /* adc.c */

/* 中断侧易失量 */
volatile llc_state_t g_llc_state  = LLC_STATE_OPENLOOP;
volatile float       g_llc_fs_cmd = LC_FS_MAX;
volatile bool        g_llc_fault  = false;
volatile bool        g_llc_start_req = false;

llc_ctrl_t g_ctrl;

/* ADC 快读超时保护（防死循环） */
#define ADC_FAST_GUARD  10000

static float fclamp(float v, float lo, float hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/* ADC1 顺序读两通道（正轨 PA0 / 负轨 PA1）。
   要求：ADC1 已使能（vs_adc_startup_diag 保证），扫描模式 2 转换，EOC_SINGLE。
   读 DR 自动清 EOC；带超时保护，超时置 g_adc_error 并返回 0。 */
static void adc1_fast_read(uint16_t *c1, uint16_t *c2)
{
  uint32_t guard = ADC_FAST_GUARD;
  ADC1->CR |= ADC_CR_ADSTART;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0)
  {
    if (--guard == 0) { g_adc_error = 9; *c1 = 0; *c2 = 0; return; }
  }
  *c1 = (uint16_t)ADC1->DR;

  guard = ADC_FAST_GUARD;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0)
  {
    if (--guard == 0) { g_adc_error = 10; *c2 = 0; return; }
  }
  *c2 = (uint16_t)ADC1->DR;
}

/* 过流/故障：停止 HRTIM TimerD 计数与 TD1/TD2 输出。
   从 REP 中断内调用安全（TimerD 自身的中断不会自锁）。 */
static void llc_stop_outputs(void)
{
  HAL_HRTIM_WaveformCounterStop(&hhrtim1, HRTIM_TIMERID_TIMER_D);
  HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD1);
  HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TD2);
}

void llc_ctrl_init(llc_ctrl_t *c)
{
  c->b0 = LC_B0 * LC_K_GAIN_SCALE;   /* 低压测试增益补偿；额定 400V 时 LC_K_GAIN_SCALE=1 */
  c->b1 = LC_B1 * LC_K_GAIN_SCALE;
  c->b2 = LC_B2 * LC_K_GAIN_SCALE;
  c->a1 = LC_A1;
  c->a2 = LC_A2;
  c->x1 = c->x2 = 0.0f;
  c->vref   = 200.0f;                 /* 额定 ±200V；低压测试在 main 里改 30 */
  c->fs_nom = LC_FS_NOM;
  c->fs_min = LC_FS_MIN;
  c->fs_max = LC_FS_MAX;
  c->fs_cmd = LC_FS_MAX;
  c->fs_raw = LC_FS_NOM;
  c->ss_step = 2.0f;                  /* 23kHz / 2Hz ≈ 11500 周期 ≈ 107ms 斜坡 */
  c->state   = LLC_STATE_OPENLOOP;
  c->enable  = false;
  c->fault_oc = false;
  c->ticks   = 0;

  g_llc_state   = LLC_STATE_OPENLOOP;
  g_llc_fs_cmd  = LC_FS_MAX;
  g_llc_fault   = false;
  g_llc_start_req = false;

  /* 确保 ADC1 已使能（vs_adc_startup_diag 正常会做，这里兜底） */
  if (!(ADC1->CR & ADC_CR_ADEN))
  {
    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
  }
}

void llc_ctrl_set_vref(llc_ctrl_t *c, float v)   { c->vref = v; }
void llc_ctrl_set_gain_scale(llc_ctrl_t *c, float k)
{
  c->b0 = LC_B0 * k;  c->b1 = LC_B1 * k;  c->b2 = LC_B2 * k;
}

/* 开环固定频率（扫描/调试用）：状态回 OPENLOOP，仅保持 fs_cmd */
void llc_ctrl_set_fixed_fs(llc_ctrl_t *c, float fs)
{
  c->state  = LLC_STATE_OPENLOOP;
  c->enable = false;
  c->x1 = c->x2 = 0.0f;
  c->fs_cmd = fclamp(fs, c->fs_min, c->fs_max);
  g_llc_state  = c->state;
  g_llc_fs_cmd = c->fs_cmd;
}

/* 进入软启动：清除故障锁存，从 130kHz 开始斜坡（重复调用无副作用） */
void llc_ctrl_start(llc_ctrl_t *c)
{
  if (c->state == LLC_STATE_CLOSED || c->state == LLC_STATE_SOFTSTART)
    return;                             /* 已在软启动/闭环，忽略 */

  c->fault_oc = false;
  c->x1 = c->x2 = 0.0f;
  c->fs_cmd = c->fs_max;
  c->state  = LLC_STATE_SOFTSTART;
  c->enable = false;
  g_llc_fault     = false;
  g_llc_start_req = false;
  g_llc_state     = c->state;
  g_llc_fs_cmd    = c->fs_cmd;
}

/* 2p2z 一步。vmeas = (vp + |vn|)/2，目标 c->vref。返回已钳位 fs_cmd。 */
float llc_ctrl_update(llc_ctrl_t *c, float vmeas)
{
  float w, e, fs;

  e = c->vref - vmeas;
  w  = c->b0*e + c->x1;
  c->x1 = c->b1*e - c->a1*w + c->x2;
  c->x2 = c->b2*e - c->a2*w;
  c->fs_raw = c->fs_nom + w;
  fs = fclamp(c->fs_raw, c->fs_min, c->fs_max);
  c->fs_cmd = fs;
  return fs;
}

/* fs(Hz) -> HRTIM TimerD 周期寄存器。PER = clk/fs；CMP1 = PER/2 保 50% 占空比。
   ⚠️ 必须用 HRTIM_TIMERINDEX_TIMER_D（数组下标 3）；HRTIM_TIMERID_TIMER_D 是
   MCR 掩码位，当数组下标会越界（旧 llc_controller.c 的 bug）。
   预装载已使能 + REP 更新：写在周期边界，下一次 REP 事件生效（自带 1 拍延迟）。 */
uint32_t llc_apply_frequency(float fs)
{
  uint32_t per;

  fs = fclamp(fs, LC_FS_MIN, LC_FS_MAX);
  per = (uint32_t)((LC_HRTIM_CLK / fs) + 0.5f);

  HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].PERxR  = per;
  HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP1xR = (per + 1) >> 1;  /* 50% */
  return per;
}

/* 每开关周期调用（HRTIM TimerD REP 中断内） */
void llc_ctrl_period_isr(void)
{
  llc_ctrl_t *c = &g_ctrl;
  uint16_t c1, c2;
  float vp, vn, vmeas;

  /* 1. 电压采样（ADC1）——采样时刻=周期边界，相位锁定，无纹波拍频 */
  adc1_fast_read(&c1, &c2);
  g_raw_pos = c1;
  g_raw_neg = c2;
  vp = vs_rail_voltage(VS_VCM_DEF, c1);   /* 正轨（带符号，如 +30） */
  vn = vs_rail_voltage(VS_VCM_DEF, c2);   /* 负轨（带符号，如 -30） */
  g_vpos = vp;
  g_vneg = vn;
  vmeas = (vp - vn) * 0.5f;               /* (vp + |vn|)/2 = 幅值平均 */

  /* 2. 过流/故障检查（g_llc_fault 由 main 主循环按电流判定） */
  if (g_llc_fault && c->state != LLC_STATE_FAULT)
  {
    c->enable = false;
    c->state  = LLC_STATE_FAULT;
    llc_stop_outputs();
  }

  /* 3. 状态机 */
  switch (c->state)
  {
    case LLC_STATE_OPENLOOP:
      break;                              /* 保持 c->fs_cmd（等待/扫描设定） */

    case LLC_STATE_SOFTSTART:
      c->fs_cmd -= c->ss_step;
      if (c->fs_cmd <= c->fs_nom)
      {
        c->fs_cmd = c->fs_nom;
        c->x1 = c->x2 = 0.0f;             /* 清状态，避免切入闭环时跳变 */
        c->enable = true;
        c->state  = LLC_STATE_CLOSED;
      }
      break;

    case LLC_STATE_CLOSED:
      llc_ctrl_update(c, vmeas);
      break;

    case LLC_STATE_FAULT:
      break;                              /* 输出已停止，等复位 */
  }

  /* 4. 频率写入（预装载，REP 更新生效） */
  llc_apply_frequency(c->fs_cmd);

  /* 中断侧易失量同步（OLED/调试读取） */
  g_llc_state  = c->state;
  g_llc_fs_cmd = c->fs_cmd;
}

/* HRTIM TimerD 周期（REP）中断回调 —— 重写 HAL weak 回调，接入控制节拍 */
void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
{
  if (TimerIdx == HRTIM_TIMERINDEX_TIMER_D)
    llc_ctrl_period_isr();
}
