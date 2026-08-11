# 全桥 LLC 谐振变换器

面向数据中心高压直流供电场景的 20kW 单向全桥 LLC 谐振变换器。

- **竞赛**：高校电气电子工程创新大赛 省三等奖
- **团队**：周俊杰 + 袁泽桂，指导教师：许俊云

---

项目分为两个阶段：

| 阶段 | 目录 | 内容 |
|---|---|---|
| **20kW 纸面设计** | [`20kw-design/`](./20kw-design/) | FHA 谐振腔设计、SiC 选型与损耗、PLECS 仿真 |
| **1kW 原理样机** | [`1kw-prototype/`](./1kw-prototype/) | 原理图设计、PCB、电压采样板、实物调试 |

---

## 文件导航

```
llc-resonant-converter/
├── README.md
├── 20kw-design/
│   ├── README.md                       ← 设计过程详解
│   ├── 参数作品设计书.docx               ← 94 页完整设计书
│   ├── 答辩.pptx
│   ├── simulation/  (×9 .plecs)
│   └── images/      (待补充)
└── 1kw-prototype/
    ├── README.md                       ← 样机详解 + 当前状态
    ├── docs/                           ← 工程文档集中目录
    │   ├── 工程开发文档.md              ← 🔥 完整工程开发记录（60V 教训 + 改腔方案）
    │   ├── 1kW高压隔离LLC变换器原理图说明.docx
    │   ├── 缩尺方案与闭环固件测试报告.docx
    │   └── 调试日志/  (ADC 排障 + 采样板全流程)
    ├── pcb/
    │   └── Gerber_PCB1_2026-07-31.zip
    ├── voltage-sampling/
    │   ├── README.md
    │   ├── llc_board.ioc
    │   └── src/  (×6 .c/.h)
    ├── control-loop/                   ← 环路设计 + 理论仿真（MATLAB ×4）
    └── images/      (待补充)
```
