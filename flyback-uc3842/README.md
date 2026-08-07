# 反激开关电源 — 基于 UC3842 + 同步整流

## 概述

- **拓扑**：单端反激 (Flyback)，副边同步整流
- **控制器**：UC3842B（SOP-8，峰值电流模式 PWM）
- **输入**：220V AC
- **输出**：24V DC（额定 72W / 3A）
- **开关频率**：由 RT/CT 设定（C17=3.3nF, R22=10kΩ）
- **反馈**：TL431 + PC817 隔离反馈
- **变压器**：PQ2620，三明治绕法
- **同步整流**：UCC24612-1DBVR 驱动 AGM18N20D MOSFET

> **📌 图片占位**：此处放原理图全览截图
> `![原理图全览](images/schematic-overview.png)`

## 性能指标

| 指标 | 数值 | 备注 |
|---|---|---|
| 效率 | — | 待实测 |
| 输出纹波 | — | 待实测 |
| 负载调整率 | — | 待实测 |
| 原边电感量 | 27µH ± 5% | |
| 漏感 | 0.8µH (< 3%) | |

---

## 1. 原理图设计（68 个元件，38 个网络）

原理图分为 5 个功能区域，自左向右依次为：输入保护/EMI → 整流滤波 → 反激功率级 + UC3842 控制 → 同步整流 → 输出滤波。

### 1.1 输入保护与 EMI 滤波

| 位号 | 器件 | 规格 | 功能 |
|---|---|---|---|
| F1 | JK250-2000U | PTC 自恢复保险，250V/2A 保持 | 过流保护 |
| RF1 | 5D-11 | NTC 热敏电阻 | 抑制上电浪涌电流 |
| L1 | UU10.5-10mH | 共模电感，2×10mH | 共模噪声抑制 |
| CX1, CX2 | 100nF X2 | 275VAC，MPX/MKP | 差模噪声抑制 |
| CY1, CY2 | 1nF Y2 | 250VAC | 共模噪声到 PE |
| CY3, CY4 | 1nF Y1 | 400VAC | 原副边跨接 Y 电容 |
| R1, R5 | 470kΩ (1206) | 泄放电阻 | 断电后 X 电容放电 |
| D1 | MSB40M | 1kV/4A 整流桥 | 全桥整流 |

> **📌 图片占位**：输入保护与 EMI 滤波区域截图
> `![输入保护与EMI滤波](images/schematic-input-emi.png)`

### 1.2 整流滤波与启动

| 位号 | 器件 | 规格 | 功能 |
|---|---|---|---|
| C2 | 150µF/400V | 铝电解，18×35mm | 母线储能滤波 |
| R2, R3, R25 | 68kΩ + 1kΩ + 1kΩ (1206) | — | VBUS 电压检测分压（可选） |
| C1 | 2.2nF/1kV | X7R, 1206 | 原边 RCD 吸收电容 |
| D2 | FR107W | 1kV/1A, 500ns | VCC 辅助绕组整流 |
| R26 | 10Ω (1206) | — | VCC 整流限流 |
| R24 | 10kΩ (1206) | — | 高压启动电阻（VBUS → VCC） |

> **📌 图片占位**：整流滤波区域截图
> `![整流滤波](images/schematic-rectifier.png)`

### 1.3 UC3842 控制器与功率级

#### 控制核心

| 位号 | 器件 | 功能 |
|---|---|---|
| U3 | UC3842B (SOP-8) | 峰值电流模式 PWM 控制器 |

#### UC3842 外围配置

| 位号 | 规格 | 连接引脚 | 功能 |
|---|---|---|---|
| C17 | 3.3nF | RT/CT (Pin 4) | 振荡器定时电容 |
| R22 | 10kΩ | RT/CT (Pin 4) | 振荡器定时电阻 |
| C16 | 470pF | COMP (Pin 1) | 误差放大器补偿 |
| C14 | 47µF/35V | VCC (Pin 7) | VCC 储能滤波 |
| C15 | 100nF | VCC (Pin 7) | VCC 高频退耦 |
| C11 | 1µF | VREF (Pin 8) | 5V 基准退耦 |
| R20 | 1kΩ | ISENSE (Pin 3) | 电流采样滤波 |
| C16 | 470pF | COMP (Pin 1) | 环路补偿 |

#### 功率开关

| 位号 | 器件 | 规格 | 功能 |
|---|---|---|---|
| Q4 | NCEP1520K | N-MOS, TO-252 | 原边主功率开关 |
| R8 | 80mΩ (1206) | 采样电阻 | 原边峰值电流检测 |
| R7 | 10kΩ | — | ISENSE 分压 |
| R6 | 20Ω | — | 栅极驱动电阻 |
| D4 | 1N4148W | 100V/150mA | 栅极保护（加速关断） |

