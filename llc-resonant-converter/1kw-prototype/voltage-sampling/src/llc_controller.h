/**
  ******************************************************************************
  * @file    llc_controller.h
  * @brief   1kW 全桥 LLC —— 数字电压环控制器（2p2z / Type II）
  *
  *   被控对象（降阶模型，见 control-loop/llc_loop_design.m）：
  *     Gvd(s) = K_f*(1+s/wz_lm) / ((1+s/wp_out)*(1 + s/(wr*Qr) + s^2/wr^2))
  *     满载工作点 fs=fr=107kHz：K_f = -0.535 V/kHz（fs↑则 Vout↓，反相）
  *
  *   补偿器（Type II：积分 + 20Hz 零点 + 10kHz 高频极点）：
  *     C(s) = K_c*(1+s/2pi/20) / (s*(1+s/2pi/10000)),  K_c = -2.3571e7
  *     穿越 fco≈2kHz，相位裕度 PM≈70°(含1拍延迟)，增益裕度 GM≈44dB
  *
  *   离散化：Tustin，控制率 = 每开关周期 (Ts = 1/fr = 9.349us)
  *   系数由 llc_loop_design.m 生成 —— 若改控制率/参数，重新跑脚本覆盖。
  *
  *   使用：
  *     llc_ctrl_t c;  llc_ctrl_init(&c);
  *     在开关周期中断里:  llc_ctrl_update(&c, vmeas, i_load) -> fs_cmd
  *     llc_apply_frequency(c.fs_cmd) 写入 HRTIM TimerD 周期寄存器
  ******************************************************************************
  */
#ifndef LLC_CONTROLLER_H
#define LLC_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

/* ============ 2p2z 系数（llc_loop_design.m 生成，勿手改） ============ */
#define LC_B0  -42608.62147483f
#define LC_B1  -50.02890368f
#define LC_B2  42558.59257115f
#define LC_A1  -1.54594167f
#define LC_A2  0.54594167f

/* ============ 控制参数（按实际整定） ============ */
#define LC_VREF_RAIL    200.0f      /* 每路目标电压 V（±200V 双输出） */
#define LC_FS_NOM       107000.0f   /* 额定开关频率 = fr */
#define LC_FS_MIN       95000.0f    /* 变频下限（升压区，见 README 满载裕量发现） */
#define LC_FS_MAX       130000.0f   /* 变频上限（轻载降压） */
#define LC_HRTIM_CLK    170000000.0f/* HRTIM TimerD 时钟 */
#define LC_OC_THRESH_A  3.0f        /* 过流阈值 A/路（额定2.5A × 1.2） */
#define LC_OC_HYST_A    0.2f        /* 过流退出迟滞，防抖动 */

/* ============ 控制器状态 ============ */
typedef struct
{
  /* 系数（初始化时从宏拷贝，便于在线修改） */
  float b0, b1, b2, a1, a2;
  /* 内部状态（DFII） */
  float x1, x2;
  /* 设定与输出 */
  float vref;             /* 电压基准 V */
  float fs_cmd;           /* 频率指令 Hz（钳位后） */
  float fs_raw;           /* 未钳位原始指令（诊断） */
  /* 状态 */
  bool  enable;           /* 闭环使能；false 时输出 LC_FS_NOM */
  bool  fault_oc;         /* 过流锁存 */
  uint8_t mode;           /* 0=开环(固定fs) 1=闭环 */
} llc_ctrl_t;

void   llc_ctrl_init(llc_ctrl_t *c);
void   llc_ctrl_set_vref(llc_ctrl_t *c, float v);
/* 每控制周期调用。vmeas: 两路电压幅值平均值（(vp-vn)/2）；i_load: 任意一路电流(A)
   返回本次频率指令 fs_cmd（已钳位）。 */
float  llc_ctrl_update(llc_ctrl_t *c, float vmeas, float i_load);
/* 频率 -> HRTIM TimerD 周期寄存器值，并请求更新生效 */
uint32_t llc_apply_frequency(float fs);

#endif /* LLC_CONTROLLER_H */
