% LLC 谐振腔 60V/300W 缩尺样机 —— 实际到手器件参数校核与增益计算
%
% 实际到手器件（2026-08-18 实测为准）：
%   Lr = 8.2µH, Cr = 470nF, Lm = 26µH（新变压器 PQ35 1:1、6T、气隙 0.32mm）
%   fr 计算值 ≈ 81kHz；实测 fr ≈ 75kHz（增益=1，差 6-8% 属寄生/容差）
%   Z0 = sqrt(Lr/Cr) ≈ 4.18Ω，励磁比 k = Lm/Lr ≈ 3.2
%   满载 60V/300W：R_load=12Ω → Rac=(8/π²)·12=9.73Ω → Q ≈ 0.43
%
% 增益关系：全桥 LLC，Vout = M × Vin（变压器 1:1）
% 实测基准：fr≈75kHz 增益=1（60V/75k 副边满摆 60V）；100kHz ≈50V（M≈0.85）
%   副边整流前方波增益健康；输出压降根因=整流管 RHRP860 VF，换 MBR10100（见 README §3）
%
% 用法：matlab -batch "llc_rescale"
clear; clc;

%% ============ 实际到手器件参数 ============
Lr  = 8.2e-6;
Cr  = 470e-9;
Lm  = 26e-6;            % 变压器励磁电感（PQ35 1:1、6T、气隙 0.32mm）
lam = Lr/Lm;            % λ ≈ 0.3154
fr  = 1/(2*pi*sqrt(Lr*Cr));   % 计算值 ≈ 81kHz（实测 ≈75kHz）
Z0  = sqrt(Lr/Cr);            % ≈ 4.18 ohm
Vin = 60.0;
V1rms = (2*sqrt(2)/pi)*Vin;   % 全桥基波 rms = 54V
R2Q = @(R) Z0/(0.8106*R);     % 1:1 整绕组：Rac=(8/π²)·R
M   = @(la,q,fn) 1./sqrt((1+la-la./fn.^2).^2 + q.^2.*(fn-1./fn).^2);

fprintf('===== 60V/300W 缩尺腔（实际到手器件） =====\n');
fprintf('Lr=%.1fuH  Cr=%.0fnF  Lm=%.0fuH  fr(计算)=%.1fkHz(实测≈75k)  Z0=%.2fΩ  λ=%.4f (k=%.2f)\n', ...
        Lr*1e6, Cr*1e9, Lm*1e6, fr/1e3, Z0, lam, 1/lam);

%% ============ 满载工作点（300W） ============
Rfull = Vin^2/300;          % 12 ohm
Rac_f = 0.8106*Rfull;       % 9.73 ohm
Qfull = Z0/Rac_f;           % 0.43
fprintf('满载 60V/300W: R_load=%.1fΩ  Rac=%.2fΩ  Q=%.2f\n\n', Rfull, Rac_f, Qfull);

%% ============ 功率阶梯（谐振点 M=1） ============
loads = {12,'12Ω  (满载)',300; 15,'15Ω',240; 20,'20Ω',180; ...
         30,'30Ω',120; 46,'46Ω (现有电阻)',78};
fprintf('===== 60V 功率阶梯（60V, M=1） =====\n');
fprintf('%-22s %6s %6s %8s %7s\n','负载','R','Q','P','Rac');
for i=1:size(loads,1)
  R = loads{i,1};
  fprintf('%-22s %6.0f %6.2f %8.0f %7.2f\n', loads{i,2}, R, R2Q(R), 60^2/R, 0.8106*R);
end

%% ============ 增益曲线（各负载） ============
fs = (60:0.5:115)*1e3;  fn = fs/fr;
figure('Color','w','Position',[60 60 900 520]);
plot(fs/1e3, M(lam, Qfull, fn), 'LineWidth',1.9); hold on;
for R=[15 20 30]
  plot(fs/1e3, M(lam, R2Q(R), fn), '--','LineWidth',1.3);
end
xline(fr/1e3,'k--','fr(计算)≈81k'); xline(75,'k:','fr(实测)≈75k'); yline(1,'k:','M=1'); grid on;
xlabel('fs [kHz]'); ylabel('M'); ylim([0 1.4]);
legend({'满载 12Ω (Q≈0.43)','15Ω (Q≈0.34)','20Ω (Q≈0.26)','30Ω (Q≈0.17)'},'Location','best');
title('60V/300W 缩尺腔 (Lr=8.2uH/Cr=470nF/Lm=26uH) 增益曲线');
saveas(gcf, 'LLC_gain_curves_Lm26uH.png');

%% ============ 满载增益 vs fs（看调压范围） ============
fprintf('\n===== 60V 满载(12Ω) 各频率增益（Vout=60V×M） =====\n');
fprintf('%8s %8s %9s\n','fs[kHz]','M','Vout');
for f=[65 70 75 80 85 90 95 100 110]
  fprintf('%8.0f %8.3f %9.1f\n', f, M(lam, Qfull, f*1e3/fr), Vin*M(lam, Qfull, f*1e3/fr));
end
fprintf('\n实测基准：fr≈75kHz 增益=1（副边满摆 60V）；100kHz ≈50V（M≈0.85）。\n');
fprintf('副边整流前方波增益健康；输出压降根因=整流管 RHRP860 VF，换 MBR10100（见 README §3）。\n');
