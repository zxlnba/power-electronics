# Power Electronics Portfolio

## 仓库地图

```
power-electronics/
├── README.md                 ← 本文件
├── flyback-uc3842/           ← 反激电源（UC3842，72W）
└── llc-resonant-converter/   ← 全桥 LLC（20kW 纸面设计 + 1kW 原理样机）
    ├── 20kw-design/          ← FHA 设计 / SiC 选型 / PLECS 仿真 / 竞赛设计书
    └── 1kw-prototype/        ← 原理图 / PCB / 固件 / 环路设计 / 调试验证
```

## 📌 最新进展（2026-08-11）

1kW LLC 样机完成 60V 低压测试，**发现谐振腔设计点与 60V 负载不匹配**（现腔 Z0=181.5Ω
按额定 400V/1kW Q=1.4 设计，60V 可用负载换算 Q=2~8 → 增益封顶 M≈1.0 无升压区，实测
M≈0.8）。已给出**改腔方案（Z0=52.2）**并整理成完整工程开发文档：

- **完整工程记录**：[`llc-resonant-converter/1kw-prototype/docs/工程开发文档.md`](./llc-resonant-converter/1kw-prototype/docs/工程开发文档.md)
- **调试日志**：[`llc-resonant-converter/1kw-prototype/docs/调试日志/`](./llc-resonant-converter/1kw-prototype/docs/调试日志/)
- 教训：任何缩尺/改点测试，先算目标负载 Q、查该 Q 下增益曲线与损耗预算，再定目标电压/功率。

## 项目

### [全桥 LLC 谐振变换器](./llc-resonant-converter/)

面向数据中心高压直流供电场景的 20kW 全桥 LLC 谐振变换器。

| 阶段 | 目录 | 内容 |
|---|---|---|
| 20kW 纸面设计 | [`20kw-design/`](./llc-resonant-converter/20kw-design/) | FHA 谐振腔设计、SiC 选型与损耗、PLECS 仿真、PCB 设计 |
| 1kW 原理样机 | [`1kw-prototype/`](./llc-resonant-converter/1kw-prototype/) | 缩比样机原理图、电压采样板、调试验证 |

### [反激开关电源 — 基于 UC3842](./flyback-uc3842/)

24V/3A (72W) 反激开关电源，从参数计算、变压器绕制到 PCB 调试全流程独立完成。含实测效率、纹波、环路数据及故障排查记录。

## 技能

| 类别 | 工具/技术 |
|---|---|
| 拓扑 | LLC、反激、Buck/Boost |
| 仿真 | PLECS、MATLAB/Simulink |
| PCB | Altium Designer、嘉立创 EDA |
| 嵌入式 | C、STM32 (Keil MDK) |
| 测试 | 示波器、直流电源、电子负载、LCR 电桥 |

- GitHub: [@zxlnba](https://github.com/zxlnba)
