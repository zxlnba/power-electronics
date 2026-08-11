# 1kW 样机 PCB

1kW 全桥 LLC 变换器主板，嘉立创 EDA 设计，2026-07-31 出 Gerber。

## 板卡信息（Gerber 实测）

| 项目 | 数据 |
|---|---|
| 层数 | **2 层**（Top + Bottom，双面焊盘） |
| 过孔 | **595**（0.305mm / 0.381mm） |
| 镀通孔 PTH | **726**（0.305–1.9mm 共 22 种孔径） |
| 非镀通孔 NPTH | **2**（1.3mm，安装孔） |
| 组装 | 双面：Top/Bottom 丝印 + 阻焊 + 锡膏 |
| 生产测试 | **飞针测试通过**（`FlyingProbeTesting.json`） |
| 工具链 | 嘉立创 EDA Pro v3.2.174 |

## 文件

```
pcb/
├── Gerber_PCB1_2026-07-31.zip   ← 完整 Gerber + 钻孔 + 飞针测试数据
└── README.md                    ← 本文件
```

> 板卡原理图与设计说明见 [`../docs/1kW高压隔离LLC变换器原理图说明.docx`](../docs/1kW高压隔离LLC变换器原理图说明.docx)。
