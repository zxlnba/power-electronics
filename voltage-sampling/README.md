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
| 采样时间 | 640 周期 (60µs)，平均 LLC 开关纹波 |
| 基线 VCM | 1.645V（0V 输入时 VADC 实测值） |
| 增益 K | 0.0066 V/V |
| 参考电压 | VDDA = 3.3V |
| 显示刷新 | 500ms |
| 平滑滤波 | 指数平滑，时间常数 ~0.3s |

## 固件关键设计

### ADC 时钟排障

CubeMX 生成的 PLL + 同步时钟配置导致 ADRDY 不置位，HAL 1ms 超时。

**修复**：改 `RCC_ADC12CLKSOURCE_SYSCLK` + `ADC_CLOCK_ASYNC_DIV16`。

### 启动诊断

`vs_adc_startup_diag()` 手动使能 ADC 并轮询 ADRDY（最长 50ms），记录就绪耗时和设备 ID，通过全局变量 `g_diag_adrdy`、`g_diag_elapsed`、`g_diag_devid` 暴露诊断信息。

### 供电问题

早期板子 3.3V/5V 供电偏低且浮动（VDDA 实测 2.7-2.8V），导致参考电压错误、读数漂移。最终使用稳定供电，固件按标准 3.3V 参考。

### 关键校准旋钮

`voltage_sample.h` 中：
- `VS_VCM_DEF`：0V 输入时的 VADC 基线（默认 1.645V）
- `VS_K`：增益（默认 0.0066）

若 0V 输入读数不归零，调整 `VS_VCM_DEF`。

## 软件结构

```
voltage-sampling/
├── NEW YEAR.ioc              ← CubeMX 工程文件
├── README.md
└── src/
    ├── main.c                ← 主程序：OLED 显示、HRTIM 输出、采样循环
    ├── adc.c / adc.h         ← ADC1 初始化（双通道顺序转换）
    ├── voltage_sample.c      ← 采样核心：原始值读取、平均、电压换算
    ├── voltage_sample.h      ← 采样参数宏定义
    └── main.h                ← 外设句柄声明
```

## 调试与验证

- ADC 错误码 `E` 显示在 OLED 第 4 行：0=正常，6=Start 失败，7/8=轮询超时
- RAW 值显示在 OLED 第 3 行，用于诊断 ADC 原始读数
- 关键寄存器可通过 Keil Watch 窗口观察：`g_vpos`、`g_vneg`、`g_adc_error`
