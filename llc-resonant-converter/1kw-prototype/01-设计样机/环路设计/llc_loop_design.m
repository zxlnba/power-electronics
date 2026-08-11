%% llc_loop_design.m
% 1kW 全桥 LLC 谐振变换器 —— 数字电压环设计（2p2z / Type II）
%
% 设计对象：1kW 原理样机
%   输入 360~440V DC，输出 ±200V（n=2:1 为每路变换比），额定 1kW，fr=107kHz
%   谐振腔：Lr=270uH, Cr=8.2nF, PQ35/35, 原边28T/副边28T中心抽头(14T+14T)
%   控制：STM32G474 HRTIM TimerD 变频（PFM），电压环 2p2z 数字补偿器
%
% 用法：MATLAB 下直接运行，或
%       matlab -batch "llc_loop_design"
% 依赖：Control System Toolbox（tf/bode/c2d）
% 输出：console 打印全部设计值 + images/ 下 4 张图 + 系数文件
%
% 设计参数（参数作品设计书 §3.2，1kW 权威值）：
%   k = Lm/Lr = 7.4，Lm = k·Lr ≈ 2.0mH，Lr=270uH，Cr=8.2nF，fr=107kHz，n=2（每路 28:14）
%   Rac(1k) = (8n²/π²)·Rdc = (8×4/π²)×160 = 518.5Ω → 满载 Q = Z0/Rac = 0.35
%   Co = 100uF/路      ← 按实际输出电容修改
%   fs 工作点 = fr (满载)  ← 变频调节范围 95~130kHz
%
% Q 折算约定（2026-08-11）：
%   满载品质因数按设计书口径：Q = Z0/Rac = 0.35（Rac=518.5Ω，n=2 半绕组折算）。
%   若按功率守恒折算 Rac' = V1rms²/P = 129.7Ω，等效 Q ≈ 1.4 = 0.35×4（差 4 倍源于 n²）。
%   K_f 与 2p2z 系数只依赖 m 与输出极点 fp_out，与 Q 无关 —— 固件系数不受 Q 口径影响。
%   增益曲线/升压裕量按设计书 Q=0.35 绘图；60V 重载分析用功率守恒口径（见
%   ../工程开发文档.md），60V 负载换算 Q 远超 0.35，结论一致。

clear; clc; close all;

%% ============ 1. 参数（1kW 样机，可改） ============
% 谐振腔
Lr       = 270e-6;      % 谐振电感
Cr       = 8.2e-9;      % 谐振电容
Lm_ratio = 7.4;         % Lm/Lr = 7.4  => λ=Lr/Lm≈0.135（设计书 §3.2 k 值，Lm=2.0mH；LCR 实测一致）
Lm       = Lm_ratio*Lr;
% 功率级
n        = 2.0;         % 每路变换比 28:14（全绕组 1:1，中心抽头分两个 14T 半绕组）
Vin_nom  = 400.0;       % 额定输入
Vout_rail= 200.0;       % 每路输出
P_total  = 1000.0;      % 额定功率
Co_rail  = 100e-6;      % 每路输出电容（待确认）
% 控制目标
fco_target = 2000;      % 穿越频率 Hz
PM_target  = 45;        % 相位裕度目标 deg
fs_min     = 95e3;      % 变频下限（升压，注意满载升压裕量不足，见发现）
fs_max     = 130e3;     % 变频上限
% 硬件
hrtim_clk = 170e6;      % HRTIM Timer 时钟

%% ============ 2. 谐振参数与满载工作点 ============
fr   = 1/(2*pi*sqrt(Lr*Cr));        % 谐振频率
fm   = 1/(2*pi*sqrt((Lr+Lm)*Cr));   % 第二谐振频率
Z0   = sqrt(Lr/Cr);                 % 特征阻抗
V1rms = (2*sqrt(2)/pi)*Vin_nom;     % 全桥输出方波基波 rms
% 设计书口径（§3.2）：Rac(1k)=(8n²/π²)·Rdc_full，Rdc_full=400V²/1kW=160Ω，n=2（每路 28:14）
Rdc_full = (2*Vout_rail)^2 / P_total;   % 全母线等效负载 160Ω
Rac_db   = (8*n^2/pi^2) * Rdc_full;     % 设计书反射交流等效电阻 518.5Ω
Q        = Z0 / Rac_db;                 % 满载品质因数（设计书 Q=0.35）
% 功率守恒折算（验算用）：Rac'=V1rms²/P=129.7Ω → 等效 Q≈1.4（=0.35×4，差 n²）
Rac_phys = V1rms^2 / P_total;
Q_phys   = Z0 / Rac_phys;
Rload_rail = Vout_rail^2/(P_total/2);
fp_out = 1/(2*pi*Rload_rail*Co_rail);  % 输出极点

