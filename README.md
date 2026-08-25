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

## 📌 最新进展（2026-08-25）

1kW 原理样机完成 **60V/300W 缩尺腔**验证与损耗排查：

- **缩尺腔（方案A 落地）**：Lr=8.2µH、Cr=470nF、Lm=26µH（PQ35 1:1、6T），实测
  fr≈75kHz 增益=1（感性区 ZVS），副边整流前方波增益健康；
- **压降/损耗根因定位**：除副边整流管 VF（RHRP860 → MBR10100 更换）外，进一步定位到
  原边开通叠区（开通电阻 20Ω→10Ω 后效率 90%+）与循环电流 I²R；
- **闭环固件**：2p2z 数字电压环 + 软启动 + 过流保护已落地；采样时序由 640 周期缩短到
  12.5 周期，并在 HRTIM 周期边界相位锁定。

- **完整工程记录**：[`1kw-prototype/01-设计样机/工程开发文档.md`](./llc-resonant-converter/1kw-prototype/01-设计样机/工程开发文档.md)
- **调试日志**：[`1kw-prototype/03-调试代码/调试日志/`](./llc-resonant-converter/1kw-prototype/03-调试代码/调试日志/)
- **60V 缩尺样机文档**：[`1kw-prototype/04-60V缩尺300W/README.md`](./llc-resonant-converter/1kw-prototype/04-60V缩尺300W/README.md)

教训：任何缩尺/改点测试，先算目标负载 Q、查该 Q 下增益曲线与损耗预算，再定目标电压/功率。

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