> **📌 图片占位**：UC3842 控制区域截图
> `![UC3842控制电路](images/schematic-uc3842.png)`

### 1.4 变压器

| 参数 | 数值 |
|---|---|
| 磁芯 | PQ2620 (PC40) |
| 原边电感量 | 27µH ± 5% |
| 漏感 | 0.8µH (< 3%) |
| 绕制工艺 | 三明治绕法 |
| 辅助绕组 | 为 VCC 供电（经 D2 整流） |

> **📌 图片占位**：变压器附近原理图截图
> `![变压器](images/schematic-transformer.png)`

### 1.5 副边同步整流

采用 TI UCC24612 同步整流控制器，取代传统肖特基二极管，大幅降低整流损耗。

| 位号 | 器件 | 规格 | 功能 |
|---|---|---|---|
| U1 | UCC24612-1DBVR | SOT-23-5, 4–28V | SR 控制器，检测漏极电压控制 MOSFET |
| Q2 | AGM18N20D | 200V/18A, 120mΩ, TO-252 | SR MOSFET |
| R11 | 10Ω | — | SR 栅极驱动电阻 |
| R15 | 20Ω | — | SR 补偿 |
| C10 | 1µF | — | SR 补偿/退耦 |
| D6 | MBR20100CD | 100V/20A, TO-252 共阴双管 | 并联肖特基（SR MOSFET 关断期间续流） |
| R9, C8 | NC (未贴) | — | SR 吸收电路预留位 |

> **📌 图片占位**：副边同步整流区域截图
> `![同步整流](images/schematic-sync-rectifier.png)`

### 1.6 反馈环路（TL431 + PC817）

标准 Type II 补偿的隔离反馈网络。

| 位号 | 器件 | 规格 | 功能 |
|---|---|---|---|
| U2 | PC817AS | SOP-4, CTR 可调 | 隔离光耦，反馈信号传输 |
| U4 | TL431AIDBZR | SOT-23-3, 2.495V ±1% | 基准 + 误差放大器 |
| R14 | 10kΩ | — | 输出电压上分压电阻 |
| R27 | 8.2kΩ | — | 输出电压中分压电阻 |
| R23 | 1kΩ | — | 输出电压下分压电阻 |
| R13 | 2.7kΩ | — | 光耦 LED 偏置电阻 |
| R18 | 1kΩ | — | 光耦 LED 限流 |
| R21 | 10kΩ | — | TL431 偏置 |
| C18 | 1nF | — | TL431 补偿电容 |
| C13 | 100nF | — | 光耦输出滤波 |
| R17 | 1kΩ | — | UC3842 FB 引脚上拉/滤波 |
| R19 | 1kΩ | — | FB 网络 |
| C12 | 10nF | — | FB 滤波 |

> **📌 图片占位**：反馈环路区域截图
> `![反馈环路](images/schematic-feedback.png)`

### 1.7 输出滤波与指示

| 位号 | 器件 | 规格 | 功能 |
|---|---|---|---|
| C3, C4 | 220µF/35V | 铝电解，6.3mm×高 | 输出滤波储能 |
| C5 | 2.2µF (X5R) | 0805 | 输出高频退耦 |
| C6 | 1µF (X7R) | 0805 | 输出退耦 |
| C7 | 100nF (X7R) | 0805 | 输出高频退耦 |
| C19 | 10µF (X5R) | 0805 | 输出退耦 |
| R4 | 5.1kΩ | — | 输出假负载（最小负载保持） |
| LED1 | NCD0805R1 | 红色 0805 | 输出指示灯 |
| R12 | 1kΩ | — | LED 上偏置 |
| R10 | 1kΩ | — | LED 下偏置/分压 |

### 1.8 测试点

| 位号 | 名称 | 连接 | 用途 |
|---|---|---|---|
| vg | 栅极测试点 | Q4 Gate (N$13) | 示波器观测栅极驱动波形 |
| vs | 源极/CS 测试点 | Q4 Source / R8 (IS) | 示波器观测电流采样波形 |

---

## 2. BOM 汇总

### 半导体

