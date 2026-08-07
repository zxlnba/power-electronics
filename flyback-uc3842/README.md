# 反激开关电源 — 基于 UC3842 + 同步整流

## 概述

- **拓扑**：单端反激 (Flyback)，副边同步整流
- **控制器**：UC3842B，峰值电流模式 PWM
- **输入**：220V AC（整流后约 310V DC）
- **输出**：24V DC / 3A（额定 72W）
- **开关频率**：~52kHz（RT=10kΩ, CT=3.3nF）
- **反馈**：TL431 + PC817 隔离反馈，Type II 补偿
- **变压器**：PQ2620 (PC40)，三明治绕法，原边电感 27µH
- **同步整流**：UCC24612 驱动 AGM18N20D，降低副边整流损耗

> **📌 图片占位**：原理图全览
> `![原理图全览](images/schematic-overview.png)`

---

## 1. 设计规格

| 参数 | 数值 | 备注 |
|---|---|---|
| 输入电压 | 220V AC ±20% | 176–264V AC，整流后 200–375V DC |
| 输出 | 24V / 3A | 额定 72W，峰值 90W |
| 开关频率 | 52kHz | CCM/DCM 边界附近 |
| 效率目标 | ≥88% | 同步整流减少副边损耗 |
| 原边电感量 | 27µH ±5% | 兼顾磁芯利用率和纹波 |
| 漏感 | < 0.8µH | 三明治绕法控制 |

---

## 2. 变压器设计

### 2.1 磁芯选择

PQ2620，有效截面积 Ae=119mm²，窗口面积 Aw=60.4mm²。按 72W 输出、52kHz 频率，PQ2620 的 AP 值有充足裕量。

### 2.2 电感量

按 CCM 满载工作，取电流纹波率 r≈0.5（峰值电流约为平均值 1.25 倍）：

```
Ip_avg = Pin / Vbus_min = 82W / 250V ≈ 0.33A
ΔIp = r × Ip_avg = 0.5 × 0.33A ≈ 0.17A
Lp = Vbus_min × Dmax / (ΔIp × fs)
   = 250V × 0.45 / (0.17A × 52kHz) ≈ 27µH
```

### 2.3 匝比

根据反射电压 Vor 选取匝比。考虑 650V MOSFET 降额使用（80% 即 520V），漏感尖峰约 100V：

```
Vor ≤ Vds_max - Vbus_max - Vspike = 520V - 375V - 100V = 45V
n = Vor / (Vout + Vf) = 45V / (24V + 0.5V) ≈ 1.84
```

实际取 **n ≈ 6 : 1**（原边 30T / 副边 5T），辅助绕组 10T 供 VCC。

> **📌 图片占位**：变压器实物（PQ2620 三明治绕法）
> `![变压器](images/detail-transformer.jpg)`

---

## 3. RCD 吸收电路

### 3.1 漏感储能

漏感 Llk≈0.8µH，每个周期漏感储能：

```
E_lk = ½ × Llk × Ip_peak² = ½ × 0.8µH × (0.5A)² ≈ 0.1µJ
P_lk = E_lk × fs = 0.1µJ × 52kHz ≈ 5mW
```

实际漏感峰值电流可达 0.5–0.6A（含纹波），钳位功耗约 **0.1–0.3W**。

### 3.2 钳位电压

设钳位电压 Vclamp = 150V（反射电压为 6×(24+0.5)=147V，钳位比反射高约 2%）：

```
Vclamp - Vor = 150V - 147V ≈ 3V  ← 微小过钳位，最大程度回收漏感能量
```

### 3.3 参数选取

| 参数 | 值 | 依据 |
|---|---|---|
| 吸收电容 C1 | 2.2nF/1kV | 需在开关周期内保持电压稳定，ΔVc < 10% Vclamp |
| 放电电阻 R2, R3, R25 | 68kΩ + 1kΩ + 1kΩ (1206) | 每周期泄放 C1 储能，功率约 0.15W/电阻 |
| 钳位二极管 D2 位置 | — | 漏感能量通过 D2 回馈到母线，实际 RCD 接在 Q4 漏极与 VBUS 之间 |

### 3.4 RCD 工作过程