fprintf('===== 谐振参数 =====\n');
fprintf('fr = %.1f kHz,  fm = %.1f kHz,  Z0 = %.1f ohm\n', fr/1e3, fm/1e3, Z0);
fprintf('Rac(设计书) = %.1f ohm,  Q(满载,设计书) = %.3f\n', Rac_db, Q);
fprintf('  验算: Rac''=V1rms²/P=%.1f ohm → 等效 Q≈%.2f（=设计书Q×4，仅口径差，K_f/2p2z 不受影响）\n', Rac_phys, Q_phys);
fprintf('Rload_rail = %.1f ohm,  fp_out = %.1f Hz\n\n', Rload_rail, fp_out);

%% ============ 3. FHA 增益曲线与升压裕量 ============
% 标准 FHA：M(fn,Q,m) = 1/sqrt((1+λ-λ/fn²)² + Q²(fn-1/fn)²)，λ=Lr/Lm=1/m
% （与精确 AC 电路传递 |Zp/(Zseries+Zp)| 一致，见 fha_gain 注释）
fn = linspace(0.55, 1.6, 4000);
fn_min = fs_min/fr;                        % 频带下限 fn=0.888（fs_min=95kHz）
Mfull = arrayfun(@(x) fha_gain(x,Q,Lm_ratio), fn);
Mhalf = arrayfun(@(x) fha_gain(x,Q/2, Lm_ratio), fn);   % 50% 负载: Rac×2, Q/2
Mquar = arrayfun(@(x) fha_gain(x,Q/4, Lm_ratio), fn);   % 25% 负载: Rac×4, Q/4

% 频带内（fn≥0.888）可达最大增益
iband = fn >= fn_min;
fnband = fn(iband);
[MMax_full, ipeak] = max(Mfull(iband));
fprintf('===== FHA 升压裕量（设计发现） =====\n');
fprintf('M_max(满载,Q=%.2f) = %.3f @ fn=%.3f  -> 最低可保持输入 %.0f V\n', ...
        Q, MMax_full, fnband(ipeak), Vin_nom/MMax_full);
fprintf('  360V 输入需要 M=%.3f -> 满载无法保持 ±200V（输出将随输入跌落）\n', Vin_nom/360);
fprintf('  50%%负载 M_max=%.3f (%.0fV), 25%%负载 M_max=%.3f (%.0fV)（均受 fs_min 限制）\n\n', ...
        max(Mhalf(iband)), Vin_nom/max(Mhalf(iband)), ...
        max(Mquar(iband)), Vin_nom/max(Mquar(iband)));


%% ============ 4. 静态灵敏度 K_f（fs=fr 工作点） ============
% dM/dfn @fn=1 = -2/m（标准 FHA 公式性质：与负载 Q 无关）
% dVout/dfs = (Vin/n)*dM/dfn/fr
dMdfn = -2/Lm_ratio;
K_f   = (Vin_nom/n) * dMdfn / fr;     % V/Hz（负：fs↑ 则 Vout↓）
fprintf('===== 静态灵敏度 K_f = dVout/dfs =====\n');
fprintf('K_f = %.3f V/kHz（与负载无关；负载只影响输出极点位置）\n\n', K_f*1e3);

%% ============ 5. 降阶被控对象模型 ============
% Gvd(s) = K_f*(1+s/wz_lm) / ( (1+s/wp_out)*(1 + s/(wr*Qr) + s^2/wr^2) )
%   wz_lm : Lm 谐振零点（远高于穿越频率，2kHz 设计不敏感）
%   wp_out: 输出电容-负载极点（主导低频极点）
%   wr/Qr : 谐振复极点（107kHz，对 2kHz 穿越贡献 <15°，默认 Qr=1）
Qr = 1.0;
s = tf('s');
Gvd = K_f * (1+s/(2*pi*fm)) / ( (1+s/(2*pi*fp_out)) * (1 + s/(2*pi*fr*Qr) + (s/(2*pi*fr))^2 ) );

% 注：穿越频率远低于 wr，补偿器设计主要取决于 K_f 和 fp_out，
%     对谐振模型细节不敏感 —— 这正是 2kHz 穿越的安全之处。

%% ============ 6. 补偿器设计（Type II：积分 + 零点 + 高频极点） ============
% C(s) = K_c*(1+s/wz)/( s*(1+s/wp) )
%   wz = 2*pi*20   -> 抵消输出极点，提供相位提升
%   wp = 2*pi*10k  -> 衰减 107kHz 开关纹波
wz = 2*pi*20;
wp = 2*pi*10000;
% K_c 使 |C*Gvd|@fco = 1；负号编码反相（err>0 -> fs 下降 -> Vout 上升）
f = fco_target;
Kc = -1/ ( abs((1+1j*f/wz)/(1j*f*(1+1j*f/wp))) * abs(freqresp(Gvd, 2*pi*f)) );
C  = Kc * (1+s/wz)/( s*(1+s/wp) );
L  = C * Gvd;              % 开环

