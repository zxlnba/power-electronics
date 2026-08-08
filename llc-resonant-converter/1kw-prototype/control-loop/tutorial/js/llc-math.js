/* llc-math.js — 环路学习网站的数学引擎（零依赖）
 * 复数运算 / 极零点频响 / FHA 增益 / 矩阵指数 / 2p2z 离散闭环仿真
 * 参数全部来自 1kW 样机（control-loop/llc_loop_design.m）
 */
'use strict';

const LLC = (function () {
  /* ================= 复数 ================= */
  class C {
    constructor(re, im = 0) { this.re = re; this.im = im; }
    add(o) { return new C(this.re + o.re, this.im + o.im); }
    sub(o) { return new C(this.re - o.re, this.im - o.im); }
    mul(o) { return new C(this.re * o.re - this.im * o.im, this.re * o.im + this.im * o.re); }
    div(o) {
      const d = o.re * o.re + o.im * o.im;
      return new C((this.re * o.re + this.im * o.im) / d, (this.im * o.re - this.re * o.im) / d);
    }
    abs() { return Math.hypot(this.re, this.im); }
    angle() { return Math.atan2(this.im, this.re); }
    static jw(w) { return new C(0, w); }
  }

  /* ================= 1kW 样机参数 ================= */
  const P = {
    Lr: 270e-6, Cr: 8.2e-9, LmRatio: 5.0, n: 2.0,
    Vin: 400, VoutRail: 200, P_total: 1000, CoRail: 100e-6,
    fsNom: 107000, fsMin: 95000, fsMax: 130000,
    Kc: -1.6848e7, wzHz: 20, wpHz: 10000
  };
  P.fr = 1 / (2 * Math.PI * Math.sqrt(P.Lr * P.Cr));
  P.fm = 1 / (2 * Math.PI * Math.sqrt((P.Lr + P.LmRatio * P.Lr) * P.Cr));
  P.Z0 = Math.sqrt(P.Lr / P.Cr);
  P.V1rms = (2 * Math.SQRT2 / Math.PI) * P.Vin;
  P.Rac = P.V1rms * P.V1rms / P.P_total;
  P.Q = P.Z0 / P.Rac;
  P.RloadRail = P.VoutRail * P.VoutRail / (P.P_total / 2);
  P.fpOut = 1 / (2 * Math.PI * P.RloadRail * P.CoRail);

  /* ================= FHA 电压增益 ================= */
  // M(fn,Q,m) = 1 / sqrt((1+λ-λ/fn²)² + Q²(fn-1/fn)²)，λ = Lr/Lm = 1/m
  // （标准 FHA 公式：与精确 AC 电路传递 |Zp/(Zseries+Zp)| 一致，验证偏差 <1e-15）
  function fhaGain(fn, q, m) {
    const la = 1 / m;
    return 1 / Math.hypot(1 + la - la / (fn * fn), q * (fn - 1 / fn));
  }
  // 静态灵敏度 K_f（V/Hz）：fn=1 处 dM/dfn = -2/m，与负载 Q 无关
  // K_f = (Vin/n)·(-2/m)/fr
  function Kf() { return (P.Vin / P.n) * (-2 / P.LmRatio) / P.fr; }

  /* ================= 频响：按因子直接算，保证直流增益 = K_f ================= */
  // Gvd(s) = K_f·(1+s/ωz_lm) / ((1+s/ωp_out)·(1+s/(ωr·Qr)+s²/ωr²))
  // fp_out = 1/(2π·Rload·Co) 随负载移动：Rload ∝ 1/q，故 fp_out(q) = fp_out_full·(q/Q)
  function gvdAt(q, w) {
    const kf = Kf();
    const wz = 2 * Math.PI * P.fm, wp = 2 * Math.PI * P.fpOut * (q / P.Q), wr = 2 * Math.PI * P.fr, Qr = 1;
    let g = new C(kf, 0);
    g = g.mul(new C(1, w / wz));        // (1 + s/wz_lm)
    g = g.div(new C(1, w / wp));        // 1/(1 + s/wp_out)
    const re = 1 - (w / wr) * (w / wr); // 1-(w/wr)²
    const im = w / (wr * Qr);           // w/(wr·Qr)
    g = g.div(new C(re, im));           // 1/(1+s/(wrQr)+s²/wr²)
    return g;
  }
  // 补偿器 C(s) = K_c·(1+s/wz)/(s·(1+s/wp))，参数可调（默认设计值）
  function cat(w, kc = P.Kc, wzHz = P.wzHz, wpHz = P.wpHz) {
    const wz = 2 * Math.PI * wzHz, wp = 2 * Math.PI * wpHz;
    let g = new C(kc, 0);
    g = g.mul(new C(1, w / wz));
    g = g.div(new C(0, w));
    g = g.div(new C(1, w / wp));
    return g;
  }
  function openLoopAt(w, q, kc, wzHz, wpHz) { return cat(w, kc, wzHz, wpHz).mul(gvdAt(q, w)); }

  /* ================= Tustin 离散化 -> 2p2z 系数 ================= */
  // C(s) = K_c·(1+s/wz)/(s·(1+s/wp))，s = (2/Ts)·(z-1)/(z+1)
  function tustin2p2z() {
    const Ts = 1 / P.fr;
    const c = 2 / Ts;
    const m0 = P.Kc, m1 = P.Kc / (2 * Math.PI * P.wzHz);
    const d1 = 1, d2 = 1 / (2 * Math.PI * P.wpHz);
    // 分子 m0 + m1·s，分母 d1·s + d2·s²；同乘 (z+1)² 后
    // num = m0(z²+2z+1) + m1·c(z²-1)
    const nA = m0 + m1 * c, nB = 2 * m0, nC = m0 - m1 * c;
    // den = d1·c(z²-1) + d2·c²(z²-2z+1)
    const dA = d1 * c + d2 * c * c, dB = -2 * d2 * c * c, dC = d2 * c * c - d1 * c;
    return { b0: nA / dA, b1: nB / dA, b2: nC / dA, a1: dB / dA, a2: dC / dA, Ts };
  }

  /* ================= Bode 扫描与裕度 =================
     fco/PM：二分定位 |L|=1（0dB 穿越）处的相位裕度
     GM   ：二分定位相位最接近 -180° 处的增益余量 */
  function bodeMargin(q, opts = {}) {
    const { kc = P.Kc, wzHz = P.wzHz, wpHz = P.wpHz, KcScale = 1 } = opts;
    const phF = f => { const l = openLoopAt(2 * Math.PI * f, q, kc, wzHz, wpHz); return l.angle() * 180 / Math.PI; };
    const magF = f => { const l = openLoopAt(2 * Math.PI * f, q, kc, wzHz, wpHz); return l.abs() * Math.abs(KcScale); };
    const bisect = (f0, f1, target, score, iters = 60) => {
      let a = f0, b = f1, sa = score(a, target), sb = score(b, target);
      if (sa * sb > 0) return null; // 区间内无符号变化
      for (let k = 0; k < iters; k++) {
        const mid = Math.exp((Math.log(a) + Math.log(b)) / 2);
        const sm = score(mid, target);
        if (sa * sm <= 0) { b = mid; sb = sm; } else { a = mid; sa = sm; }
      }
      return Math.exp((Math.log(a) + Math.log(b)) / 2);
    };
    const dMag = (f, t) => 20 * Math.log10(magF(f)) - t;  // 找 20log10|L| = 0
    const dPh  = (f, t) => phF(f) - t;                     // 找 相位 = t

    // 粗扫找候选区间
    let cFco = 0, cPh = 0, bestErr = Infinity, c180 = 0, bestPhErr = Infinity;
    for (let k = 0; k <= 800; k++) {
      const f = Math.pow(10, -1 + k * 6.5 / 800); // 0.1Hz ~ 3e5
      const mErr = Math.abs(20 * Math.log10(magF(f)));
      if (mErr < bestErr) { bestErr = mErr; cFco = f; cPh = phF(f); }
      const pErr = Math.abs(phF(f) + 180);
      if (pErr < bestPhErr) { bestPhErr = pErr; c180 = f; }
    }
    // 在候选附近找精确穿越
    const fco = bisect(cFco / 1.6, cFco * 1.6, 0, dMag) || cFco;
    const pm = phF(fco) + 180;
    const f180 = bisect(c180 / 1.8, c180 * 1.8, -180, dPh) || c180;
    const gmDB = (() => {
      const g = 20 * Math.log10(magF(f180));
      return Math.abs(g);
    })();
    return { fco, pm, gmDB, f180 };
  }

  // 在 s 平面复数极点的位置（用于极点-响应联动演示）
  function poleLoc(zeta, wn) {
    return { re: -zeta * wn, im: wn * Math.sqrt(Math.max(0, 1 - zeta * zeta)) };
  }

  /* ================= 二阶阶跃响应（解析） ================= */
  function secondOrderStep(zeta, wn, t) {
    if (zeta < 1) {
      const wd = wn * Math.sqrt(1 - zeta * zeta);
      const phi = Math.atan2(Math.sqrt(1 - zeta * zeta), zeta);
      return 1 - (wn / wd) * Math.exp(-zeta * wn * t) * Math.sin(wd * t + phi);
    } else if (zeta === 1) {
      return 1 - Math.exp(-wn * t) * (1 + wn * t);
    } else {
      const a = zeta - Math.sqrt(zeta * zeta - 1), b = zeta + Math.sqrt(zeta * zeta - 1);
      return 1 - (b / (b - a)) * Math.exp(-a * wn * t) + (a / (b - a)) * Math.exp(-b * wn * t);
    }
  }

  /* ================= 矩阵工具（数值 expm，用于 ZOH 离散化） ================= */
  function matAdd(A, B) { return A.map((r, i) => r.map((v, j) => v + B[i][j])); }
  function matScale(A, k) { return A.map(r => r.map(v => v * k)); }
  function matMul(A, B) {
    const n = A.length, m = B[0].length, p = B.length, R = [];
    for (let i = 0; i < n; i++) {
      R[i] = new Array(m).fill(0);
      for (let k = 0; k < p; k++) for (let j = 0; j < m; j++) R[i][j] += A[i][k] * B[k][j];
    }
    return R;
  }
  function matEye(n) { return Array.from({ length: n }, (_, i) => Array.from({ length: n }, (_, j) => (i === j ? 1 : 0))); }
  function matNorm(A) { return Math.max(...A.map(r => Math.max(...r.map(Math.abs)))); }
  function matExp(A, dt) {
    let B = matEye(A.length), term = matEye(A.length);
    for (let k = 1; k < 50; k++) {
      term = matScale(matMul(term, A), dt / k);
      B = matAdd(B, term);
      if (matNorm(term) < 1e-15) break;
    }
    return B;
  }
  function matInv3(A) {
    const n = A.length, aug = A.map((r, i) => [...r, ...matEye(n)[i]]);
    for (let col = 0; col < n; col++) {
      let piv = col;
      for (let r = col + 1; r < n; r++) if (Math.abs(aug[r][col]) > Math.abs(aug[piv][col])) piv = r;
      [aug[col], aug[piv]] = [aug[piv], aug[col]];
      const pv = aug[col][col];
      aug[col] = aug[col].map(v => v / pv);
      for (let r = 0; r < n; r++) {
        if (r === col) continue;
        const f = aug[r][col];
        if (f !== 0) aug[r] = aug[r].map((v, j) => v - f * aug[col][j]);
      }
    }
    return aug.map(r => r.slice(n));
  }

  /* ================= Gvd 连续状态空间（能控标准型，分子近似为常数 K_f） =================
     低频（< fm）分子零点可忽略，直流增益 = K_f。
     Gvd ≈ K_f/((1+s/wp)(1+s/(wrQr)+s²/wr²))
     den = s³ + a2 s² + a1 s + a0，分子 = K_f·a0（常数）
     能控标准型 C = [K_f·a0, 0, 0] */
  function gvdSS(q, Qr = 1.0) {
    const kf = Kf();
    const wp = 2 * Math.PI * P.fpOut * (q / P.Q), wr = 2 * Math.PI * P.fr, zeta = 1 / (2 * Qr);
    const a2 = wp + 2 * zeta * wr, a1 = wr * wr + 2 * zeta * wr * wp, a0 = wr * wr * wp;
    return {
      A: [[0, 1, 0], [0, 0, 1], [-a0, -a1, -a2]],
      B: [[0], [0], [1]],
      Cc: [[kf * a0, 0, 0]],
      Dd: [[0]],
      dc: kf
    };
  }

  /* ================= 2p2z 离散闭环仿真 ================= */
  // opts: {q, nSteps, distHz, distAt, vref, KcScale}
  function closedLoopSim(opts) {
    const o = Object.assign(
      { q: P.Q, nSteps: 2200, distHz: 10000, distAt: 800, vref: P.VoutRail, KcScale: 1 },
      opts
    );
    const Ts = 1 / P.fr;
    const { b0, b1, b2, a1, a2 } = tustin2p2z();
    const Ks = o.KcScale;
    const ss = gvdSS(o.q, 1.0);
    const Ad = matExp(ss.A, Ts);
    const Bd = matMul(matMul(matAdd(Ad, matScale(matEye(3), -1)), matInv3(ss.A)), ss.B);

    const N = o.nSteps;
    const t = new Float64Array(N), vout = new Float64Array(N), fs = new Float64Array(N);
    let x = [0, 0, 0], x1 = 0, x2 = 0, y = o.vref;
    for (let k = 0; k < N; k++) {
      t[k] = k * Ts;
      vout[k] = y;
      const err = o.vref - y;
      const w = Ks * b0 * err + x1;
      x1 = Ks * b1 * err - Ks * a1 * w + x2;
      x2 = Ks * b2 * err - Ks * a2 * w;
      let f = P.fr + w;
      f = Math.min(P.fsMax, Math.max(P.fsMin, f));
      fs[k] = f;
      const dist = (k >= o.distAt) ? o.distHz : 0;
      const u = (f - P.fr) - dist;
      x = matMul(Ad, x.map(v => [v])).map(r => r[0]).map((v, i) => v + Bd[i][0] * u);
      y = o.vref + ss.Cc[0][0] * x[0] + ss.Cc[0][1] * x[1] + ss.Cc[0][2] * x[2] + ss.Dd[0][0] * u;
    }
    return { t, vout, fs };
  }

  return {
    C, fhaGain, Kf, P, gvdAt, cat, openLoopAt, tustin2p2z, bodeMargin, poleLoc,
    secondOrderStep, closedLoopSim, matExp, matInv3, gvdSS
  };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = LLC;
