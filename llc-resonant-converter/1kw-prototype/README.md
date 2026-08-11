# 1kW 原理样机开发

将 20kW 全桥 LLC 设计缩比至 1kW 进行原理验证，验证 LLC 谐振腔工作波形、ZVS 实现及闭环调压功能。

> ## 📌 当前状态（2026-08-11）
>
> | 项 | 状态 |
> |---|---|
> | 电压采样板 | ✅ 读数准确稳定 ±0.1V（VREFINT 归一化 + 0V 慢速校零，见 `docs/调试日志/`） |
> | 闭环固件 | ✅ 2p2z + 软启动 + 过流保护已落地并通过编译 |
> | 60V 低压测试 | ⚠️ **发现设计点不匹配**：现腔 Z0=181.5Ω 按额定 Q=0.35（设计书）设计，60V 可用负载按功率守恒折算 Q=2~8（物理口径），高 Q 无升压区 + 死区/二极管 ~20% 损耗 → 增益封顶 M≈0.8，60V 每路只能到 ~24-27V |
> | 60V 专用腔 方案A | 🔧 待执行：Lr=7µH / Cr=470nF / Lm=24µH（新变压器，fr≈87.7kHz，Z0≈3.86Ω）可解锁 60V 下 78-300W 功率阶梯 |
> | 额定 400V 测试 | ⏳ 需 360-440V 母线 + 160Ω/80Ω 负载（见 `docs/工程开发文档.md` §4） |
>
> **完整工程开发记录（含 60V 缩尺测试教训、损耗根因、60V 专用腔 方案A 设计）见
> [`docs/工程开发文档.md`](docs/工程开发文档.md)**，务必先读。

## 缩比参数

| 参数 | 20kW 设计 | 1kW 样机 |
|---|---|---|
| 输入电压 | 800V DC | 360–440V DC |
| 输出架构 | ±400V / 800V 可切换 | ±200V 双输出 / 400V 单输出 可切换 |
| 额定功率 | 20kW | 1kW |
| 谐振频率 | 107kHz | 107kHz（保持一致） |
| 变压器磁芯 | PC95 | PQ35/35 PC95 |

---

## 1. 原理图设计

原理图按功能划分为 5 个模块，高低压区域物理隔离，模块化布局。

> **📌 图片占位**：此处放原理图全览截图
> `![原理图全览](images/schematic-overview.png)`

### 1.1 母线输入保护（左下角）

输入端的第一道安全屏障，抑制启动冲击与 EMI。

| 器件 | 规格 | 功能 |
|---|---|---|
| 保险丝 F2 | 10A 熔断型 | 短路时切断回路 |
| NTC 热敏电阻 | — | 限制母线上电浪涌电流 |
| 共模电感 L5 + 差模电感 L6 | — | EMI 滤波 |
| X 电容 | — | 差模噪声抑制 |
| 压敏电阻 + SMBJ600A TVS | 600V 钳位 | 雷击/浪涌过压保护 |
| Si MOS Q8 + 电压钳位电路 | — | 防反接，限制体二极管导通损耗 |

> **📌 图片占位**：此处放输入保护区域截图
> `![输入保护电路](images/schematic-input-protection.png)`

### 1.2 全桥栅极驱动（中部）

接收 MCU PWM 信号，生成隔离驱动控制功率开关管。

| 器件 | 说明 |
|---|---|
| UCC21520 ×2 | 双通道隔离栅极驱动器，5kV 隔离耐压 |
| 驱动逻辑 | 对角导通：Q1+Q4 / Q2+Q3 交替，硬件死区 **250ns** + HRTIM 59ns（实测确认，见 hrtim.c）防桥臂直通 |
| 驱动电平 | +15V 开通 / -4V 关断，适用于 Si MOSFET，加速栅极电荷泄放 |

> **📌 图片占位**：此处放栅极驱动电路截图
> `![栅极驱动电路](images/schematic-gate-drive.png)`

### 1.3 LLC 功率主回路（中部，变换器核心）

直流→高频交流→变压器升压/降压→直流，完整功率变换链路。

