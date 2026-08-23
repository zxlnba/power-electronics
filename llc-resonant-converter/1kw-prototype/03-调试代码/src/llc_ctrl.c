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
volatile uint32_t    g_isr_cnt    = 0;    /* ISR 心跳计数（诊断：确认控制中断在跑） */

llc_ctrl_t g_ctrl;

/* ADC 快读超时保护（防死循环）。
   实测 ADC 使能就绪后一次 2 通道转换 ~4μs，guard 正常只用几十次；
   500 次 ≈ 18μs，即使 ADC 异常超时也不会饿死 REP 中断/主循环，
   让 OLED 能显示 E=9/10 错误码。 */
#define ADC_FAST_GUARD  500

static float fclamp(float v, float lo, float hi)
{
  return v < lo ? lo : (v > hi ? hi : v);
}

/* ADC1 顺序读两通道（正轨 PA0 / 负轨 PA1）。
   要求：ADC1 已使能（vs_adc_startup_diag 保证），扫描模式 2 转换，EOC_SINGLE。
   读 DR 自动清 EOC；带超时保护，超时置 g_adc_error 并返回 0。 */
/* ADC1 单通道交替采样（无忙等）。
   根因：扫描模式 2 通道一次 ADSTART 连续转完，ch2 完成时 ch1 的 DR 未读
   → OVR 置位 → G4 中 OVR 挂起时 EOC 不再置位 → 永远读不到（V=0）。
   对策：每次 REP 只触发单通道，下周期进 ISR 时必已转完，读 DR + 清 OVR
   + 切另一通道触发。单转换无覆盖、无 OVR、无忙等，ISR 恒短。
   ⚠️ G4 SQR1 布局（用 LL 头核对过）：L[3:0] 在 bit0-3，SQ1[4:0] 在 bit6-10，
   SQ2 在 bit12-16。别把 L 当高字段写——写 0x1 等于 L=1 仍是双通道扫描！

   三通道轮询：PA0(正) → PA1(负) → VREFINT(内部基准)。每 3 个 REP 一轮。
   读数 = VREFINT 归一化（见 llc_ctrl_period_isr）：VADC = counts×g_vref/4096，
   g_vref 精确跟踪 VDDA（出厂标定与实测同载 VREFINT，ratio 抵消容差）
   → 参考漂移/抖动不影响读数；零点=固定 VS_VCM_DEF=1.627V（不抓基线）。 */
#define ADC1_SQR1_SINGLE_CH1   (1U << 6)    /* CH1 = PA0 正轨 */
#define ADC1_SQR1_SINGLE_CH2   (2U << 6)    /* CH2 = PA1 负轨 */
#define ADC1_SQR1_SINGLE_VREF  (18U << 6)   /* CH18 = VREFINT 内部基准 */

/* VREFINT 工厂校准值：VREF+ = 3.0V 条件下测得的 VREFINT 12 位 ADC 原始值（存 Flash 0x1FFF75AA） */
#define VREFINT_CAL_VAL   ((uint16_t)(*((volatile uint16_t*)0x1FFF75AAUL)))

static uint8_t s_adc_ch_sel = 0;   /* 0=上周期采PA0 1=上周期采PA1 2=上周期采VREFINT */
float g_vref_volts = 3.16f;        /* VREFINT 跟踪的 VDDA（0.9s 平均+低通），读数归一化用 */
uint16_t g_base_pos_c = 0;         /* 上电基线 PA0 counts（诊断：OLED 显示，定位读数偏移源） */
uint16_t g_base_neg_c = 0;

/* 信号通道平滑：指数低通 α=1/16（压显示抖动；尖峰由显示侧 344ms 低通吸收）。
   初值=上电基线仅作起步（0.5ms 内自收敛到真实值，不影响稳态）。
   ⚠️ 已删除突变拒绝：它会把错误基线锁死成永久零位（曾 0V 永久读几十V、
   每次上电不同），且会拒绝真实的快速电压变化。
   VREFINT 32 次累加平均 + 0.25 低通 → g_vref_volts（进读数归一化，见 period_isr）。 */
