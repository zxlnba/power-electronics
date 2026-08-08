%% llc_loop_design.m
% 1kW 全桥 LLC 谐振变换器 —— 数字电压环设计（2p2z / Type II）
%
% 设计对象：1kW 原理样机
%   输入 360~440V DC，输出 ±200V（n=2:1），额定 1kW，fr=107kHz
%   谐振腔：Lr=270uH, Cr=8.2nF, PQ35/35, 原边28T/副边14T+14T
%   控制：STM32G474 HRTIM TimerD 变频（PFM），电压环 2p2z 数字补偿器
%
% 用法：MATLAB 下直接运行，或
%       matlab -batch "llc_loop_design"
% 依赖：Control System Toolbox（tf/bode/c2d）
% 输出：console 打印全部设计值 + control-loop/images/ 下 4 张图 + 系数文件
%
% 重要假设（请按实测修改）：
%   Lm = 5*Lr (k=6)   ← 必须用 LCR 表实测变压器原边电感，改 Lm_ratio
%   Co = 100uF/路      ← 按实际输出电容修改
%   fs 工作点 = fr (满载)  ← 变频调节范围 95~130kHz

clear; clc; close all;

%% ============ 1. 参数（1kW 样机，可改） ============
% 谐振腔
Lr       = 270e-6;      % 谐振电感
Cr       = 8.2e-9;      % 谐振电容
Lm_ratio = 5.0;         % Lm/Lr = 5  => k=(Lr+Lm)/Lr=6  (待实测)
Lm       = Lm_ratio*Lr;
% 功率级
n        = 2.0;         % 变比 28:14
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
Rac  = V1rms^2 / P_total;           % 反射交流等效电阻
Q    = Z0/Rac;                      % 品质因数（满载）
Rload_rail = Vout_rail^2/(P_total/2);
fp_out = 1/(2*pi*Rload_rail*Co_rail);  % 输出极点

fprintf('===== 谐振参数 =====\n');
fprintf('fr = %.1f kHz,  fm = %.1f kHz,  Z0 = %.1f ohm\n', fr/1e3, fm/1e3, Z0);
fprintf('Rac'' = %.1f ohm,  Q(满载) = %.3f\n', Rac, Q);
fprintf('Rload_rail = %.1f ohm,  fp_out = %.1f Hz\n\n', Rload_rail, fp_out);

%% ============ 3. FHA 增益曲线与升压裕量 ============
% 标准 FHA：M(fn,Q,m) = 1/sqrt((1+λ-λ/fn²)² + Q²(fn-1/fn)²)，λ=Lr/Lm=1/m
% （与精确 AC 电路传递 |Zp/(Zseries+Zp)| 一致，见 fha_gain 注释）
fn = linspace(0.55, 1.6, 4000);
Mfull = arrayfun(@(x) fha_gain(x,Q,Lm_ratio), fn);
Mhalf = arrayfun(@(x) fha_gain(x,Q/2, Lm_ratio), fn);   % 50% 负载: Rac×2, Q/2
Mquar = arrayfun(@(x) fha_gain(x,Q/4, Lm_ratio), fn);   % 25% 负载: Rac×4, Q/4

[MMax_full, ipeak] = max(Mfull);
fprintf('===== FHA 升压裕量（设计发现） =====\n');
fprintf('M_max(满载,Q=%.2f) = %.3f @ fn=%.3f  -> 最低可保持输入 %.0f V\n', ...
        Q, MMax_full, fn(ipeak), Vin_nom/MMax_full);
fprintf('  360V 输入需要 M=%.3f -> 满载无法保持 ±200V（输出将随输入跌落）\n', Vin_nom/360);
fprintf('  50%%负载 M_max=%.3f (%.0fV), 25%%负载 M_max=%.3f (%.0fV)\n\n', ...
        max(Mhalf), Vin_nom/max(Mhalf), max(Mquar), Vin_nom/max(Mquar));

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
fprintf('\nfloat 系数（直接写进 llc_controller.h）:\n');
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
legend('满载 (Q=1.4)','50%负载','25%负载','Location','northeast');
title('FHA 增益曲线 —— 满载升压裕量不足（<1.02）');
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
fprintf('\n下一步：把系数填进 llc_controller.h，在 PLECS 里对 Gvd 做 AC 扫描验证模型，\n');
fprintf('再用控制回路库做闭环仿真，最后上真机。详见 control-loop/README.md\n');

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