| 环节 | 器件 | 参数 |
|---|---|---|
| 原边全桥逆变 | RS65R190T Si MOSFET ×4 | 650V/190mΩ，TO-220F 全塑封 |
| 谐振电感 Lr | — | 270μH |
| 谐振电容 Cr | — | 8.2nF |
| 高频变压器 | PQ35/35 PC95 磁芯 | 原边 28T / 副边 28T（14T+14T 中心抽头），**全绕组 1:1**；每路变换比 28:14=2:1（400V→±200V） |
| 副边整流 | RHRP860 快恢复二极管 ×4 | 600V/8A，TO-220AC |
| 输出架构切换 | 外部跳线 | 改变副边绕组连接方式，±200V 双输出 ↔ 400V 单输出 |

变压器采用中心抽头全波整流结构，原边 ZVS 软开关 + 副边 ZCS（fs ≤ fr 时二极管自然关断）。

> **📌 图片占位**：此处放功率主回路截图
> `![LLC功率主回路](images/schematic-power-stage.png)`

### 1.4 信号采集电路（右侧，闭环控制核心）

采集输出电压/电流，转换为 MCU 可识别的 0–3.3V 信号。

| 信号 | 器件 | 说明 |
|---|---|---|
| 输出电压采样 | AMC1350 ×2 | TI 隔离放大器，差分输出，精度采集高压侧电压 |
| 信号调理 | TLV9002 ×2 | 双运放，差分转单端 + 电平移位至 3.3V ADC 量程 + 抗混叠滤波 |
| 输出电流采样 | TMCS1101 ×2 | 霍尔效应电流传感器，50mV/A，隔离式，±2.5V 输出经电平移位进 ADC |

> **📌 图片占位**：此处放信号采集电路截图
> `![信号采集电路](images/schematic-signal-acquisition.png)`

### 1.5 辅助供电网络（左上角）

为控制、驱动、采样电路提供隔离稳压电源，与功率回路电气隔离。

```
外部 12V DC 输入
    ├── B1212S-2WR2  → 隔离 12V → 栅极驱动芯片 (UCC21520)
    ├── B1205S-2WR2  → 隔离 5V  → 信号采集电路 (AMC1350/TLV9002)
    ├── B1205S-2WR2  → 隔离 5V  → MCU 控制板
    └── AMS1117-5.0  → 低噪 5V  → 高精度模拟参考
```

3 路隔离 DC-DC 实现控制侧与功率侧电气隔离，AMS1117 线性稳压为 ADC 提供低纹波参考。

> **📌 图片占位**：此处放辅助供电网络截图
> `![辅助供电网络](images/schematic-aux-power.png)`

---

## 2. PCB 设计

> **📌 图片占位**：此处放 PCB 3D 视图 / Top Layer
> `![PCB 3D视图](images/pcb-3d.png)`

> **📌 图片占位**：此处放 PCB Top Layer 布线
> `![PCB Top Layer](images/pcb-top.png)`

> **📌 图片占位**：此处放 PCB Bottom Layer 布线
> `![PCB Bottom Layer](images/pcb-bottom.png)`

Gerber 文件：`pcb/Gerber_PCB1_2026-07-31.zip`

---

## 3. 实际样机

> **📌 图片占位**：此处放样机整机照片（正面）
> `![样机正面](images/prototype-front.jpg)`

> **📌 图片占位**：此处放样机整机照片（侧面/45°）
> `![样机侧面](images/prototype-side.jpg)`

> **📌 图片占位**：此处放 PCB 空板照片
> `![PCB空板](images/pcb-bare.jpg)`

> **📌 图片占位**：此处放焊接完成 PCB 照片
> `![PCB焊接完成](images/pcb-assembled.jpg)`

### 测试波形

> **📌 图片占位**：此处放上管 Vgs + Vds 波形（验证 ZVS）
> `![ZVS波形](images/waveform-zvs.png)`

> **📌 图片占位**：此处放谐振电流 + 谐振电容电压波形
> `![谐振波形](images/waveform-resonant.png)`

> **📌 图片占位**：此处放输出电压纹波
> `![输出纹波](images/waveform-ripple.png)`

> **📌 图片占位**：此处放负载阶跃动态响应波形
> `![动态响应](images/waveform-transient.png)`

---

## 4. 电压采样板

样机需要实时监测高压直流母线电压（±200V）和输出电流，为此设计了基于 STM32G474 的隔离采样板。

详见 [`voltage-sampling/`](./voltage-sampling/)

### 硬件架构

```
±200V → 分压电阻 → AMC1350 隔离放大器 → TLV9002 信号调理 → STM32G474 ADC → OLED
 电流 → TMCS1101 霍尔传感器 ────────────────────────────────────→ STM32G474 ADC
                                                                       → HRTIM PWM 输出
```