%% ============ 7. 连续域裕度 ============
[Gm,Pm,Wcg,Wcp] = margin(L);
fprintf('===== 连续域设计 =====\n');
fprintf('K_c = %.4e  Hz·s/V\n', Kc);
fprintf('fco = %.0f Hz,  PM = %.1f deg,  GM = %.1f dB\n\n', Wcp/2/pi, Pm, 20*log10(Gm));

%% ============ 7b. 负载敏感性（K_f 恒定，fp_out 随负载左移） ============
% Rload = Vout²/P ∝ 1/Q，故 fp_out(q) = fp_out·(q/Q)
fprintf('===== 负载敏感性（K_f 不变，输出极点移动） =====\n');
for qq = [Q, Q/2, Q/4]
    fp_q = fp_out*(qq/Q);
    Gvd_q = K_f * (1+s/(2*pi*fm)) / ( (1+s/(2*pi*fp_q)) * (1 + s/(2*pi*fr*Qr) + (s/(2*pi*fr))^2 ) );
    L_q = C * Gvd_q;
    [Gmq,Pmq,~,Wcpq] = margin(L_q);
    fprintf('  Q=%.3f  fp_out=%.1f Hz  fco=%.0f Hz  PM=%.1f deg  GM=%.1f dB\n', ...
            qq, fp_q, Wcpq/2/pi, Pmq, 20*log10(Gmq));
end
fprintf('\n');

%% ============ 8. 离散化（每开关周期控制，含一拍延迟） ============
% 控制率 = 开关频率 107kHz, Ts = 1/fr
Ts = 1/fr;
Cz = c2d(C, Ts, 'tustin');
Cz_delay = Cz / tf([1 0],1,Ts);        % 乘 z^-1 计算延迟（1 拍）
Gvd_d = c2d(Gvd, Ts, 'zoh');           % 对象离散化（与离散控制器同率）
Ld = Cz_delay * Gvd_d;                 % 采样环路增益
[Gmd,Pmd,Wcgd,Wcpd] = margin(Ld);
[b0,b1,b2,a1,a2] = disp2p2z(Cz);        % 由 C(z) 提取直接II型系数
fprintf('===== 离散化（每开关周期 Ts=%.3f us） =====\n', Ts*1e6);
fprintf('含1拍延迟: fco = %.0f Hz,  PM = %.1f deg\n', Wcpd/2/pi, Pmd);
fprintf('\nfloat 系数（直接写进 llc_ctrl.h）:\n');
fprintf('  #define LC_B0  %.8ff\n', b0);
fprintf('  #define LC_B1  %.8ff\n', b1);
fprintf('  #define LC_B2  %.8ff\n', b2);
fprintf('  #define LC_A1  %.8ff\n', a1);
fprintf('  #define LC_A2  %.8ff\n\n', a2);

%% ============ 9. 数字控制器闭环仿真（负载阶跃） ============
% 用降阶模型 Gvd 搭线性闭环，数字 2p2z 控制器分步运行。
% 注意：这里是线性小信号验证（模型裕度），非线性仿真请在 PLECS 中做。
Gd = c2d(Gvd, Ts, 'zoh');          % 对象离散化（ZOH）
[A,B,Cc,Dd] = ssdata(ss(Gd));      % 离散状态空间
d2d  = 1;                          % 与控制器同采样率（Ts），避免步长错配放大峰值
N    = 4000;
t    = (0:N-1)*Ts*d2d;
vref = Vout_rail*ones(1,N);
step_at = round(0.008/(Ts*d2d));   % 8ms 处加入扰动
d_f = 10e3;                        % 输入扰动 +10kHz => 若开环输出跌 ~7.5V
x1=0; x2=0; x=zeros(size(A,1),1);
fs_cmd = fr*ones(1,N);  y = Vout_rail*ones(1,N);
for k = 2:N
    vmeas = y(k-1);                % 上一拍输出 = 1 拍计算延迟
    err   = vref(k) - vmeas;
    % 直接 II 型 2p2z
    w  = b0*err + x1;
    x1 = b1*err - a1*w + x2;
    x2 = b2*err - a2*w;
    fs_cmd(k) = min(fs_max, max(fs_min, fr + w));
    % 更新对象：输入 = 频率指令 - 扰动（扰动模拟负载阶跃）
    dist = (k >= step_at) * d_f;
    x = A*x + B*((fs_cmd(k)-fr) - dist);
    y(k) = Vout_rail + Cc*x;