static uint16_t s_pos_sm  = 0;     /* PA0 counts 平滑值（初值=上电基线） */
static uint16_t s_neg_sm  = 0;     /* PA1 counts 平滑值 */
static float   s_base_pos_v = 0.0f;  /* 读数零点 VADC(V)：起点 1.627V，0V 门内慢速校零修正 */
static float   s_base_neg_v = 0.0f;
static float   s_vp_disp = 0.0f;     /* 显示慢低通状态（344ms 时间常数） */
static float   s_vn_disp = 0.0f;
static float   s_vm_gate = 0.0f;     /* 校零门：|vmeas| 慢平均（τ≈53ms），0V 恒开/出压即关 */
#define DISP_LP_N  32768.0f          /* α=1/N per REP → 95kHz/32768≈344ms */
#define REZERO_N     50000.0f        /* 校零低通：α=1/N per REP → τ≈0.5s，无跳变 */
#define REZERO_AVG_N  5000.0f        /* 校零门平均：α=1/N → τ≈53ms */
#define REZERO_GATE_V 0.5f           /* |vmeas|<0.5V → 两路都在 0V，允许校零 */
static uint32_t s_vref_acc = 0;    /* VREFINT counts 累加器 */
static uint16_t s_vref_cnt = 0;    /* VREFINT 已累加样本数 */
#define VREF_AVG_N  32             /* 32×~31.6μs≈1ms 平均一次 */
#define BL_N_SAMP   16             /* 基线每通道有效采样数 */

static void adc1_sample_isr(void)
{
  uint32_t isr;
  uint16_t d;

  if (!(ADC1->CR & ADC_CR_ADEN))   /* ADC 未使能：直接报错，不触发 */
  {
    g_adc_error = 20;
    return;
  }

  isr = ADC1->ISR;
  if (isr & ADC_ISR_EOC)           /* 上一周期触发的单通道转换已完成 */
  {
    d = (uint16_t)ADC1->DR;
    if (s_adc_ch_sel == 0)      /* 上一周期采 PA0：α=1/16 低通 */
    {
      if (s_pos_sm == 0U) s_pos_sm = d;
      else s_pos_sm = (uint16_t)(((uint32_t)s_pos_sm * 15U + d) / 16U);
      g_raw_pos = s_pos_sm;
    }
    else if (s_adc_ch_sel == 1) /* 上一周期采 PA1 */
    {
      if (s_neg_sm == 0U) s_neg_sm = d;
      else s_neg_sm = (uint16_t)(((uint32_t)s_neg_sm * 15U + d) / 16U);
      g_raw_neg = s_neg_sm;
    }
    else if (d > 100U)   /* 上一周期采 VREFINT：32 次累加平均 + 0.25 低通 → g_vref_volts（VDDA 跟踪） */
    {
      s_vref_acc += d;
      if (++s_vref_cnt >= VREF_AVG_N)
      {
        uint16_t avg = (uint16_t)(s_vref_acc / VREF_AVG_N);
        s_vref_acc = 0; s_vref_cnt = 0;
        float g_new = 3.0f * VREFINT_CAL_VAL / (float)avg;
        g_vref_volts += 0.25f * (g_new - g_vref_volts);   /* 低通：抑归一化步进，读数不跳 */
        if (g_vref_volts < 2.5f || g_vref_volts > 4.0f) g_vref_volts = VS_ADC_VREF;
      }
    }
  }

  ADC1->ISR = ADC_ISR_EOC | ADC_ISR_OVR;   /* 清标志（含 OVR，防抑制 EOC） */

  /* 轮询下一通道 */
  s_adc_ch_sel = (s_adc_ch_sel + 1) % 3;
  switch (s_adc_ch_sel)
  {
    case 0:  ADC1->SQR1 = ADC1_SQR1_SINGLE_CH1;  break;  /* 触发 PA0 */
    case 1:  ADC1->SQR1 = ADC1_SQR1_SINGLE_CH2;  break;  /* 触发 PA1 */
    default: ADC1->SQR1 = ADC1_SQR1_SINGLE_VREF; break;  /* 触发 VREFINT */
  }
  ADC1->CR |= ADC_CR_ADSTART;
}

