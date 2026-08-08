/**
  ******************************************************************************
  * @file    llc_controller.c
  * @brief   1kW 全桥 LLC —— 数字电压环控制器实现（2p2z / Type II）
  *
  *   离散 2p2z（直接 II 型，误差->频率）：
  *       w   = b0*e + x1
  *       x1  = b1*e - a1*w + x2
  *       x2  = b2*e - a2*w
  *       fs  = clamp( fs_nom + w )
  *
  *   极性：fs↑ -> Vout↓（K_f<0），err>0（电压偏低）-> w<0 -> fs 下降 -> 升压，
  *   负反馈方向正确。
  *
  *   保护：
  *     过流（任一路电流 > LC_OC_THRESH_A）锁存 fault_oc，输出退回固定
  *     LC_FS_MIN（最大升压点）并保持，直到外部清零 enable。
  *
  *   使用（main.c，每开关周期中断内）：
  *       vmeas = (vp + |vn|)/2   或两路 ADC 电压幅值平均
  *       fs = llc_ctrl_update(&c, vmeas, i_load);
  *       llc_apply_frequency(fs);
  ******************************************************************************
  */
#include "llc_controller.h"
#include "stm32g4xx_hal.h"   /* 依赖 HAL 头（HRTIM 寄存器） */

/* HRTIM TimerD 索引（stm32g4xx_hal.h 定义） */
#define LC_TIMERD  HRTIM_TIMERID_TIMER_D

void llc_ctrl_init(llc_ctrl_t *c)
{
  c->b0 = LC_B0; c->b1 = LC_B1; c->b2 = LC_B2;
  c->a1 = LC_A1; c->a2 = LC_A2;
  c->x1 = 0.0f;  c->x2 = 0.0f;
  c->vref  = LC_VREF_RAIL;
  c->fs_cmd = LC_FS_NOM;
  c->fs_raw = LC_FS_NOM;
  c->enable  = false;
  c->fault_oc = false;
  c->mode = 0;               /* 默认开环 */
}

void llc_ctrl_set_vref(llc_ctrl_t *c, float v)
{
  c->vref = v;
}

float llc_ctrl_update(llc_ctrl_t *c, float vmeas, float i_load)
{
  float w, e, fs;

  if (!c->enable)
  {
    /* 开环：固定额定频率 */
    c->x1 = 0.0f; c->x2 = 0.0f;     /* 清状态，避免切入闭环时跳变 */
    c->fs_cmd = LC_FS_NOM;
    return c->fs_cmd;
  }

  /* 过流保护（锁存） */
  if (i_load > LC_OC_THRESH_A)
    c->fault_oc = true;

  if (c->fault_oc)
  {
    /* 锁存：退到最大升压点（最低 fs），等外部复位 */
    c->fs_cmd = LC_FS_MIN;
    return c->fs_cmd;
  }

  /* 2p2z 差分方程 */
  e = c->vref - vmeas;
  w  = c->b0*e + c->x1;
  c->x1 = c->b1*e - c->a1*w + c->x2;
  c->x2 = c->b2*e - c->a2*w;

  /* 变频：fs = fr + w，带输出钳位（抗饱和防止积分无限增长） */
  c->fs_raw = LC_FS_NOM + w;
  fs = c->fs_raw;
  if (fs < LC_FS_MIN) fs = LC_FS_MIN;
  if (fs > LC_FS_MAX) fs = LC_FS_MAX;
  c->fs_cmd = fs;

  return fs;
}

/* fs(Hz) -> HRTIM TimerD 周期寄存器值。PER = clk/fs（对应 50% 占空比，半周期计数值）。
   写寄存器后置 SWU 请求，下一个周期起生效。 */
uint32_t llc_apply_frequency(float fs)
{
  uint32_t per;
  if (fs < LC_FS_MIN) fs = LC_FS_MIN;
  if (fs > LC_FS_MAX) fs = LC_FS_MAX;
  per = (uint32_t)((LC_HRTIM_CLK / fs) + 0.5f);

  HRTIM1->sTimerxRegs[LC_TIMERD].PERxR = per;
  HRTIM1->sTimerxRegs[LC_TIMERD].CR2 |= HRTIM_CR2_SWU;
  return per;
}
