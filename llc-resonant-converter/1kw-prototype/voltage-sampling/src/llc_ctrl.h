/**
  ******************************************************************************
  * @file    llc_ctrl.h
  * @brief   1kW 全桥 LLC —— 数字电压环闭环控制（2p2z / Type II + 软启动）
  *
  *   被控对象（降阶模型，见 control-loop/llc_loop_design.m）：
  *     Gvd(s) = K_f*(1+s/wz_lm) / ((1+s/wp_out)*(1 + s/(wr*Qr) + s^2/wr^2))
  *     fs=fr=107kHz 工作点：K_f = -0.505 V/kHz（400V 输入，m=Lm/Lr=7.4，与负载无关）。
  *     **极性：fs↑ ⇒ Vout↓（K_f<0），2p2z 的负号编码了反相，切勿改动符号。**
  *
  *   补偿器（Type II：积分 + 20Hz 零点 + 10kHz 高频极点），Tustin 离散为 2p2z，
  *   控制率 = 每开关周期（Ts = 1/fr ≈ 9.35µs），系数由 llc_loop_design.m 生成。
  *
  *   集成（关键）：
  *     1. HRTIM TimerD REP 周期中断（每开关周期）作为控制节拍；
  *     2. ADC1 在周期边界软件触发采样（相位锁定，无纹波拍频），采样时间必须
  *        已改为 12.5 周期（~4.7µs），不是原来的 640 周期（60µs）；
  *     3. 频率写入 = 预装载 PERxR + CMP1xR（=PER/2 保 50% 占空比），
  *        在 REP 更新事件生效（预装载使能），自带 1 拍延迟；
  *     4. 软启动：130kHz（最低增益）→ 斜坡到 107kHz → 闭合环路。
  *
  *   ⚠️ 低压测试（60V 电源）：K_f ∝ Vin，本模块把补偿器增益按 LC_K_GAIN_SCALE
  *   放大恢复穿越频率。额定 400V 时把 LC_K_GAIN_SCALE 改回 1.0 并重编译。
  *   ⚠️ 开环极性验证：首次上电先把 LLC_TEST_MODE_OPENLOOP_SWEEP 置 1 编译烧录，
  *   跑频率扫描确认 fs↑⇒Vout↓（K_f 为负）后，再置 0 进入闭环。见 main.c。
  ******************************************************************************
  */
#ifndef LLC_CTRL_H
#define LLC_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/* ============ 2p2z 系数（llc_loop_design.m 生成，勿手改；m=7.4 = 设计书 §3.2 k 值，Lm=2.0mH） ============ */
#define LC_B0  -45051.85926587f
#define LC_B1  -52.89763081f
#define LC_B2  44998.96163506f
#define LC_A1  -1.54594167f
#define LC_A2  0.54594167f

/* ============ 控制参数 ============ */
#define LC_HRTIM_CLK   170000000.0f /* HRTIM TimerD 时钟（170MHz） */
#define LC_FS_NOM      107000.0f    /* 额定开关频率 = fr（谐振点） */
#define LC_FS_MIN      95000.0f     /* 变频下限（升压区） */
#define LC_FS_MAX      130000.0f    /* 变频上限（降压区） */
#define LC_OC_THRESH_A 3.0f         /* 过流锁存阈值 A/路（额定 2.5A×1.2；低压测试需按负载电流调整） */

/* 低压测试（300W/60V 电源）：K_f 正比于 Vin，补偿器增益按 400/60=6.667 放大
   以恢复 fco≈2kHz。**额定 400V 运行时必须改回 1.0 并重编译。** */
#define LC_K_GAIN_SCALE 6.667f

/* 开环频率扫描测试模式：
     =1  首次上电极性验证（130kHz→95kHz 步进，观察 Vout 随 fs 下降而上升）。
     =0  正常运行（软启动 + 闭环）。极性确认后务必置回 0。 */
#define LLC_TEST_MODE_OPENLOOP_SWEEP 1

/* 软启动自动触发延时：MCU 上电后 N ms 才进入软启动。
   推荐时序：先开控制板 → 在此窗口内合母线 → 到时自动软启动。
   调试器里把 g_llc_start_req 置 1 可立即触发（跳过延时）。 */
#define LLC_START_DELAY_MS 2000

/* ============ 控制器状态 ============ */
typedef enum
{
  LLC_STATE_OPENLOOP = 0,   /* 开环：保持 fs_cmd（频率扫描/调试/等待软启动） */
  LLC_STATE_SOFTSTART,      /* 软启动：fs 从 130kHz 斜坡到 107kHz */
  LLC_STATE_CLOSED,         /* 闭环：2p2z 每开关周期调节 */
  LLC_STATE_FAULT           /* 过流锁存：已停止功率级，等复位 */
} llc_state_t;

typedef struct
{
  /* 2p2z 系数（init 时按 LC_K_GAIN_SCALE 缩放；a 不变） */
  float b0, b1, b2, a1, a2;
  float x1, x2;             /* DFII 内部状态 */
  float vref;               /* 目标每路电压（幅值）V */
  float fs_nom, fs_min, fs_max;
  float fs_cmd;             /* 当前频率指令 Hz（钳位后） */
  float fs_raw;             /* 未钳位原始指令（诊断） */
  float ss_step;            /* 软启动每周期步进 Hz（默认 2.0 ≈ 107ms 斜坡） */
  llc_state_t state;
  bool  enable;             /* 闭环使能（状态机内部管理） */
  bool  fault_oc;
  uint32_t ticks;           /* 周期计数（诊断） */
} llc_ctrl_t;

/* 供 main / Keil Watch 访问的中断侧易失量 */
extern volatile llc_state_t g_llc_state;   /* 当前状态（OLED/调试） */
extern volatile float       g_llc_fs_cmd;  /* 当前频率指令 Hz（OLED/调试） */
extern volatile bool        g_llc_fault;   /* 过流故障锁存（main 置位，ISR 动作） */
extern volatile bool        g_llc_start_req; /* 调试器置 1 → 立即软启动 */

void    llc_ctrl_init(llc_ctrl_t *c);
void    llc_ctrl_set_vref(llc_ctrl_t *c, float v);
void    llc_ctrl_set_gain_scale(llc_ctrl_t *c, float k);
void    llc_ctrl_set_fixed_fs(llc_ctrl_t *c, float fs);  /* 开环固定频率（扫描） */
void    llc_ctrl_start(llc_ctrl_t *c);                    /* 进入软启动（清除故障） */
float   llc_ctrl_update(llc_ctrl_t *c, float vmeas);      /* 2p2z 一步 */
uint32_t llc_apply_frequency(float fs);                   /* fs -> PERxR/CMP1xR */
void    llc_ctrl_period_isr(void);                        /* 每开关周期调用（REP 中断） */

/* 硬件保护相关（供 main 使用） */
extern llc_ctrl_t g_ctrl;

#endif /* LLC_CTRL_H */