1. Q4 关断瞬间：漏感电流通过 D2 向 C1 充电，C1 电压上升到 Vclamp
2. C1 电压钳住漏极尖峰，幅值 = Vbus + Vclamp（约 310+150=460V，在 650V MOSFET 安全范围内）
3. Q4 开通期间：C1 通过 R2//R3//R25 放电回到稳态

> **📌 图片占位**：漏极电压波形（含 RCD 钳位效果对比）
> `![RCD钳位波形](images/waveform-vds-vgs.png)`

---

## 4. UC3842 控制电路

### 4.1 芯片简介

UC3842B 是经典峰值电流模式 PWM 控制器，关键特性：

- **启动阈值**：16V（UVLO ON），**关断阈值**：10V（UVLO OFF），6V 迟滞
- **5V 基准** (VREF)：±1% 精度，50mA 驱动能力
- **振荡器**：RT 接 VREF→CT 到 GND，频率 `f ≈ 1.72/(RT×CT)`
- **电流采样**：CS 脚 1V 阈值逐周期限流
- **图腾柱输出**：±1A 峰值驱动

### 4.2 频率设定

RT=10kΩ, CT=3.3nF：

```
f_osc ≈ 1.72 / (10kΩ × 3.3nF) ≈ 52kHz
```

### 4.3 启动过程

```
上电 → R24(10kΩ) 从 VBUS 向 C14(47µF) 充电
     → VCC 升至 16V → UC3842 启动，开始 PWM
     → 辅助绕组接手供电 → VCC 稳定在 ~14V
```

启动时间：`t_start ≈ C14 × 16V / (Vbus/R24) ≈ 47µF × 16V / 31mA ≈ 24ms`

辅助绕组 10T，输出电压约 14V（经 D2/FR107W 整流 + R26 限流），既保持 >10V（不掉 UVLO），又不触发 34V 过压保护。

### 4.4 限流保护

UC3842 CS 脚 1V 阈值。采样电阻 R8=80mΩ：

```
Ip_limit = 1V / 80mΩ = 12.5A（原边峰值）
```

对应满载峰值约 0.5A，裕量充足。CS 脚前的 RC 滤波 (R20=1kΩ, C16=470pF) 滤除前沿尖峰（LEB 辅助）。

> **📌 图片占位**：UC3842 外围电路
> `![UC3842控制电路](images/schematic-uc3842.png)`

---

## 5. 反馈环路设计

### 5.1 TL431 + PC817 隔离反馈

**工作原理**：输出电压经 R14(10kΩ) + R27(8.2kΩ) + R23(1kΩ) 分压到 TL431 REF（2.495V），TL431 与 2.495V 内部基准比较后驱动光耦 LED。二次侧反馈 → 光耦 → 一次侧 UC3842 COMP 脚，调节占空比。

### 5.2 电压设定

TL431 REF = 2.495V ±1%，上分压 (R14=10kΩ + R27=8.2kΩ)，下分压 (R23=1kΩ)：

```
Vout = 2.495V × (1 + (R14+R27)/R23)
     = 2.495V × (1 + 18.2kΩ/1kΩ)
     = 2.495V × 19.2 ≈ 47.9V
```

> ⚠️ 注：若以 24V 为目标输出，需调整分压电阻。24V 时 `(R14+R27)/R23 = 24/2.495-1 ≈ 8.62`，建议 R14=8.2kΩ, R27=NC, R23=1kΩ（或 R14=18kΩ, R27=NC, R23=2.2kΩ）。

### 5.3 Type II 补偿

C18=1nF 并联在 TL431 阴极-REF 之间，与 R21=10kΩ 构成单极点，滚降高频环路增益：

```
f_pole = 1 / (2π × R21 × C18) = 1 / (2π × 10kΩ × 1nF) ≈ 16kHz
```

光耦输出经 R17/R19 分压 + C12=10nF 滤波后馈入 UC3842 COMP 脚（Pin 1），C16=470pF 提供额外高频衰减。

### 5.4 光耦工作点

光耦 LED 电流由 R13(2.7kΩ) 偏置 + R18(1kΩ) 限流：