/* 阻塞读当前通道 nsamp 次（先去 3 次弃采样等 ADC 稳定），去最大最小后平均，返回 counts。
   spread 可选：返回 nsamp 内 max−min（输入未稳定/悬空时会很大）。 */
static uint16_t adc_block_read_avg(uint32_t nsamp, uint16_t *spread)
{
  uint32_t sum = 0, t;
  uint16_t mn = 0xFFFF, mx = 0, v;
  int i;

  for (i = 0; i < 3; i++)                 /* 弃采样：等 ADC 稳定 */
  {
    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_OVR;
    ADC1->CR |= ADC_CR_ADSTART;
    t = 0;
    while (!(ADC1->ISR & ADC_ISR_EOC)) { if (++t > 200000U) break; }
    (void)ADC1->DR;
  }
  for (i = 0; i < (int)nsamp; i++)
  {
    ADC1->ISR = ADC_ISR_EOC | ADC_ISR_OVR;
    ADC1->CR |= ADC_CR_ADSTART;
    t = 0;
    while (!(ADC1->ISR & ADC_ISR_EOC)) { if (++t > 200000U) break; }
    v = (uint16_t)ADC1->DR;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    sum += v;
  }
  if (spread) *spread = (uint16_t)(mx - mn);
  return (uint16_t)((sum - mn - mx) / (nsamp - 2));
}

/* 零点=固定标定 VCM（VS_VCM_DEF=1.627V），读数直接减它。
   抓取的 PA0/PA1 计数仅作平滑起步（0.5ms 自收敛）与诊断显示（B=），不作零点。
   平滑为纯指数低通（无突变拒绝，防锁死）；异常抓取值回退名义值。 */