| 位号 | 器件 | 封装 | 厂家 | LCSC |
|---|---|---|---|---|
| U3 | UC3842B | SOP-8 | UMW | C347459 |
| U1 | UCC24612-1DBVR | SOT-23-5 | TI | C2657796 |
| U2 | PC817AS | SOP-4 | AOTE | C22375537 |
| U4 | TL431AIDBZR | SOT-23-3 | TI | C23892 |
| Q4 | NCEP1520K | TO-252 | 无锡新洁能 | C341717 |
| Q2 | AGM18N20D | TO-252 | AGMSEMI | C22386156 |
| D1 | MSB40M | UMSB | MDD | C173549 |
| D2 | FR107W | SOD-123FL | R+O | C18199114 |
| D4 | 1N4148W | SOD-123 | R+O | C7420318 |
| D6 | MBR20100CD | TO-252AB | AnBon | C397636 |
| LED1 | NCD0805R1 | 0805 | 国星光电 | C84256 |

### 电容

| 位号 | 规格 | 封装 | 厂家 | LCSC |
|---|---|---|---|---|
| C2 | 150µF/400V | 18×35mm 插件 | JIERR | C49256834 |
| C3, C4 | 220µF/35V | 6.3mm 插件 | SHENGYANG | C44606503 |
| C14 | 47µF/35V | 5×11mm 插件 | JIERR | C47344181 |
| C19 | 10µF/50V X5R | 0805 | SAMSUNG | C2932476 |
| C9, C5 | 2.2µF/50V X5R | 0805 | SAMSUNG | C377773 |
| C11, C6, C10 | 1µF/50V X7R | 0805 | SAMSUNG | C28323 |
| C7, C13, C15 | 100nF/50V X7R | 0805 | YAGEO | C49678 |
| C12 | 10nF/50V X7R | 0805 | SAMSUNG | C1710 |
| C17 | 3.3nF (2.2nF) | 0805 | SAMSUNG | C28260 |
| C1 | 2.2nF/1kV X7R | 1206 | FH | C9191 |
| C18 | 1nF/50V X7R | 0805 | SAMSUNG | C46653 |
| C16 | 470pF/50V X7R | 0805 | FH | C1743 |
| CX1, CX2 | 100nF X2 | 插件 | KNSCHA | C489079 |
| CY1, CY2 | 1nF Y2 | 插件 | Dersonic | C2761722 |
| CY3, CY4 | 1nF Y1 | 插件 | Dersonic | C2974845 |
| C8 | NC | 0805 | — | —

### 电阻

| 位号 | 规格 | 封装 | 功能 |
|---|---|---|---|
| R1, R5 | 470kΩ | 1206 | X 电容泄放 |
| R2 | 68kΩ | 1206 | VBUS 分压 |
| R3, R25 | 1kΩ | 1206 | VBUS 分压 |
| R24 | 10kΩ | 1206 | 高压启动 |
| R26 | 10Ω | 1206 | VCC 限流 |
| R8 | 80mΩ | 1206 | 电流采样 |
| R6, R15 | 20Ω | 0805 | 栅极驱动 |
| R11 | 10Ω | 0805 | SR 栅极驱动 |
| R20, R17, R19, R18, R12, R10 | 1kΩ | 0805 | 各功能 |
| R23 | 1kΩ | 0805 | 分压下电阻 |
| R13 | 2.7kΩ | 0805 | 光耦偏置 |
| R4 | 5.1kΩ | 0805 | 假负载 |
| R7, R22, R14, R21 | 10kΩ | 0805 | 各功能 |
| R27 | 8.2kΩ | 0805 | 分压中电阻 |
| R9 | NC | 0805 | SR 吸收预留 |

---

## 3. PCB 设计

> **📌 图片占位**：此处放 PCB 3D 视图
> `![PCB 3D视图](images/pcb-3d.png)`

> **📌 图片占位**：此处放 PCB Top Layer 布线
> `![PCB Top Layer](images/pcb-top.png)`

> **📌 图片占位**：此处放 PCB Bottom Layer 布线
> `![PCB Bottom Layer](images/pcb-bottom.png)`

---

## 4. 实物

### 4.1 整机

> **📌 图片占位**：样机整机照片（正面）
> `![样机正面](images/prototype-front.jpg)`

> **📌 图片占位**：样机整机照片（侧面/45°）
> `![样机侧面](images/prototype-side.jpg)`

### 4.2 PCB

> **📌 图片占位**：PCB 空板照片
> `![PCB空板](images/pcb-bare.jpg)`

> **📌 图片占位**：焊接完成 PCB 照片（正面）
> `![PCB焊接正面](images/pcb-assembled-top.jpg)`

> **📌 图片占位**：焊接完成 PCB 照片（背面）
> `![PCB焊接背面](images/pcb-assembled-bottom.jpg)`

### 4.3 关键细节

> **📌 图片占位**：变压器特写（PQ2620 三明治绕法）
> `![变压器特写](images/detail-transformer.jpg)`

