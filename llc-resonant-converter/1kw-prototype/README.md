# 1kW 原理样机开发

将 20kW 全桥 LLC 设计缩比至 1kW 进行原理验证。

## 缩比参数

| 参数 | 20kW 设计 | 1kW 样机 |
|---|---|---|
| 输入电压 | 800V DC | 200V DC |
| 输出 | ±400V / 800V | ±100V / 200V |
| 谐振频率 | 107kHz | 107kHz（保持一致） |
| 变压器变比 | 800:400:400 | 200:100:100 |

缩比保持谐振频率不变，验证 LLC 谐振腔工作波形、ZVS 实现、闭环调压功能。

## 原理图设计

`1kW高压隔离LLC变换器原理图说明.docx` — 包含：

- 全桥逆变驱动电路（SiC MOSFET + 隔离栅极驱动）
- LLC 谐振腔与高频变压器
- 副边全桥整流与输出滤波
- 辅助供电（控制板、驱动、采样供电）
- 采样与保护电路（电压、电流采样 + 硬件过流保护）

## 电压采样板

LLC 样机需要实时监测高压直流母线电压（±100V），为此设计了 STM32G474 双路隔离电压采样板。

详见 [`voltage-sampling/`](./voltage-sampling/)

### 硬件链路

```
±100V → 分压电阻 → AMC1350 隔离放大器 → TLV9002 信号调理 → STM32G474 ADC → OLED
                                                                       → HRTIM PWM
```

| 环节 | 器件 | 说明 |
|---|---|---|
| 隔离采样 | AMC1350 | ±5V 差分输入，5kV 隔离 |
| 信号调理 | TLV9002 | 差分转单端 + 电平移位至 0–3.3V |
| MCU | STM32G474RBT6 | 12-bit ADC, 170MHz, HRTIM |
| 显示 | 0.96" OLED | I2C，实时电压 + 诊断 |

### 关键参数

| 参数 | 值 |
|---|---|
| 采样范围 | ±200V DC（兼容 1kW 样机和 20kW 设计） |
| ADC 分辨率 | 12-bit |
| ADC 时钟 | SYSCLK/16 = 10.6MHz（异步时钟方案） |
| 采样时间 | 640 周期 (~60μs)，平均开关纹波 |
| VCM 基线 | 1.645V（0V 输入 ADC 读数） |
| 增益 K | 0.0066 V/V |

### 固件关键问题

**ADC 时钟排障**：CubeMX 默认 PLL 同步时钟导致 ADRDY 不置位，HAL 1ms 超时无法启动 ADC。根因是 G4 系列 ADC 时钟路径与 F1/F4 不同。修复：手动配置 `RCC_ADC12CLKSOURCE_SYSCLK` + `ADC_CLOCK_ASYNC_DIV16`。

**供电问题**：早期板子 VDDA 实测 2.7–2.8V（标称 3.3V），导致参考电压错误、读数漂移，后更换稳定供电解决。

**校准**：`VS_VCM_DEF` 和 `VS_K` 在 `voltage_sample.h` 中定义为可调宏，适配硬件差异。

### OLED 显示

```
Vp: +150.2V     ← 正轨电压
Vn: -148.7V     ← 负轨电压
RAW: 2048 2051  ← ADC 原始读数
E:0             ← 错误码 (0=正常, 5=时钟失败, 6=Start失败, 7/8=轮询超时)
```

### 固件文件

| 文件 | 功能 |
|---|---|
| `src/main.c` | OLED 刷新、HRTIM 输出、100ms/64 样本平均 |
| `src/adc.c` | ADC1(电压×2) + ADC2(电流×2, TMCS1101) 初始化 |
| `src/voltage_sample.c` | 采样核心：`vs_read_counts()`、`vs_read_rails_avg()`、`vs_rail_voltage()` |
| `src/voltage_sample.h` | `VS_VCM_DEF`、`VS_K`、`VS_ADC_VREF` 等可调参数 |

## 文件结构

```
1kw-prototype/
├── README.md
├── 1kW高压隔离LLC变换器原理图说明.docx
└── voltage-sampling/
    ├── README.md
    ├── NEW YEAR.ioc              ← CubeMX 工程
    └── src/
        ├── main.c / main.h
        ├── adc.c / adc.h
        ├── voltage_sample.c
        └── voltage_sample.h
```
