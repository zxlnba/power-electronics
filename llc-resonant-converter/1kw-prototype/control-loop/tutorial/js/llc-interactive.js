/* llc-interactive.js — 环路学习网站交互组件
 * 5 个可拖参数实时看结果的教学演示（HTML 中 <div data-int="..."> 为宿主）
 * 依赖 llc-math.js, llc-plots.js
 */
'use strict';

const Int = (function () {
  /* ---------- 通用控件构建 ---------- */
  function ctlRow() { const d = document.createElement('div'); d.className = 'ctl-row'; return d; }
  function readout() { const d = document.createElement('div'); d.className = 'readout'; return d; }
  function slider(host, label, min, max, step, value, fmt, onChange) {
    const l = document.createElement('label'); l.className = 'ctl';
    const s = document.createElement('span'); s.textContent = label;
    const i = document.createElement('input');
    i.type = 'range'; i.min = min; i.max = max; i.step = step; i.value = value;
    const b = document.createElement('b'); b.textContent = fmt(value);
    i.addEventListener('input', () => { b.textContent = fmt(+i.value); onChange(+i.value); });
    l.append(s, i, b); host.append(l);
    return { input: i, get: () => +i.value };
  }
  function select(host, label, options, onChange) {
    const l = document.createElement('label'); l.className = 'ctl';
    const s = document.createElement('span'); s.textContent = label;
    const se = document.createElement('select');
    for (const [v, t] of options) { const o = document.createElement('option'); o.value = v; o.textContent = t; se.append(o); }
    se.addEventListener('change', () => onChange(se.value));
    l.append(s, se); host.append(l);
    return { el: se, get: () => se.value };
  }
  function canvas(w, h) {
    const c = document.createElement('canvas');
    c.className = 'int-canvas';
    c.dataset.W = w; c.dataset.H = h;
    return c;
  }
  function phaseUnwrap(phs) {
    const out = [phs[0]];
    for (let i = 1; i < phs.length; i++) {
      let d = phs[i] - phs[i - 1];
      while (d > 180) d -= 360; while (d < -180) d += 360;
      out.push(out[i - 1] + d);
    }
    return out;
  }

  /* =================================================================
     组件 1：FHA 增益曲线 —— 旋钮与秋千
     调 m(Lm/Lr) 与负载，看增益曲线、M_max 与升压极限
     ================================================================= */
  function initGainCurve(host) {
    const ctl = ctlRow(), ro = readout(), cv = canvas(680, 360);
    host.append(ctl, cv, ro);
    const P = LLC.P;
    const loads = { full: { q: P.Q, name: '满载' }, half: { q: P.Q / 2, name: '50%负载' }, quar: { q: P.Q / 4, name: '25%负载' } };
    let m = 5, lk = 'full';
    slider(ctl, 'Lm/Lr (m)', 3, 7, 0.1, 5, v => v.toFixed(1), v => { m = v; draw(); });
    select(ctl, '负载', [['full', '满载'], ['half', '50%'], ['quar', '25%']], v => { lk = v; draw(); });

    function draw() {
      const { ctx, w, h } = Plot.setup(cv, +cv.dataset.W, +cv.dataset.H);
      Plot.clear(ctx, w, h);
      const pad = { l: 62, r: 18, t: 26, b: 42, w, h };
      const fnMin = 0.4, fnMax = 1.7;
      const tr = Plot.log(fnMin, fnMax, 0.4, 1.6, pad);
      const q = loads[lk].q;
      // 曲线
      const xs = [], ys = [];
      for (let i = 0; i <= 500; i++) { const fn = fnMin + i * (fnMax - fnMin) / 500; xs.push(fn); ys.push(LLC.fhaGain(fn, q, m)); }
      Plot.grid(ctx, tr, Plot.logTicks(fnMin, fnMax), Plot.linTicks(0.4, 1.6, 12));
      Plot.line(ctx, tr, xs, ys, { color: '#2563eb', lineWidth: 2.4 });
      // 参考线
      Plot.hline(ctx, tr, 1, { color: '#64748b' });
      Plot.hline(ctx, tr, P.Vin / 360, { color: '#f43f5e', dash: [5, 4] });
      Plot.hline(ctx, tr, P.Vin / 380, { color: '#f97316', dash: [3, 3] });
      Plot.vline(ctx, tr, 1, { color: '#94a3b8', dash: [4, 3] });
      const fnFloor = P.fsMin / P.fr;   // fs_min=95kHz → fn=0.888，低于此控制器够不到
      Plot.vline(ctx, tr, fnFloor, { color: '#f97316', dash: [3, 3] });
      // M_max 标注（理论峰值，可能低于 fs_min 下限）
      let mmax = 0, fnAt = 0;
      for (let i = 0; i <= 4000; i++) { const fn = fnMin + i * (fnMax - fnMin) / 4000; const M = LLC.fhaGain(fn, q, m); if (M > mmax) { mmax = M; fnAt = fn; } }
      Plot.mark(ctx, tr, fnAt, mmax, { color: '#f43f5e', r: 5 });
      Plot.label(ctx, 'M_max = ' + mmax.toFixed(3) + ' @ fn=' + fnAt.toFixed(2), tr.x(fnAt) + 6, tr.y(mmax) - 6, { color: '#f43f5e', size: 12 });
      Plot.label(ctx, 'M=1.111（360V 需求）', tr.x(1.3), tr.y(P.Vin / 360) - 6, { color: '#f43f5e', size: 11, align: 'right' });
      Plot.label(ctx, 'M=1（400V 需求）', tr.x(1.45), tr.y(1) - 6, { color: '#64748b', size: 11, align: 'right' });
      Plot.label(ctx, 'fn=1（fs=fr）', tr.x(1) - 4, tr.pad.h - 6, { color: '#94a3b8', size: 11, align: 'right' });
      Plot.label(ctx, 'fn = fs/fr', tr.pad.w - 2, tr.pad.h - 4, { color: '#64748b', size: 11, align: 'right' });
      Plot.label(ctx, '电压增益 M', 10, 18, { color: '#334155', size: 12 });
      // 读数：最低输入按"频带内可达增益"算（峰值低于 fs_min 时够不到）
      let mreach = 0;
      for (let i = 0; i <= 4000; i++) { const fn = fnFloor + i * (fnMax - fnFloor) / 4000; const M = LLC.fhaGain(fn, q, m); if (M > mreach) mreach = M; }
      const reachable = Math.min(mmax, mreach);
      const VinMin = P.Vin / reachable;
      const needs360 = reachable >= P.Vin / 360;
      const note = (mmax > reachable) ? '（峰值在 fs_min 下方，不可达）' : '';
      ro.innerHTML = '<div class="ro-item"><span>M<sub>max</sub> 可达</span><b>' + reachable.toFixed(3) + '</b>' + (note ? '<em style="color:#d97706;font-size:11px">' + note + '</em>' : '') +
        '</div><div class="ro-item"><span>最低可保持 ±200V 的输入</span><b>' + VinMin.toFixed(0) + ' V</b></div>' +
        '<div class="ro-item"><span>360V 满载稳压</span><b class="' + (needs360 ? 'ok' : 'warn') + '">' + (needs360 ? '✓ 能保持' : '✗ 会跌落（升压不足）') + '</b></div>';
    }
    draw();
  }

  /* =================================================================
     组件 2：极点在 s 平面的位置 vs 阶跃响应
     拖 ζ（阻尼）与 ωn（自然频率），右侧看对应二阶系统的阶跃
     ================================================================= */
  function initPoles(host) {
    const ctl = ctlRow(), ro = readout();
    const wrap = document.createElement('div'); wrap.className = 'poles-wrap';
    const cvS = canvas(320, 320), cvY = canvas(360, 320);
    cvS.className += ' small'; cvY.className += ' small';
    wrap.append(cvS, cvY);
    host.append(ctl, wrap, ro);
    let zeta = 0.35, wn = 400;
    slider(ctl, '阻尼 ζ', 0.05, 1.3, 0.01, 0.35, v => v.toFixed(2), v => { zeta = v; draw(); });
    slider(ctl, '自然频率 ωn (rad/s)', 100, 1500, 10, 400, v => v.toFixed(0), v => { wn = v; draw(); });

    function draw() {
      // ---- s 平面 ----
      const { ctx, w, h } = Plot.setup(cvS, +cvS.dataset.W, +cvS.dataset.H);
      Plot.clear(ctx, w, h);
      const pad = { l: 46, r: 14, t: 16, b: 40, w, h };
      const sMin = -1700, sMax = 100, wMax = 1600;
      const tr = Plot.lin(sMin, sMax, -wMax, wMax, pad);
      // 轴
      ctx.strokeStyle = '#cbd5e1'; ctx.lineWidth = 1.2;
      ctx.beginPath(); ctx.moveTo(tr.x(sMin), tr.y(0)); ctx.lineTo(tr.x(sMax), tr.y(0)); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(tr.x(0), tr.y(-wMax)); ctx.lineTo(tr.x(0), tr.y(wMax)); ctx.stroke();
      // 阻尼线
      const beta = Math.acos(zeta);
      const R = 1500;
      ctx.setLineDash([3, 4]); ctx.strokeStyle = '#94a3b8'; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(tr.x(0), tr.y(0)); ctx.lineTo(tr.x(-R * Math.cos(beta)), tr.y(R * Math.sin(beta))); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(tr.x(0), tr.y(0)); ctx.lineTo(tr.x(-R * Math.cos(beta)), tr.y(-R * Math.sin(beta))); ctx.stroke();
      ctx.setLineDash([]);
      // 极点
      const loc = LLC.poleLoc(zeta, wn);
      for (const im of [loc.im, -loc.im]) {
        ctx.fillStyle = '#2563eb';
        ctx.beginPath(); ctx.arc(tr.x(loc.re), tr.y(im), 7, 0, Math.PI * 2); ctx.fill();
        ctx.strokeStyle = '#1e40af'; ctx.lineWidth = 2; ctx.stroke();
      }
      Plot.label(ctx, 'σ', tr.pad.w - 16, tr.y(0) + 16, { size: 12 });
      Plot.label(ctx, 'jω', tr.x(0) + 6, tr.pad.t + 8, { size: 12 });
      Plot.label(ctx, '极点离虚轴越近 → 振荡衰减越慢', tr.pad.l, tr.pad.h - 4, { color: '#64748b', size: 11 });
      // ---- 阶跃响应 ----
      const { ctx: c2, w: w2, h: h2 } = Plot.setup(cvY, +cvY.dataset.W, +cvY.dataset.H);
      Plot.clear(c2, w2, h2);
      const padY = { l: 40, r: 14, t: 20, b: 40, w: w2, h: h2 };
      const tMax = Math.max(0.06, 6 / (zeta * wn));
      const trY = Plot.lin(0, tMax, -0.3, 1.5, padY);
      const xs = [], ys = [];
      for (let i = 0; i <= 400; i++) { const t = i * tMax / 400; xs.push(t); ys.push(LLC.secondOrderStep(zeta, wn, t)); }
      Plot.grid(c2, trY, Plot.linTicks(0, tMax, 6), Plot.linTicks(-0.2, 1.4, 8));
      Plot.hline(c2, trY, 1, { color: '#f43f5e', dash: [4, 3] });
      Plot.line(c2, trY, xs, ys, { color: '#2563eb', lineWidth: 2.2 });
      Plot.label(c2, '单位阶跃响应', trY.pad.l, 14, { color: '#334155', size: 12 });
      Plot.label(c2, 't (s)', trY.pad.w - 2, trY.pad.h - 4, { size: 11, align: 'right' });
      // ---- 读数 ----
      const Mp = zeta < 1 ? Math.exp(-Math.PI * zeta / Math.sqrt(1 - zeta * zeta)) * 100 : 0;
      const ts = zeta < 1 ? 4 / (zeta * wn) : 5 / wn;
      ro.innerHTML = '<div class="ro-item"><span>超调量</span><b>' + Mp.toFixed(0) + '%</b></div>' +
        '<div class="ro-item"><span>峰值时间</span><b>' + (zeta < 1 ? (Math.PI / (wn * Math.sqrt(1 - zeta * zeta)) * 1000).toFixed(0) + ' ms' : '—') + '</b></div>' +
        '<div class="ro-item"><span>调节时间(2%)</span><b>' + (ts * 1000).toFixed(0) + ' ms</b></div>' +
        '<div class="ro-item"><span>阻尼比</span><b>' + (zeta < 1 ? '欠阻尼：会振荡' : zeta === 1 ? '临界阻尼' : '过阻尼：无振荡') + '</b></div>';
    }
    draw();
  }

  /* =================================================================
     组件 3：开环 Bode 设计台
     调 K_c、零点(20Hz)、高频极点(10kHz)，实时看 fco/PM/GM
     ================================================================= */
  function initBode(host) {
    const ctl = ctlRow(), ro = readout(), cv = canvas(680, 440);
    host.append(ctl, cv, ro);
    let kcX = 1, wz = LLC.P.wzHz, wp = LLC.P.wpHz;
    slider(ctl, 'K_c ×', 0.2, 3, 0.05, 1, v => v.toFixed(2) + '×', v => { kcX = v; draw(); });
    slider(ctl, '零点 fz (Hz)', 1, 200, 1, 20, v => v.toFixed(0), v => { wz = v; draw(); });
    slider(ctl, '高频极点 fp (kHz)', 1, 50, 0.5, 10, v => v.toFixed(1), v => { wp = v * 1000; draw(); });

    function draw() {
      const { ctx, w, h } = Plot.setup(cv, +cv.dataset.W, +cv.dataset.H);
      Plot.clear(ctx, w, h);
      const kc = LLC.P.Kc * kcX;
      const fMin = 1, fMax = 1e6, N = 320;
      const fs = [], mags = [], phs = [];
      for (let i = 0; i <= N; i++) {
        const f = Math.pow(10, Math.log10(fMin) + i * (Math.log10(fMax) - Math.log10(fMin)) / N);
        const L = LLC.openLoopAt(2 * Math.PI * f, LLC.P.Q, kc, wz, wp);
        fs.push(f); mags.push(20 * Math.log10(L.abs())); phs.push(L.angle() * 180 / Math.PI);
      }
      const phU = phaseUnwrap(phs);
      // 裕度
      const m = LLC.bodeMargin(LLC.P.Q, { kc, wzHz: wz, wpHz: wp });

      // ---- 增益面板 ----
      const pad1 = { l: 54, r: 16, t: 30, b: 26, w, h };
      const tr1 = Plot.log(fMin, fMax, -100, 100, pad1);
      Plot.grid(ctx, tr1, Plot.logTicks(fMin, fMax), Plot.linTicks(-100, 100, 10));
      Plot.line(ctx, tr1, fs, mags, { color: '#2563eb', lineWidth: 2.2 });
      Plot.hline(ctx, tr1, 0, { color: '#f43f5e' });
      Plot.vline(ctx, tr1, m.fco, { color: '#10b981' });
      Plot.mark(ctx, tr1, m.fco, 0, { color: '#10b981', r: 4.5 });
      Plot.label(ctx, '开环增益 |L| (dB)', tr1.pad.l, 16, { size: 12 });
      Plot.label(ctx, 'fco = ' + m.fco.toFixed(0) + ' Hz', tr1.x(m.fco) + 6, tr1.y(6), { color: '#10b981', size: 12 });

      // ---- 相位面板 ----
      const pad2 = { l: 54, r: 16, t: 42, b: 26, w, h };
      const tr2 = Plot.log(fMin, fMax, -360, 0, pad2);
      Plot.grid(ctx, tr2, Plot.logTicks(fMin, fMax), Plot.linTicks(-360, 0, 6));
      Plot.line(ctx, tr2, fs, phU, { color: '#f59e0b', lineWidth: 2.2 });
      Plot.hline(ctx, tr2, -180, { color: '#f43f5e' });
      Plot.mark(ctx, tr2, m.fco, -180 + m.pm, { color: '#10b981', r: 4.5 });
      Plot.label(ctx, '相位 ∠L (°)', tr2.pad.l, tr2.pad.t - 8, { size: 12 });
      Plot.label(ctx, '-180°（振荡线）', tr2.pad.w - 90, tr2.y(-180) - 6, { color: '#f43f5e', size: 11, align: 'right' });
      Plot.label(ctx, 'fco 处相位 → PM', tr2.x(m.fco) + 6, tr2.y(-180 + m.pm) - 6, { color: '#10b981', size: 11 });
      Plot.label(ctx, '频率 (Hz)', tr2.pad.w - 2, tr2.pad.h - 4, { size: 11, align: 'right' });

      // ---- 读数 ----
      const pmWarn = m.pm < 30 ? 'warn' : (m.pm < 45 ? 'ok' : 'ok');
      ro.innerHTML =
        '<div class="ro-item"><span>穿越频率 fco</span><b>' + m.fco.toFixed(0) + ' Hz</b></div>' +
        '<div class="ro-item"><span>相位裕度 PM</span><b class="' + (m.pm < 30 ? 'warn' : 'ok') + '">' + m.pm.toFixed(1) + '°</b></div>' +
        '<div class="ro-item"><span>增益裕度 GM</span><b>' + (isFinite(m.gmDB) ? m.gmDB.toFixed(1) + ' dB' : '∞') + '</b></div>' +
        '<div class="ro-item"><span>设计参考</span><b>fco≈2kHz, PM≥45°, GM>10dB</b></div>';
    }
    draw();
  }

  /* =================================================================
     组件 4：稳定性演示 —— 拉大 K_c 看到环路从稳到发散
     ================================================================= */
  function initStability(host) {
    const ctl = ctlRow(), ro = readout(), cv = canvas(680, 420);
    host.append(ctl, cv, ro);
    let kcX = 1;
    slider(ctl, 'K_c ×', 0.2, 8, 0.1, 1, v => v.toFixed(1) + '×', v => { kcX = v; draw(); });

    function draw() {
      const { ctx, w, h } = Plot.setup(cv, +cv.dataset.W, +cv.dataset.H);
      Plot.clear(ctx, w, h);
      const s = LLC.closedLoopSim({ KcScale: kcX, nSteps: 2600, distHz: 10000, distAt: 800 });
      const P = LLC.P;
      // 是否发散
      const div = Math.max(...Array.from(s.vout)) > 280 || Math.min(...Array.from(s.vout)) < 120 ||
        Array.from(s.vout).some(v => !isFinite(v));

      const pad1 = { l: 48, r: 16, t: 28, b: 26, w, h };
      const tr1 = Plot.lin(0, s.t[s.t.length - 1], 140, 260, pad1);
      Plot.grid(ctx, tr1, Plot.linTicks(0, s.t[s.t.length - 1], 8), Plot.linTicks(150, 250, 5));
      Plot.hline(ctx, tr1, 200, { color: '#f43f5e', dash: [4, 3] });
      Plot.line(ctx, tr1, s.t, s.vout, { color: '#2563eb', lineWidth: 1.8 });
      Plot.label(ctx, 'Vout (V)，负载阶跃在 7.5ms', tr1.pad.l, 14, { size: 12 });
      Plot.label(ctx, 't (s)', tr1.pad.w - 2, tr1.pad.h - 4, { size: 11, align: 'right' });

      const pad2 = { l: 48, r: 16, t: 52, b: 26, w, h };
      const tr2 = Plot.lin(0, s.t[s.t.length - 1], 90, 150, pad2);
      Plot.grid(ctx, tr2, Plot.linTicks(0, s.t[s.t.length - 1], 8), Plot.linTicks(95, 145, 5));
      Plot.line(ctx, tr2, s.t, s.fs.map(f => f / 1e3), { color: '#f59e0b', lineWidth: 1.8 });
      Plot.hline(ctx, tr2, 107, { color: '#94a3b8', dash: [4, 3] });
      Plot.label(ctx, 'fs 指令 (kHz)，107 = fr', tr2.pad.l, tr2.pad.t - 8, { size: 12 });
      Plot.label(ctx, 't (s)', tr2.pad.w - 2, tr2.pad.h - 4, { size: 11, align: 'right' });

      const mx = Math.max(...Array.from(s.vout).map(v => isFinite(v) ? Math.abs(v - 200) : 0));
      const pm = LLC.bodeMargin(P.Q, { KcScale: kcX }).pm;
      ro.innerHTML =
        '<div class="ro-item"><span>此时相位裕度</span><b>' + pm.toFixed(1) + '°</b></div>' +
        '<div class="ro-item"><span>峰值偏差</span><b>' + mx.toFixed(1) + ' V</b></div>' +
        '<div class="ro-item"><span>结论</span><b class="' + (div ? 'warn' : 'ok') + '">' +
        (div ? '发散！环路已振荡失稳' : (pm < 30 ? '临界：接近振荡，余量不足' : '稳定：裕度充足')) + '</b></div>';
    }
    draw();
  }

  /* =================================================================
     组件 5：闭环阶跃仿真（真实 2p2z 系数）
     调负载扰动大小，看 Vout 与 fs 的恢复过程
     ================================================================= */
  function initStepSim(host) {
    const ctl = ctlRow(), ro = readout(), cv = canvas(680, 420);
    host.append(ctl, cv, ro);
    let dist = 10000;
    slider(ctl, '负载扰动 (等效 kHz)', 0, 20000, 500, 10000, v => (v / 1e3).toFixed(1) + 'k', v => { dist = v; draw(); });

    function draw() {
      const { ctx, w, h } = Plot.setup(cv, +cv.dataset.W, +cv.dataset.H);
      Plot.clear(ctx, w, h);
      const s = LLC.closedLoopSim({ nSteps: 2600, distHz: dist, distAt: 800 });
      const P = LLC.P;
      const tMax = s.t[s.t.length - 1];

      const pad1 = { l: 48, r: 16, t: 28, b: 26, w, h };
      const tr1 = Plot.lin(0, tMax, 190, 210, pad1);
      Plot.grid(ctx, tr1, Plot.linTicks(0, tMax, 8), Plot.linTicks(192, 208, 8));
      Plot.hline(ctx, tr1, 200, { color: '#f43f5e', dash: [4, 3] });
      Plot.line(ctx, tr1, s.t, s.vout, { color: '#2563eb', lineWidth: 1.8 });
      Plot.label(ctx, 'Vout (V) — 目标 200V', tr1.pad.l, 14, { size: 12 });
      Plot.label(ctx, 't (s)', tr1.pad.w - 2, tr1.pad.h - 4, { size: 11, align: 'right' });

      const pad2 = { l: 48, r: 16, t: 52, b: 26, w, h };
      const tr2 = Plot.lin(0, tMax, 90, 140, pad2);
      Plot.grid(ctx, tr2, Plot.linTicks(0, tMax, 8), Plot.linTicks(95, 135, 8));
      Plot.line(ctx, tr2, s.t, s.fs.map(f => f / 1e3), { color: '#f59e0b', lineWidth: 1.8 });
      Plot.hline(ctx, tr2, 107, { color: '#94a3b8', dash: [4, 3] });
      Plot.label(ctx, 'fs 指令 (kHz) — 107 = fr', tr2.pad.l, tr2.pad.t - 8, { size: 12 });

      // 读数
      let peak = 0, iPeak = 0;
      for (let i = 800; i < s.t.length; i++) { const d = Math.abs(s.vout[i] - 200); if (d > peak) { peak = d; iPeak = i; } }
      let settle = -1;
      for (let i = iPeak; i < s.t.length; i++) if (Math.abs(s.vout[i] - 200) < 0.4 && settle < 0) settle = i;
      const fsEnd = s.fs[s.t.length - 1] / 1e3;
      ro.innerHTML =
        '<div class="ro-item"><span>峰值偏差</span><b>' + peak.toFixed(2) + ' V (' + (peak / 200 * 100).toFixed(1) + '%)</b></div>' +
        '<div class="ro-item"><span>恢复时间(±0.4V)</span><b>' + (settle >= 0 ? ((settle - 800) * s.t[1] * 1e3).toFixed(1) + ' ms' : '>20ms') + '</b></div>' +
        '<div class="ro-item"><span>稳态频率</span><b>' + fsEnd.toFixed(1) + ' kHz</b></div>';
    }
    draw();
  }

  /* ---------- 入口：扫描 [data-int] 并初始化 ---------- */
  function initAll() {
    document.querySelectorAll('[data-int]').forEach(host => {
      const kind = host.dataset.int;
      if (kind === 'gain-curve') initGainCurve(host);
      else if (kind === 'poles') initPoles(host);
      else if (kind === 'bode') initBode(host);
      else if (kind === 'stability') initStability(host);
      else if (kind === 'step-sim') initStepSim(host);
    });
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', initAll);
  else initAll();

  return { initAll };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = Int;