> **📌 图片占位**：同步整流区域特写（UCC24612 + AGM18N20D + MBR20100CD）
> `![同步整流特写](images/detail-sync-rectifier.jpg)`

---

## 5. 测试

### 5.1 稳态波形

> **📌 图片占位**：Vds + Vgs 波形（满载，验证开关特性与电压应力）
> `![Vds Vgs](images/waveform-vds-vgs.png)`

> **📌 图片占位**：Vds + 原边电流采样波形（验证逐周期限流与 CCM/DCM 边界）
> `![Vds Ipri](images/waveform-vds-ipri.png)`

> **📌 图片占位**：副边同步整流 Vds 波形（验证 SR 导通/关断时序）
> `![SR Vds](images/waveform-sr-vds.png)`

> **📌 图片占位**：输出纹波（满载，AC 耦合，20MHz 带宽）
> `![输出纹波](images/waveform-ripple.png)`

### 5.2 动态响应

> **📌 图片占位**：负载阶跃响应（50%↔100% 负载切换，VOUT 跌落/过冲）
> `![动态响应](images/waveform-transient.png)`

### 5.3 启动与保护

> **📌 图片占位**：启动波形（VCC 上电爬升 + VOUT 建立过程）
> `![启动波形](images/waveform-startup.png)`

> **📌 图片占位**：输出短路保护波形（验证打嗝模式恢复）
> `![短路保护](images/waveform-short-circuit.png)`

### 5.4 效率曲线

> **📌 图片占位**：效率-负载曲线（25%/50%/75%/100% 负载，标注同步整流 vs 二极管整流对比）
> `![效率曲线](images/efficiency-curve.png)`

---

## 6. 调试记录

### 5.1 启动打嗝问题

**现象**：上电后输出反复启动-关断（打嗝模式）。

**排查过程**：
1. 启动电阻偏大（22kΩ），VCC 充电电流不足 → 改为 10kΩ
2. VCC 电容容量不足 → 加大 C14 到 47µF
3. 辅助绕组匝数偏低（7T），VCC 供电电压低于 UVLO 阈值 → 增加到 10T

### 5.2 基准电压偏移（TL431 vs TLV431）

**现象**：输出电压偏离设计值。

**根因**：误将 TLV431 (Vref=1.24V) 当成 TL431 (Vref=2.5V) 使用，更换为正确的 TL431AIDBZR (Vref=2.495V ±1%) 后正常。

### 5.3 逐周期限流误触发

**现象**：无法达到额定功率，提前进入限流保护。

**根因**：电流采样电阻 75mΩ 偏大 → 改为 80mΩ（并联 200mΩ 电阻得到），峰值电流阈值匹配目标功率。

### 5.4 测试链路压降

**现象**：负载端测量电压偏低。

**根因**：测试引线电阻导致远端测量压降，更换低阻测试线后正常。

---

## 文件结构

```
flyback-uc3842/
├── README.md                         ← 本文件
└── images/                           ← 图片目录（待补充）
    ├── schematic-overview.png        ← 原理图全览
    ├── schematic-input-emi.png       ← 输入保护与EMI滤波
    ├── schematic-rectifier.png       ← 整流滤波
    ├── schematic-uc3842.png          ← UC3842控制电路
    ├── schematic-transformer.png     ← 变压器
    ├── schematic-sync-rectifier.png  ← 同步整流
    ├── schematic-feedback.png        ← 反馈环路
    ├── pcb-3d.png                    ← PCB 3D视图
    ├── pcb-top.png                   ← PCB Top Layer
    ├── pcb-bottom.png                ← PCB Bottom Layer
    ├── prototype-front.jpg           ← 样机正面
    ├── prototype-side.jpg            ← 样机侧面
    ├── pcb-bare.jpg                  ← PCB空板
    ├── pcb-assembled-top.jpg         ← 焊接完成(正面)
    ├── pcb-assembled-bottom.jpg      ← 焊接完成(背面)
    ├── detail-transformer.jpg        ← 变压器特写
    ├── detail-sync-rectifier.jpg     ← 同步整流特写
    ├── waveform-vds-vgs.png          ← Vds+Vgs波形
    ├── waveform-vds-ipri.png         ← Vds+原边电流
    ├── waveform-sr-vds.png           ← 同步整流Vds
    ├── waveform-ripple.png           ← 输出纹波
    ├── waveform-transient.png        ← 动态响应
    ├── waveform-startup.png          ← 启动波形
    ├── waveform-short-circuit.png    ← 短路保护
    └── efficiency-curve.png          ← 效率曲线
```