```
I_LED ≈ (Vout - Vf_TL431 - Vf_LED) / R13 ≈ (24V - 2.5V - 1.2V) / 2.7kΩ ≈ 7.5mA
```

PC817 CTR 范围 50%–600%，设计目标一次侧电流约 0.5–1mA（UC3842 COMP 脚灌电流能力）。

> **📌 图片占位**：反馈环路原理图
> `![反馈环路](images/schematic-feedback.png)`

---

## 6. 同步整流

### 6.1 为什么用同步整流

72W 输出时副边电流有效值约 3–4A。肖特基二极管 0.5V Vf 产生约 1.5–2W 损耗，占总损耗 ~25%。同步整流 MOSFET（AGM18N20D, 120mΩ）导通损耗：

```
P_SR = I² × Rds(on) = (4A)² × 0.12Ω ≈ 1.9W
```

似乎与肖特基相当？实际在轻载时 SR 优势更明显，且 MOSFET 发热面积大、散热更好。另外可选用更低 Rds(on) 的 MOSFET（如 20mΩ 级）进一步优化。

### 6.2 UCC24612 工作原理

UCC24612 是高精度 SR 控制器，通过检测 MOSFET 漏极电压（Vds）判断开关时机：

1. **开通检测**：Vds < -300mV（典型值），即体二极管导通 → 输出 GATE 高电平开通 MOSFET
2. **关断检测**：Vds 趋向 0V（电流过零），达到 -50mV 阈值 → 关闭 GATE
3. **比例驱动**：轻载时减小驱动电压，降低待机功耗
4. **振铃抑制**：检测到 DCM 振铃时锁定输出，防止误开通

### 6.3 并联肖特基

D6 (MBR20100CD, 100V/20A) 与 SR MOSFET 并联，作用：
- SR 关断瞬间续流，减少振铃
- SR 失效时提供冗余整流路径
- 体二极管导通时的反向恢复由肖特基分担

> **📌 图片占位**：同步整流区域 + SR Vds 波形
> `![同步整流](images/schematic-sync-rectifier.png)`

---

## 7. EMI 滤波

### 7.1 滤波器架构

```
AC输入 → F1(保险) → RF1(NTC) → L1(共模扼流圈) → CX1+CX2(X电容) → 整流桥
                                     ├── CY1+CY2(Y电容到PE)
                                     └── CY3+CY4(Y1跨原副边)
```

### 7.2 共模扼流圈

L1 为 UU10.5 磁芯，双线并绕 2×10mH。共模电感量 10mH 对 50Hz 工频阻抗可忽略，对 150kHz–30MHz 传导噪声提供高阻抗。

共模谐振频率由 L1 与 CY1+CY2(2nF) 决定：

```
f_cm = 1 / (2π × √(10mH × 2nF)) ≈ 35.6kHz
```

低于 150kHz（传导测试起点），确保全频段有效。

### 7.3 X/Y 电容

- **CX1, CX2**：100nF X2，275VAC，抑制差模噪声。CX1 前的泄放电阻 R1, R5 (470kΩ×2) 确保 1s 内断电后电压 < 60V（安规要求）
- **CY1, CY2**：1nF Y2，PE 接地，共模噪声旁路。Y2 电容 ≤ 4700pF（IEC 60384-14 对地漏电流限制）
- **CY3, CY4**：1nF Y1，原副边跨接。Y1 等级更高（≥ 400VAC），抵御原副边 3kV 隔离电压

### 7.4 输入保护

- **F1 (JK250-2000U)**：PTC 自恢复保险，250V/2A 保持。短路时自恢复，免更换
- **RF1 (5D-11)**：NTC 热敏电阻，冷态 5Ω，抑制电解电容充电浪涌。稳态热态 < 0.5Ω

> **📌 图片占位**：输入 EMI 滤波区域
> `![EMI滤波](images/schematic-input-emi.png)`

---

## 8. PCB 设计

> **📌 图片占位**：PCB 3D 视图
> `![PCB 3D视图](images/pcb-3d.png)`

> **📌 图片占位**：PCB Top Layer
> `![PCB Top Layer](images/pcb-top.png)`

> **📌 图片占位**：PCB Bottom Layer
> `![PCB Bottom Layer](images/pcb-bottom.png)`

---

