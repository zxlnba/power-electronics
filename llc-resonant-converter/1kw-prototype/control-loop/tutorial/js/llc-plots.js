/* llc-plots.js — Canvas 绘图工具（零依赖）
 * 提供半对数/线性坐标、网格、折线、标注等原语，供各交互组件使用
 */
'use strict';

const Plot = (function () {
  const DPR = window.devicePixelRatio || 1;

  /* 初始化 canvas 的物理尺寸，返回 {ctx, w, h}（逻辑像素） */
  function setup(canvas, w, h) {
    canvas.width = w * DPR;
    canvas.height = h * DPR;
    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
    const ctx = canvas.getContext('2d');
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    return { ctx, w, h };
  }

  function clear(ctx, w, h, bg = '#ffffff') {
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, w, h);
  }

  /* 坐标变换对象：
     linX / logX: 把数据坐标映射为像素；xPx: 反变换 */
  function lin(xMin, xMax, yMin, yMax, pad) {
    if (!isFinite(pad.w) || !isFinite(pad.h))
      throw new Error('Plot.lin: pad 缺少 w/h（需传 canvas 尺寸，见 setup 返回值）');
    return {
      pad,
      x: x => pad.l + (x - xMin) / (xMax - xMin) * (pad.w - pad.l),
      y: y => pad.h - pad.b - (y - yMin) / (yMax - yMin) * (pad.h - pad.b - pad.t),
      xPx: px => xMin + (px - pad.l) / (pad.w - pad.l) * (xMax - xMin),
      yPx: py => yMin + (pad.h - pad.b - py) / (pad.h - pad.b - pad.t) * (yMax - yMin)
    };
  }
  function log(xMin, xMax, yMin, yMax, pad) {
    if (!isFinite(pad.w) || !isFinite(pad.h))
      throw new Error('Plot.log: pad 缺少 w/h（需传 canvas 尺寸，见 setup 返回值）');
    const L0 = Math.log10(xMin), L1 = Math.log10(xMax);
    return {
      pad,
      x: x => pad.l + (Math.log10(x) - L0) / (L1 - L0) * (pad.w - pad.l),
      y: y => pad.h - pad.b - (y - yMin) / (yMax - yMin) * (pad.h - pad.b - pad.t),
      xPx: px => Math.pow(10, L0 + (px - pad.l) / (pad.w - pad.l) * (L1 - L0)),
      yPx: py => yMin + (pad.h - pad.b - py) / (pad.h - pad.b - pad.t) * (yMax - yMin)
    };
  }

  /* 画网格。xTicks: 数组（半对数下给 decade 幂次）。 */
  function grid(ctx, tr, xTicks, yTicks, opts = {}) {
    const c = opts.color || '#e8eef4';
    const lw = opts.lineWidth || 1;
    ctx.strokeStyle = c;
    ctx.lineWidth = lw;
    ctx.beginPath();
    for (const t of xTicks) {
      const px = tr.x(t);
      ctx.moveTo(px, tr.pad.h - tr.pad.b);
      ctx.lineTo(px, tr.pad.t);
    }
    for (const t of yTicks) {
      const py = tr.y(t);
      ctx.moveTo(tr.pad.l, py);
      ctx.lineTo(tr.pad.w, py);
    }
    ctx.stroke();
    // 边框
    ctx.strokeStyle = '#b8c4cf';
    ctx.strokeRect(tr.pad.l, tr.pad.t, tr.pad.w - tr.pad.l, tr.pad.h - tr.pad.b - tr.pad.t);
  }

  function line(ctx, tr, xs, ys, opts = {}) {
    const c = opts.color || '#2563eb';
    const lw = opts.lineWidth || 2;
    const dash = opts.dash || [];
    ctx.strokeStyle = c;
    ctx.lineWidth = lw;
    ctx.setLineDash(dash);
    ctx.beginPath();
    for (let i = 0; i < xs.length; i++) {
      const px = tr.x(xs[i]), py = tr.y(ys[i]);
      if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
    }
    ctx.stroke();
    ctx.setLineDash([]);
  }

  function hline(ctx, tr, y, opts = {}) {
    const c = opts.color || '#f43f5e';
    const lw = opts.lineWidth || 1.2;
    const dash = opts.dash || [4, 4];
    ctx.strokeStyle = c;
    ctx.lineWidth = lw;
    ctx.setLineDash(dash);
    ctx.beginPath();
    ctx.moveTo(tr.pad.l, tr.y(y));
    ctx.lineTo(tr.pad.w, tr.y(y));
    ctx.stroke();
    ctx.setLineDash([]);
  }
  function vline(ctx, tr, x, opts = {}) {
    const c = opts.color || '#f43f5e';
    const lw = opts.lineWidth || 1.2;
    const dash = opts.dash || [4, 4];
    ctx.strokeStyle = c;
    ctx.lineWidth = lw;
    ctx.setLineDash(dash);
    ctx.beginPath();
    ctx.moveTo(tr.x(x), tr.pad.t);
    ctx.lineTo(tr.x(x), tr.pad.h - tr.pad.b);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  function mark(ctx, tr, x, y, opts = {}) {
    const c = opts.color || '#f43f5e';
    ctx.fillStyle = c;
    const px = tr.x(x), py = tr.y(y);
    ctx.beginPath();
    ctx.arc(px, py, opts.r || 4, 0, Math.PI * 2);
    ctx.fill();
  }

  function label(ctx, text, x, y, opts = {}) {
    const c = opts.color || '#334155';
    ctx.fillStyle = c;
    ctx.font = (opts.size || 12) + 'px ' + (opts.font || 'system-ui, "Microsoft YaHei", sans-serif');
    ctx.textAlign = opts.align || 'left';
    ctx.textBaseline = opts.baseline || 'alphabetic';
    ctx.fillText(text, x, y);
  }

  /* 半对数轴刻度生成：从 1e3 的 decade 建议值 */
  function logTicks(fMin, fMax) {
    const ticks = [];
    for (let d = Math.ceil(Math.log10(fMin)); d <= Math.floor(Math.log10(fMax)); d++)
      for (let m = 1; m <= 9; m++) { const v = m * Math.pow(10, d); if (v >= fMin && v <= fMax) ticks.push(v); }
    return ticks;
  }
  function linTicks(min, max, n) {
    const step = (max - min) / n, ticks = [];
    for (let v = min; v <= max + 1e-9; v += step) ticks.push(v);
    return ticks;
  }

  return { setup, clear, lin, log, grid, line, hline, vline, mark, label, logTicks, linTicks };
})();

if (typeof module !== 'undefined' && module.exports) module.exports = Plot;
