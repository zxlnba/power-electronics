% LLC 谐振腔 60V/300W 缩尺设计（换新变压器）
%
% 背景：现谐振腔按额定 400V/1kW 设计（Z0=181.5Ω，设计书 Q=0.35），60V 下负载
%       按功率守恒折算 Q=2~8（物理口径），高 Q 增益封顶 M≈1.0 且无升压区
%       （实测 M≈0.8），60V 根本跑不出功率。
%       要用 60V/300W 电源跑真实功率，必须单独设计 60V 谐振腔。
%
% 60V/300W 专用腔设计：
%   Lr = 7µH, Cr = 470nF, Lm = 24µH, fr ≈ 87.7kHz, Z0 ≈ 3.86Ω, h=Lm/Lr=3.43
%   满载 60V/300W：R_load=12Ω → Rac=(8/π²)·12=9.73Ω → Q≈0.40（功率守恒物理值）
%   **必须换新变压器**：现 PQ35 原边 Lm=2mH，本设计需 Lm≈24µH（差 80 倍），
%   旧变压器不可用。新变压器 1:1、Lm≈24µH（如 PQ35 重绕 + 气隙，或小 E 磁芯）。
%
% Q 口径：本 60V 缩尺设计用功率守恒物理口径 Rac=V1rms²/P（n=1 全绕组），
%   满载 Q≈0.40。1kW 额定点用设计书 §3.2 口径 Q=0.35（Rac=(8n²/π²)·Rdc=518.5Ω，
%   n=2 每路），两口径差 n²=4 倍，只影响 Q 数值标签；增益曲线/升压裕量/固件系数
%   与 Q 口径无关（K_f、2p2z 只依赖 m 与 fp_out）。60V 现腔实测增益塌是硬件事实，
%   不因口径而异（见 01-设计样机/工程开发文档.md §3.2 说明）。
%
% 用法：matlab -batch "llc_rescale"
clear; clc;

%% ============ 方案A 参数 ============
Lr_new  = 7e-6;
Cr_new  = 470e-9;
Lm_new  = 24e-6;            % 新变压器励磁电感（1:1）
lam     = Lr_new/Lm_new;    % λ=0.2915
fr_new  = 1/(2*pi*sqrt(Lr_new*Cr_new));   % 87.7kHz
Z0_new  = sqrt(Lr_new/Cr_new);            % 3.86 ohm
Vin     = 60.0;
V1rms   = (2*sqrt(2)/pi)*Vin;             % 全桥基波 rms = 54V
R2Q     = @(R) Z0_new/(0.8106*R);         % 1:1 整绕组：Rac=(8/π²)·R
M       = @(la,q,fn) 1./sqrt((1+la-la./fn.^2).^2 + q.^2.*(fn-1./fn).^2);

fprintf('===== 方案A（60V/300W 专用腔） =====\n');
fprintf('Lr=%.1fuH  Cr=%.0fnF  Lm=%.0fuH  fr=%.1fkHz  Z0=%.2fΩ  λ=%.4f (h=%.2f)\n', ...
        Lr_new*1e6, Cr_new*1e9, Lm_new*1e6, fr_new/1e3, Z0_new, lam, 1/lam);

%% ============ 满载工作点（300W） ============
Rfull = Vin^2/300;                 % 12 ohm
Rac_f = 0.8106*Rfull;              % 9.73 ohm
Qfull = Z0_new/Rac_f;              % 0.397
fprintf('满载 60V/300W: R_load=%.1fΩ  Rac=%.2fΩ  Q=%.2f\n\n', Rfull, Rac_f, Qfull);

%% ============ 功率阶梯（谐振点 M=1） ============
loads = {12,'12Ω  (满载)',300; 15,'15Ω',240; 20,'20Ω',180; ...
         30,'30Ω',120; 46,'46Ω (现有电阻)',78};
fprintf('===== 60V 功率阶梯（60V, fs=fr=87.7k, M=1） =====\n');
fprintf('%-22s %6s %6s %8s %7s\n','负载','R','Q','P','Rac');
for i=1:size(loads,1)
  R = loads{i,1};
  fprintf('%-22s %6.0f %6.2f %8.0f %7.2f\n', loads{i,2}, R, R2Q(R), 60^2/R, 0.8106*R);
end

%% ============ 增益曲线（60V 专用腔，各负载） ============
fs = (65:0.5:115)*1e3;  fn = fs/fr_new;
figure('Color','w','Position',[60 60 900 520]);
plot(fs/1e3, M(lam, Qfull, fn), 'LineWidth',1.9); hold on;
for R=[15 20 30]
  plot(fs/1e3, M(lam, R2Q(R), fn), '--','LineWidth',1.3);
end
xline(fr_new/1e3,'k--','fr=87.7k'); yline(1,'k:','M=1'); grid on;
xlabel('fs [kHz]'); ylabel('M'); ylim([0 1.4]);
legend({'满载 12Ω (Q=0.40)','15Ω (Q=0.32)','20Ω (Q=0.24)','30Ω (Q=0.16)'},'Location','best');
title('60V/300W 缩尺腔 (Lr=7uH/Cr=470nF/Lm=24uH) 增益曲线 —— 满载 Q=0.4 平坦可调');
saveas(gcf, 'llc_rescale_60v300w.png');

%% ============ 满载增益 vs fs（看调压范围） ============
fprintf('\n===== 60V 满载(12Ω) 各频率增益（60V 输出=60V×M） =====\n');
fprintf('%8s %8s %9s\n','fs[kHz]','M','Vout');
for f=[65 70 75 80 87.7 95 100 110 115]
  fprintf('%8.0f %8.3f %9.1f\n', f, M(lam, Qfull, f*1e3/fr_new), Vin*M(lam, Qfull, f*1e3/fr_new));
end
fprintf('\n结论：满载 Q≈0.4，曲线平缓（65k 时 M≈1.25 有升压余量，>87.7k 平滑降压），\n');
fprintf('      扫频不塌、软启平滑、闭环留有余量。代价：换 Lr/Cr + 新变压器（Lm≈24µH）。\n');
fprintf('      固件适配：LC_FS_NOM≈87.7k，范围约 65~115k，LC_K_GAIN_SCALE 按 Vin 重算。\n');
