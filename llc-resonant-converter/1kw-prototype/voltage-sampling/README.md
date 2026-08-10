# 双路电压采样板 — STM32G474

±200V 高压直流电压采样，用于 LLC 谐振变换器母线电压实时监测。

## 硬件架构

| 环节 | 器件 | 说明 |
|---|---|---|
| 隔离采样 | AMC1350 | 差分隔离放大器，±5V 输入范围 |
| 信号调理 | TLV9002 | 单端转换 + 电平移位 |
| MCU | STM32G474RBT6 | 12-bit ADC，HRTIM 互补 PWM |
| 显示 | OLED | I2C，实时显示正/负轨电压 + RAW 值 + 错误码 |

## 关键参数

| 参数 | 值 |
|---|---|
| 采样范围 | ±200V DC |
| ADC 分辨率 | 12-bit |
| ADC 时钟 | SYSCLK / 16 = 10.6MHz |
| 采样时间 | 12.5 周期 (~2.4µs)，供 HRTIM REP 周期中断每开关周期闭环 |
| 基线 VCM | **1.627V**（0V 零点，固定标定值，用户定；由"0V 慢速校零"修正到当前真实共模） |
| 增益 K | **0.0058 V/V**（两通道一致，2026-08-10 十点实测拟合） |
| 参考电压 | **VREFINT 归一化**（`g_vref=3.0×VREFINT_CAL/VREFINT_counts`，精确跟踪 VDDA；出厂标定 vs 实测同载 VREFINT，ratio 抵消 ±1% 容差与漂移/抖动） |
| 显示刷新 | 500ms |
| 平滑滤波 | 显示侧慢低通 344ms（`DISP_LP_N=32768` per REP）+ 信号通道 α=1/16 |

## 固件关键设计

### ADC 时钟排障

CubeMX 生成的 PLL + 同步时钟配置导致 ADRDY 不置位，HAL 1ms 超时。

**修复**：改 `RCC_ADC12CLKSOURCE_SYSCLK` + `ADC_CLOCK_ASYNC_DIV16`。

### 启动诊断

`vs_adc_startup_diag()` 手动使能 ADC 并轮询 ADRDY（最长 50ms），记录就绪耗时和设备 ID，通过全局变量 `g_diag_adrdy`、`g_diag_elapsed`、`g_diag_devid` 暴露诊断信息。

### 供电问题

早期板子 3.3V/5V 供电偏低且浮动（VDDA 实测 2.7-2.8V），导致参考电压错误、读数漂移。最终使用稳定供电，固件按标准 3.3V 参考。

### 2026-08-10 测量精确化（读数准确稳定 ±0.1V）

把电压读数从"能动"追到"准确稳定"的完整过程记录在
[`../../调试日志/电压采样板调试全流程日志.md`](../../调试日志/电压采样板调试全流程日志.md)
与 `LLC测试步骤/00_ADC电压显示调试记录.docx`。要点：

1. **单通道交替采样（核心修复）**：扫描模式 2 通道一次 ADSTART 连续转完，ch2 完成时
   ch1 的 DR 未读 → OVR 置位 → G4 中 OVR 挂起时 EOC 不再置位 → 永远读不到（V 恒 0000）。
   修复：每次 REP 只触发单通道（~2.4µs），下周期进 ISR 时必已转完，读 DR + 清 OVR +
   切另一通道。⚠️ G4 SQR1：L[3:0] 在 bit0-3、SQ1[4:0] 在 bit6-10，写 0x1 仍是双通道扫描，
   必须 `(ch<<6)`。每 3 个 REP 轮询 PA0 → PA1 → VREFINT(CH18)。
2. **VREFINT 归一化**：`g_vref = 3.0×VREFINT_CAL/VREFINT_counts`（32 次累加 + 0.25 低通），
   `VADC = counts×g_vref/4096`。ratio 抵消 ±1% 容差，VDDA 漂移/抖动（实测 3.183→3.16V）
   自动消除。⚠️ 别把 VREFINT 当绝对参考（±1% ÷K ≈ ×88 → 0V 偏 2V）。
3. **0V 慢速校零（最终方案）**：零点起点 `VS_VCM_DEF=1.627V`，但 `|vmeas|<0.5V`（vmeas=(vp−vn)/2
   中零点偏移精确抵消，0V 恒开/出压即关）时把零点慢速拉向当前真实共模（=当前 VADC），
   τ≈0.5s 无跳变。自动跟随逐次上电/起步/热漂的前端共模（实测 1.626 与 2.15V 两态），
   一出压冻结。
4. **显示慢低通**：`g_vpos = s_vp_disp += (vp−s_vp_disp)/32768` per REP ≈ 344ms，
   50Hz 噪声压 ~108×，真实变化 ~1.6s 跟上。vmeas 快路径留给闭环另行滤波。
5. **G4 ADC 校准不可省**：`HAL_ADCEx_Calibration_Start`，省了基线漂移/偶发 EOC 丢失。

### 关键校准旋钮

`voltage_sample.h` 中：
- `VS_VCM_DEF`：0V 零点起点（默认 **1.627V**，用户定），由 0V 慢速校零修正到真实共模
- `VS_K` / `VS_K_POS` / `VS_K_NEG`：增益（**0.0058** V/V，两通道一致，10 点拟合）
- `VS_ADC_VREF`：`g_vref` 初值/钳制兜底（**3.16V** 实测派生；归一化后对读数影响趋零）

若 0V 输入读数不归零，说明该次上电前端共模落在固定零点之外——这是硬件特性
（前端共模逐次上电不同），0V 慢速校零在确认 0V 后会自动修正。

## 软件结构

```
voltage-sampling/
├── NEW YEAR.ioc              ← CubeMX 工程文件
├── README.md
└── src/
    ├── main.c / main.h       ← 主程序：OLED 显示、HRTIM 输出、自动软启动/开环扫描、过流判定
    ├── adc.c / adc.h         ← ADC1 电压 + ADC2 电流（采样 12.5 周期）
    ├── voltage_sample.c/.h   ← 电压换算（AMC1350，反相公式 Vrail=(VCM-VADC)/K）
    ├── current_sample.c/.h   ← 电流换算（TMCS1101 ×2）
    └── llc_ctrl.c / llc_ctrl.h ← 数字电压环：2p2z + 软启动状态机 + 过流锁存
                                   （HRTIM TimerD REP 中断为控制节拍，每开关周期）
```

> 实际 Keil 工程在 `D:\桌面\超级档案\voltage_sampling`（含 CubeMX 全套外设
> hrtim.c / stm32g4xx_it.c / OLED.c 等）。本目录是固件源文件镜像，供仓库引用与
> 版本管理。闭环固件详见
> [`../control-loop/scaled-low-voltage-test-plan.md`](../control-loop/scaled-low-voltage-test-plan.md)。

## 调试与验证

- ADC 错误码 `E` 显示在 OLED 第 4 行：0=正常，6=Start 失败，7/8=轮询超时
- RAW 值显示在 OLED 第 3 行，用于诊断 ADC 原始读数
- 关键寄存器可通过 Keil Watch 窗口观察：`g_vpos`、`g_vneg`、`g_adc_error`