| 环节 | 器件 | 说明 |
|---|---|---|
| 电压隔离采样 | AMC1350 ×2 | ±5V 差分输入，5kV 隔离，对应 ±200V 量程 |
| 电流采样 | TMCS1101 ×2 | 50mV/A 霍尔传感器，隔离式 |
| 信号调理 | TLV9002 ×2 | 差分转单端 + 电平移位 0–3.3V + 抗混叠滤波 |
| MCU | STM32G474RBT6 | 12-bit ADC，170MHz，HRTIM |
| 显示 | 0.96" OLED | I2C，实时电压/电流 + 诊断信息 |

### 关键参数

| 参数 | 值 |
|---|---|
| 电压采样范围 | ±200V DC |
| 电流采样范围 | 0–20A（50mV/A） |
| ADC 分辨率 | 12-bit（4096 LSB） |
| ADC 时钟 | SYSCLK/16 = 10.6MHz（异步时钟方案） |
| 采样时间 | 12.5 周期（~2.4μs），HRTIM REP 中断内周期边界相位锁定，每开关周期可闭环 |
| VCM 基线 | 1.627V（0V 慢速校零起点，实测 10 点拟合） |
| 电压增益 K | 0.0058 V/V（两通道一致，10 点拟合；旧值 0.0066 已弃） |
| 参考电压 | **VREFINT 归一化**（精确跟踪 VDDA，抵消漂移/抖动） |
| 显示刷新 | 500ms |
| 平滑滤波 | 显示侧慢低通 344ms（`DISP_LP_N=32768`）+ 信号通道 α=1/16 |

### 固件关键问题

**ADC 时钟排障**：CubeMX 默认 PLL 同步时钟配置导致 G4 系列 ADRDY 不置位，HAL_ADC_Start 1ms 超时失败。根因：G4 ADC 时钟路径与 F1/F4 不同，需手动配置 `RCC_ADC12CLKSOURCE_SYSCLK` + `ADC_CLOCK_ASYNC_DIV16`。

**供电问题**：早期板子 VDDA 实测 2.7–2.8V（标称 3.3V），导致参考电压偏移、ADC 读数漂移。后更换稳定供电解决。

**校准**：`VS_VCM_DEF` 和 `VS_K` 在 `voltage_sample.h` 中定义为可调宏，适配分压电阻公差。

**OLED 显示布局**：
```
Vp: +150.2V     ← 正轨电压
Vn: -148.7V     ← 负轨电压
Ip:  3.52A      ← 正轨电流
In:  3.48A      ← 负轨电流
E:0             ← ADC 错误码 (0=正常)
```

### 固件文件

| 文件 | 功能 |
|---|---|
| `src/main.c` | 主循环：OLED 刷新、HRTIM 输出、100ms/64 样本平均 |
| `src/adc.c` | ADC1(电压×2) + ADC2(电流×2, TMCS1101) 初始化 |
| `src/voltage_sample.c` | 采样核心：原始值读取、平均、电压换算 |
| `src/voltage_sample.h` | `VS_VCM_DEF`、`VS_K` 等可调校准参数 |

### 调试与验证

- ADC 错误码 `E` 显示在 OLED 第 6 行：0=正常，5=时钟配置失败，6=Start 失败，7/8=轮询超时
- 可通过 Keil Watch 窗口观察全局变量：`g_vpos`、`g_vneg`、`g_adc_error`
- 0V 输入不归零时调整 `VS_VCM_DEF`，满量程不准时调整 `VS_K`

> **📌 图片占位**：此处放电压采样板实物照片
> `![电压采样板](images/voltage-sampling-board.jpg)`

> **📌 图片占位**：此处放 OLED 显示效果照片
> `![OLED显示](images/oled-display.jpg)`

---

## 5. 数字控制环路设计

样机的闭环调压采用 **PFM（变频）**：全桥固定 50% 占空比，通过 HRTIM TimerD 在
95–130kHz 范围调整开关频率，改变谐振腔增益来稳定 ±200V 输出。已按数字电源标准
流程完成 **环路建模 → 补偿器设计 → Tustin 离散化 → 闭环仿真**，2p2z 系数可直接
落入固件。

