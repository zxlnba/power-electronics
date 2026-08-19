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

%% ============ 增益曲线（一张图两面板）：左=不同 k 比对选取 Lm，右=各带载 ============
fs = (60:0.5:115)*1e3;  fn = fs/fr;
ks  = [2.5 3.0 3.2 3.5 4.0 5.0];       % k = Lm/Lr 候选
cols = [0.55 0.65 0.75; 0.35 0.55 0.70; 0.85 0.15 0.1; 0.9 0.6 0.0; 0.6 0.25 0.7; 0.4 0.4 0.4];
figure('Color','w','Position',[40 60 1400 540]);

%% 左面板：不同 k 增益曲线 —— 选取 Lm（满载 12Ω）
subplot(1,2,1); hold on;
for i=1:numel(ks)
  lam_i = 1/ks(i);
  if ks(i)==3.2   % 选定的 Lm=26µH（PQ35 气隙 0.32mm）
    plot(fs/1e3, M(lam_i, Qfull, fn), '-', 'LineWidth', 2.6, 'Color', cols(3,:));
  else
    plot(fs/1e3, M(lam_i, Qfull, fn), '--', 'LineWidth', 1.1, 'Color', cols(i,:));
  end
end
xline(fr/1e3,'k--','fr(计算)≈81k'); xline(75,'k:','fr(实测)≈75k'); yline(1,'k:','M=1'); grid on;
xlabel('fs [kHz]'); ylabel('M'); ylim([0 1.5]);
legend({'k=2.5 (Lm≈20µH)','k=3.0 (Lm≈25µH)','k=3.2 (Lm≈26µH) 选定','k=3.5 (Lm≈29µH)','k=4.0 (Lm≈33µH)','k=5.0 (Lm≈41µH)'},'Location','best');
title('满载 12Ω：不同 k=Lm/Lr 增益曲线（选取 Lm）');

%% 右面板：各带载增益曲线（选定 k=3.2, Lm=26µH）
loads_c = [12 15 20 30 40 46];
lg = {'满载 12Ω','15Ω','20Ω','30Ω','40Ω (验证)','46Ω','实测工作点'};
subplot(1,2,2); hold on;
for i=1:numel(loads_c)
  R = loads_c(i);
  if R==12            % 满载：加粗实线
    plot(fs/1e3, M(lam, R2Q(R), fn), '-', 'LineWidth', 2.4, 'Color', cols(3,:));
  elseif R==40        % 验证负载（60V/80kHz/40Ω）：加粗蓝实线
    plot(fs/1e3, M(lam, R2Q(R), fn), '-', 'LineWidth', 2.0, 'Color', [0.1 0.35 0.7]);
  else
    plot(fs/1e3, M(lam, R2Q(R), fn), '--', 'LineWidth', 1.1, 'Color', cols(6,:));
  end
end
% 实测工作点（副边整流前方波）：fr≈75kHz 增益=1；80kHz 满摆 60V；100kHz ≈50V(M≈0.85)
plot([75 80 100], [1.0 1.0 0.85], 'ko', 'MarkerFaceColor','k', 'MarkerSize', 7);
xline(fr/1e3,'k--','fr(计算)≈81k'); xline(75,'k:','fr(实测)≈75k'); yline(1,'k:','M=1'); grid on;
xlabel('fs [kHz]'); ylabel('M'); ylim([0 1.5]);
legend(lg, 'Location','best');
title('各带载增益曲线 (k=3.2, Lm=26µH)');

saveas(gcf, 'LLC_gain_curves_Lm26uH.png');

%% ============ 满载增益 vs fs（看调压范围） ============
fprintf('\n===== 60V 满载(12Ω) 各频率增益（Vout=60V×M） =====\n');
fprintf('%8s %8s %9s\n','fs[kHz]','M','Vout');
for f=[65 70 75 80 85 90 95 100 110]
  fprintf('%8.0f %8.3f %9.1f\n', f, M(lam, Qfull, f*1e3/fr), Vin*M(lam, Qfull, f*1e3/fr));
end
fprintf('\n实测基准：fr≈75kHz 增益=1（副边满摆 60V）；100kHz ≈50V（M≈0.85）。\n');
fprintf('副边整流前方波增益健康；输出压降根因=整流管 RHRP860 VF，换 MBR10100（见 README §3）。\n');