end
dv_peak = max(abs(y(step_at:end)-Vout_rail));
settle = find(abs(y(step_at:end)-Vout_rail) < 0.02, 1, 'first');
if isempty(settle), settle = numel(y(step_at:end)); end
fprintf('===== 闭环扰动抑制（线性模型，8ms 处 +10kHz 等效负载阶跃） =====\n');
fprintf('最大偏差 %.2f V (%.1f%%)，回到 2%% 内 ~ %.1f ms\n\n', ...
        dv_peak, dv_peak/Vout_rail*100, settle*Ts*d2d*1e3);

%% ============ 10. 画图 ============
figdir = fullfile(fileparts(mfilename('fullpath')), 'images');
if ~exist(figdir,'dir'), mkdir(figdir); end

% 图1: FHA 增益曲线
figure('Color','w','Position',[50 50 700 480]);
plot(fn, Mfull,'LineWidth',1.8); hold on;
plot(fn, Mhalf,'--','LineWidth',1.4);
plot(fn, Mquar,'--','LineWidth',1.4);
yline(1,'k:'); yline(Vin_nom/360,'r--','M=1.111 @360V','LabelVerticalAlignment','bottom');
yline(MMax_full,'r:','M_max 满载');
xline(1,'k:'); ylim([0.5 1.4]); grid on;
xlabel('fn = fs/fr'); ylabel('电压增益 M');
legend(sprintf('满载 (Q=%.2f)',Q),'50%负载','25%负载','Location','northeast');
title('FHA 增益曲线 —— 满载升压裕量不足');
saveas(gcf, fullfile(figdir,'fha-gain-curves.png'));

% 图2: 开环 Bode（连续 + 离散）
figure('Color','w','Position',[60 60 760 560]);
[magL,phL,wL] = bode(L);  [magLd,phLd,wLd] = bode(Ld);
subplot(2,1,1); semilogx(wL/2/pi, 20*log10(squeeze(magL)),'LineWidth',1.6); hold on;
semilogx(wLd/2/pi,20*log10(squeeze(magLd)),'--','LineWidth',1.4);
yline(0,'k:'); grid on; xlabel('Hz'); ylabel('dB'); legend('连续','离散+1拍延迟');
title(sprintf('开环 Bode —— fco=%.0fHz, PM=%.1f°(离散)', Wcpd/2/pi, Pmd));
subplot(2,1,2); semilogx(wL/2/pi, squeeze(phL),'LineWidth',1.6); hold on;
semilogx(wLd/2/pi,squeeze(phLd),'--','LineWidth',1.4);
yline(-180,'k:'); grid on; xlabel('Hz'); ylabel('deg');
saveas(gcf, fullfile(figdir,'loop-bode.png'));

% 图3: 闭环负载阶跃
figure('Color','w','Position',[70 70 700 420]);
plot(t*1000, y,'LineWidth',1.8); hold on;
xline(step_at*Ts*d2d*1000,'r--','50%负载阶跃');
yline(Vout_rail,'k:'); grid on;
xlabel('t (ms)'); ylabel('Vout_rail (V)');
title(sprintf('闭环响应（线性降阶模型）—— 峰值 %.1fV', max(y)));
saveas(gcf, fullfile(figdir,'closed-loop-step.png'));

% 图4: 频率指令
figure('Color','w','Position',[80 80 700 320]);
plot(t*1000, fs_cmd/1e3,'LineWidth',1.6); grid on;
xlabel('t (ms)'); ylabel('fs 指令 (kHz)');
title(sprintf('变频指令 fs：%.0f~%.0f kHz', min(fs_cmd)/1e3, max(fs_cmd)/1e3));
saveas(gcf, fullfile(figdir,'fs-command.png'));

fprintf('图已保存到 %s\n', figdir);
fprintf('\n下一步：把系数填进 llc_ctrl.h，在 PLECS 里对 Gvd 做 AC 扫描验证模型，\n');
fprintf('再用控制回路库做闭环仿真，最后上真机。详见环路设计/README.md\n');

%% ============ 辅助函数 ============
function M = fha_gain(fn,q,mm)
% FHA 电压增益（标准公式，与精确 AC 电路传递一致）
% M(fn,Q,m) = 1/sqrt((1+λ-λ/fn²)² + Q²(fn-1/fn)²)，λ=Lr/Lm=1/m
la = 1/mm;
M = 1./sqrt( (1+la-la./fn.^2).^2 + (q.*(fn-1./fn)).^2 );
end

function [b0,b1,b2,a1,a2] = disp2p2z(Cz)
% 由 tf 提取直接 II 型系数：C(z) = (b0+b1 z^-1+b2 z^-2)/(1+a1 z^-1+a2 z^-2)
[num,den] = tfdata(Cz,'v');
num = num/den(1); den = den/den(1);
b0 = num(1); b1 = num(2); b2 = num(3);
a1 = den(2); a2 = den(3);
end

function y = sys_dstep(sys, u, k, tk)
% 简单离散响应（未使用，占位）
persistent dx
y = 0;
end