```
Gvd(s) = K_f·(1+s/ωz_lm) / ((1+s/ωp_out)·(1+s/(ωr·Qr)+s²/ωr²))
C(z)   = 2p2z，Ts = 1/fr = 9.35µs（每开关周期控制）
指标：fco ≈ 2kHz，PM ≈ 71°（含 1 拍延迟），GM ≈ 43dB
      K_f = -0.505 V/kHz（负载无关，m=Lm/Lr=7.4），2p2z 系数见 llc_ctrl.h
```

详见 [`control-loop/`](./control-loop/)（设计推导 + MATLAB 脚本 + 闭环验证 + PLECS 验证步骤）

> **⚠️ 关键设计发现**：满载下 FHA 增益峰值 M_max = 1.034（@fn=0.888，fs_min 频带内，
> 设计书 Q=0.35），360V 输入需 M = 1.111，**满载低输入时无法维持 ±200V**（输出随输入
> 跌落），控制器会饱和到 fs_min；轻载受 fs_min 限制可达约 1.037。应对与后续优化见
> [`control-loop/README.md`](./control-loop/README.md) §3。

---

## 文件结构

```
1kw-prototype/
├── README.md                                    ← 本文件（样机总览 + 当前状态）
├── docs/                                        ← 📁 工程文档集中目录（2026-08-11 重组）
│   ├── 工程开发文档.md                            ← 🔥 完整工程开发记录（教训 + 60V 专用腔 方案A，先读）
│   ├── 1kW高压隔离LLC变换器原理图说明.docx        ← 原理图设计说明文档
│   ├── 缩尺方案与闭环固件测试报告.docx             ← 缩尺测试 Word 报告
│   └── 调试日志/
│       ├── 00_ADC电压显示调试记录.docx            ← ADC 排障详细记录
│       └── 电压采样板调试全流程日志.md             ← 采样板调试全流程（含 0V 慢速校零定论）
├── pcb/
│   └── Gerber_PCB1_2026-07-31.zip               ← PCB Gerber 文件
├── voltage-sampling/                            ← 电压采样板固件
│   ├── README.md
│   ├── llc_board.ioc                            ← CubeMX 工程
│   └── src/
│       ├── main.c / main.h
│       ├── adc.c / adc.h                        ← ADC1 电压 + ADC2 电流，采样 12.5 周期
│       ├── voltage_sample.c / voltage_sample.h  ← 电压换算（AMC1350，反相公式，VREFINT 归一化 + 0V 慢速校零）
│       ├── current_sample.c / current_sample.h  ← 电流换算（TMCS1101 ×2）
│       ├── llc_ctrl.c / llc_ctrl.h              ← 数字电压环（2p2z + 软启动 + 过流锁存）
│       └── (stm32g4xx_it.c / hrtim.c 见桌面 Keil 工程，HRTIM REP 中断做控制节拍)
├── control-loop/                                ← 控制环路设计 + 理论仿真
│   ├── README.md                                ← 设计文档（模型推导+验证）
│   ├── llc_loop_design.m                        ← MATLAB 环路设计脚本（可复现）
│   ├── llc_theory.m                             ← FHA 增益理论 + 时域解（回答"低于谐振是否升压"）
│   ├── llc_rescale.m                            ← 60V 专用腔 方案A 设计脚本
│   ├── llc_sim.m                                ← 时域仿真 + 损耗灵敏度（复现实测）
│   ├── scaled-low-voltage-test-plan.md          ← 60V 缩尺测试方案（方案A 专用腔）
│   ├── tutorial/                                ← 交互式 LLC 教学页
│   └── images/
│       ├── fha-gain-curves.png
│       ├── loop-bode.png
│       ├── closed-loop-step.png
│       └── fs-command.png
└── images/                                      ← 图片目录（待补充）
    ├── schematic-overview.png
    ├── schematic-input-protection.png
    ├── schematic-gate-drive.png
    ├── schematic-power-stage.png
    ├── schematic-signal-acquisition.png
    ├── schematic-aux-power.png
    ├── pcb-3d.png
    ├── pcb-top.png
    ├── pcb-bottom.png
    ├── prototype-front.jpg
    ├── prototype-side.jpg
    ├── pcb-bare.jpg
    ├── pcb-assembled.jpg
    ├── waveform-zvs.png
    ├── waveform-resonant.png
    ├── waveform-ripple.png
    ├── waveform-transient.png
    ├── voltage-sampling-board.jpg
    └── oled-display.jpg
```