## 9. 实物

### 9.1 整机

> **📌 图片占位**：样机正面
> `![样机正面](images/prototype-front.jpg)`

> **📌 图片占位**：样机侧面
> `![样机侧面](images/prototype-side.jpg)`

### 9.2 PCB

> **📌 图片占位**：PCB 空板
> `![PCB空板](images/pcb-bare.jpg)`

> **📌 图片占位**：焊接完成（正面）
> `![焊接完成正面](images/pcb-assembled-top.jpg)`

> **📌 图片占位**：焊接完成（背面）
> `![焊接完成背面](images/pcb-assembled-bottom.jpg)`

### 9.3 细节

> **📌 图片占位**：变压器特写（PQ2620）
> `![变压器特写](images/detail-transformer.jpg)`

> **📌 图片占位**：同步整流区域特写
> `![同步整流特写](images/detail-sync-rectifier.jpg)`

---

## 10. 测试

### 10.1 稳态波形

> **📌 图片占位**：Vds + Vgs（满载，验证开关特性与电压应力）
> `![Vds Vgs](images/waveform-vds-vgs.png)`

> **📌 图片占位**：Vds + 原边电流采样（验证逐周期限流与 CCM/DCM 边界）
> `![Vds Ipri](images/waveform-vds-ipri.png)`

> **📌 图片占位**：同步整流 Vds（验证 SR 导通/关断时序）
> `![SR Vds](images/waveform-sr-vds.png)`

> **📌 图片占位**：输出纹波（满载，AC 耦合，20MHz 带宽）
> `![输出纹波](images/waveform-ripple.png)`

### 10.2 动态响应

> **📌 图片占位**：负载阶跃（50%↔100%），VOUT 跌落/过冲 + 恢复时间
> `![动态响应](images/waveform-transient.png)`

### 10.3 启动与保护

> **📌 图片占位**：启动波形（VCC 爬升 + VOUT 建立）
> `![启动波形](images/waveform-startup.png)`

> **📌 图片占位**：输出短路保护（打嗝模式自动恢复）
> `![短路保护](images/waveform-short-circuit.png)`

### 10.4 效率

> **📌 图片占位**：效率-负载曲线（25%/50%/75%/100%），同步整流 vs 二极管整流对比
> `![效率曲线](images/efficiency-curve.png)`

---

## 11. 调试记录

### 11.1 启动打嗝

**现象**：上电后反复启动-关断。

**根因**：启动电阻 R24 偏大（22kΩ）→ VCC 充电电流不足，辅助绕组电压未建立前 VCC 跌破 UVLO。加大 C14 到 47µF、R24 改 10kΩ 解决。

### 11.2 TL431 vs TLV431

**现象**：输出电压偏离设计值。

**根因**：误用 TLV431 (Vref=1.24V) 替代 TL431 (Vref=2.495V)。更换后正常。

### 11.3 峰值限流误触发

**现象**：无法达到额定功率。

**根因**：电流采样电阻 75mΩ 偏大 → 改 80mΩ（并联调整），匹配峰值电流阈值。

### 11.4 测试链路压降

**现象**：负载端测量电压偏低。

**根因**：测试引线电阻 → 远端测量压降。更换低阻线后正常。

---

## 文件结构

```
flyback-uc3842/
├── README.md                       ← 本文件
└── images/
    ├── schematic-overview.png
    ├── schematic-input-emi.png
    ├── schematic-uc3842.png
    ├── schematic-feedback.png
    ├── schematic-sync-rectifier.png
    ├── pcb-3d.png
    ├── pcb-top.png
    ├── pcb-bottom.png
    ├── prototype-front.jpg
    ├── prototype-side.jpg
    ├── pcb-bare.jpg
    ├── pcb-assembled-top.jpg
    ├── pcb-assembled-bottom.jpg
    ├── detail-transformer.jpg
    ├── detail-sync-rectifier.jpg
    ├── waveform-vds-vgs.png
    ├── waveform-vds-ipri.png
    ├── waveform-sr-vds.png
    ├── waveform-ripple.png
    ├── waveform-transient.png
    ├── waveform-startup.png
    ├── waveform-short-circuit.png
    └── efficiency-curve.png
```