static void adc_capture_baseline(void)
{
  uint16_t base_pos, base_neg, spread;
  uint16_t nominal = (uint16_t)(VS_VCM_DEF * VS_ADC_BITS / VS_ADC_VREF);

  ADC1->SQR1 = ADC1_SQR1_SINGLE_CH1;
  base_pos = adc_block_read_avg(BL_N_SAMP, &spread);
  if (base_pos < 1500U || base_pos > 3400U || spread > 25U) base_pos = nominal;

  ADC1->SQR1 = ADC1_SQR1_SINGLE_CH2;
  base_neg = adc_block_read_avg(BL_N_SAMP, &spread);
  if (base_neg < 1500U || base_neg > 3400U || spread > 25U) base_neg = nominal;

  s_pos_sm = base_pos;               /* 平滑初值（仅起步，0.5ms 自收敛） */
  s_neg_sm = base_neg;
  g_base_pos_c = base_pos;           /* 诊断暴露 */
  g_base_neg_c = base_neg;

  s_base_pos_v = VS_VCM_DEF;         /* 零点=固定 1.627V */
  s_base_neg_v = VS_VCM_DEF;

  ADC1->ISR = ADC_ISR_EOC | ADC_ISR_OVR;
  s_adc_ch_sel = 0;
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

  /* VREFINT 动态标定前置（否则动态标定算出的参考错误 → 读数整体偏移）：
     ① ADC_CCR.VREFEN 必须置位，CH18 才真正连到 VREFINT 内部基准；
        工程原无任何 VREFEN 使能 → CH18 读到浮空杂散值（~1342 counts）
        → vref 被算成 ~3.7V → 0V 输入显示 -40V。这是本版根因。
     ② CH18 采样时间 12.5cyc(1.18μs) 不足，VREFINT 源阻抗高采不满 → 提到 47.5cyc(4.48μs)。
        转换总时长 47.5+12.5=60cyc≈5.66μs < REP 周期 9.3μs，时序安全。
        此刻 HRTIM 未启动、无转换在进行，写 CCR/SMPR2 安全。 */
  ADC12_COMMON->CCR |= ADC_CCR_VREFEN;   /* VREFEN 在 ADC12_COMMON（双 ADC 公共块），不是 ADC1->CCR */
  ADC1->SMPR2 = (ADC1->SMPR2 & ~ADC_SMPR2_SMP18_Msk)
              | ADC_SMPR2_SMP18_2;   /* CH18 47.5 cycles（G4 编码 0b100） */

  /* 上电抓取 PA0/PA1 计数：此刻 HRTIM 未启动、无 REP 抢占，阻塞读安全。
     两路输入必为 0V（未合母线）。结果仅作平滑初值 + B= 诊断显示；
     读数零点=固定 VS_VCM_DEF=1.627V，不依赖本次抓取（前端建立时间不定）。 */
  adc_capture_baseline();
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
  float vp, vn, vmeas;
  g_isr_cnt++;

  /* 电压采样（无忙等版）：读上一周期结果 + 触发本周期转换。
     VREFINT 对照诊断已证实 ADC 转换链路健康（CV 稳定），
     主循环读会被 REP 抢占→OVR→EOC 丢失，故必须 ISR 内采样。 */
  adc1_sample_isr();

  /* 电压换算（VREFINT 归一化 + 0V 慢速校零零点，2026-08-10 改版）：
     VADC = counts × g_vref/4096，g_vref = 3.0×VREFINT_CAL/VREFINT_counts
     —— 精确跟踪 VDDA：出厂标定与实测同载 VREFINT，ratio 抵消 ±1% 容差，
        参考漂移/抖动自动消除（实测：固定参考在 VDDA 抖 ±0.3% 时读数跳 0.8V，归一化后根除）。
     Vrail = (零点 − 当前VADC)/K。零点起点 VS_VCM_DEF=1.627V，但前端共模逐次上电
     会漂（实测 1.621~1.627，还出现过 2.15V），固定值追不上 → 由下方"0V 慢速校零"
     在确认 0V 时拉向当前真实共模。 */
  vp = (s_base_pos_v - (float)g_raw_pos * g_vref_volts / VS_ADC_BITS) / VS_K_POS;
  vn = (s_base_neg_v - (float)g_raw_neg * g_vref_volts / VS_ADC_BITS) / VS_K_NEG;
  vmeas = (vp - vn) * 0.5f;   /* 控制用快速测量（闭环阶段另行设计滤波器带宽） */

  /* 0V 慢速校零：门控 = |vmeas| 慢平均 < 0.5V。vmeas=(vp−vn)/2 中零点偏移精确抵消，
     故 0V 时门恒开（即使显示偏 ~1V）、一有输出电压立即关。门内把零点慢速拉向
     当前 VADC（=当前真实共模），τ≈0.5s 无跳变，多次读数平均掉噪声。前端共模漂移
     （逐次上电/起步/热漂）由此跟随；一出压即冻结不再动。 */
  s_vm_gate += (vmeas - s_vm_gate) * (1.0f / REZERO_AVG_N);
  if (s_vm_gate < REZERO_GATE_V && s_vm_gate > -REZERO_GATE_V)
  {
    s_base_pos_v += ((float)g_raw_pos * g_vref_volts / VS_ADC_BITS - s_base_pos_v) * (1.0f / REZERO_N);
    s_base_neg_v += ((float)g_raw_neg * g_vref_volts / VS_ADC_BITS - s_base_neg_v) * (1.0f / REZERO_N);
  }

  /* 显示慢低通：剩余 ±1V 是 >10Hz 测量噪声（万用表稳、ADC 跳——表内部积分把快噪声
     平均掉，ADC 31kHz 采样没平均）。参考归一化治不了它（噪声真实落在 counts 里），
     只能靠显示侧滤波。α=1/32768 per REP ≈ 344ms 时间常数：
     50Hz 噪声压 ~108×、100Hz ~216× → ±1V → ±0.01V；真实变化 ~1.6s 内跟上，与表一致。 */
  g_vpos = s_vp_disp += (vp - s_vp_disp) * (1.0f / DISP_LP_N);
  g_vneg = s_vn_disp += (vn - s_vn_disp) * (1.0f / DISP_LP_N);

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
