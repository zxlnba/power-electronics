% LLC 全桥时域仿真 + FHA 增益曲线
% 复现实测: 60V, 46欧跨全, 100k->43.49V, 105k->47.8V, 115k->36.92V
% Q 口径：本脚本针对 60V 现腔分析，用功率守恒物理口径 Q=Z0/(0.8106·R)（跨全 n=1）。
%   额定 160Ω → Q≈1.4；设计书 §3.2 每路口径（n=2）额定 Q=0.35，差 n²=4 倍，只影响
%   数值标签；本脚本复现的 60V 实测塌陷与 Q 口径无关。
clear; clc;

% ===== 元件参数（从报告/代码反推, fr≈105k, Z0=181.5, Lm=2mH） =====
Vin     = 60.0;        % 母线
Z0      = 181.5;       % 特性阻抗
fr      = 105000;      % 实测谐振点
Lr      = Z0/(2*pi*fr);            % 275uH
Cr      = 1/(Z0*2*pi*fr);          % 8.35nF
Lm      = Lr/0.135;                % ~2.04mH (λ=0.135)
N       = 28/28;        % 原边28T:副边全绕组28T = 1:1 (跨全输出)

% 损耗模型参数（先给一组合理值, 后面灵敏度分析）
Rs_pri  = 0.5;    % 原边串联电阻(绕组+ESR) [ohm]
Vf      = 0.8;    % 二极管压降 [V], 跨全路径上两只 => 2*Vf
Td      = 400e-9; % 死区时间 [s]   <-- 关键嫌疑

% ===== 实测数据 =====
meas_fs  = [100000, 105000, 115000];
meas_Vo  = [43.49,  47.8,   36.92];

% ===== 1) FHA 理想增益曲线 =====
fs_axis  = (90:0.5:130)*1e3;
fn       = fs_axis/fr;
lam      = 0.135;
M_fha    = @(Q,fn) 1./sqrt((1+lam-lam./fn.^2).^2 + Q.^2.*(fn-1./fn).^2);
% Q for various loads (跨全: R_ac=0.81*R_load)
Rloads   = [1000, 160, 46, 26];  % 轻/额定/你的/更重
Qv       = Z0./(0.8106*Rloads);
leg1 = {};
for i=1:numel(Rloads)
    plot(fs_axis/1e3, M_fha(Qv(i),fn),'LineWidth',1.2); hold on;
    leg1{i} = sprintf('FHA Q=%.2f (R=%d\\Omega)',Qv(i),Rloads(i));
end
plot(meas_fs/1e3, meas_Vo/60, 'ko','MarkerFaceColor','k','MarkerSize',8);
leg1{end+1} = '实测 M (60V/46\Omega)';
xlabel('fs [kHz]'); ylabel('M = Vout/Vin');
title('FHA 理想增益曲线 vs 实测');
grid on; legend(leg1,'Location','best'); ylim([0 1.2]);
saveas(gcf, 'llc_fha.png');
disp('=== FHA ideal M at fr=105k ===');
fprintf('Q=%.2f (1k欧):   M(105k)=%.3f\n', Qv(1), M_fha(Qv(1),1));
fprintf('Q=%.2f (160欧额):M(105k)=%.3f\n', Qv(2), M_fha(Qv(2),1));
fprintf('Q=%.2f (46欧):   M(105k)=%.3f\n', Qv(3), M_fha(Qv(3),1));
fprintf('Q=%.2f (26欧):   M(105k)=%.3f\n', Qv(4), M_fha(Qv(4),1));

% ===== 2) 时域仿真: 功率平衡固定点法 =====
% 给定 v_out, 跑谐振腔N周期, 返回平均副边输出电流
function Iavg = tankAvg(vout, fs, Vf, Td, Rs, Lr,Cr,Lm,N,Vin)
  T  = 1/fs;
  dt = T/400;              % 每周期400步
  ncyc = 50;               % 周期数
  Nst  = round(T*ncyc/dt);
  t    = 0;
  iLr  = 0; vCr = 0; iLm = 0;   % 状态: 谐振电流, 谐振电容电压, 励磁电流
  Iav  = 0; Iavn = 0;
  % 半周期时长 (全桥50%占空 + 死区)
  Th = T/2;
  for s=1:Nst
    tnow = t;
    % 输入方波: ±Vin, 死区内为0
    ph = mod(tnow, T);
    if ph < Th
       if ph > Th - Td, Vsq = 0; else Vsq = +Vin; end   % 前半: 关断前死区
    else
       if ph > T - Td,  Vsq = 0; else Vsq = -Vin; end   % 后半
    end
    % v_pri 由整流钳位决定
    % 检测整流导通: |v_pri|*N > vout + 2*Vf
    % 先试算不导通情况下的 v_pri (i_p=0, i_Lr=i_Lm)
    diLr_no = (Vsq - vCr - Rs*iLr)/(Lr+Lm);
    vpri_no = Lm*diLr_no;
    vsec_no = vpri_no*N;
    if abs(vsec_no) > vout + 2*Vf
        % 导通: v_pri 被钳位到 ±(vout+2Vf)/N
        vpri = sign(vsec_no)*(vout+2*Vf)/N;
        ip   = iLr - iLm;              % 原边变压器电流
        isec = ip*N;                   % 副边电流
        if isec < 0, isec = 0; end
        diLr = (Vsq - vCr - vpri - Rs*iLr)/Lr;
        diLm = vpri/Lm;
        iout = abs(isec);
    else
        diLr = diLr_no; diLm = diLr_no;
        iout = 0;
    end
    diCr = iLr/Cr;
    % RK4 简化为显式欧拉(小步长够用)
    iLr  = iLr + dt*diLr;
    iLm  = iLm + dt*diLm;
    vCr  = vCr + dt*diCr;
    % 平均输出电流 (最后10个周期)
    if tnow > (ncyc-10)*T
        Iav  = Iav + iout*dt;
        Iavn = Iavn + dt;
    end
    t = t + dt;
  end
  Iavg = Iav/Iavn;
end

% 固定点: vout = Iavg(vout)*R
Rload = 46;
for k=1:length(meas_fs)
    fs = meas_fs(k);
    % 迭代求 vout
    v = 30;   % 初值
    for it=1:30
        ia = tankAvg(v, fs, Vf, Td, Rs_pri, Lr,Cr,Lm,N,Vin);
        vnew = ia*Rload;
        if abs(vnew-v) < 0.01, v=vnew; break; end
        v = 0.6*v + 0.4*vnew;   % 阻尼
    end
    simVo(k) = v;
    fprintf('[%5.0fk] 实测Vout=%.2fV  M=%.3f | 仿真Vout=%.2fV  M=%.3f\n', ...
        fs/1e3, meas_Vo(k), meas_Vo(k)/Vin, v, v/Vin);
end

% ===== 3) 灵敏度: 死区/铜阻/二极管哪个是主因 =====
fprintf('\n=== 灵敏度 @105k, 46欧 ===\n');
fs = 105000;
cfgs = { '理想(无损耗)'         , 0.0, 0.0, 0.0;
         '+死区400ns'            , 0.0, 0.0, 400e-9;
         '+二极管1.6V'           , 0.0, 0.8, 400e-9;
         '+铜阻0.5Ω(全损耗)'     , 0.5, 0.8, 400e-9};
for c=1:size(cfgs,1)
    Rs0 = cfgs{c,2}; Vf0 = cfgs{c,3}; Td0 = cfgs{c,4};
    v = 30;
    for it=1:30
        ia = tankAvg(v, fs, Vf0, Td0, Rs0, Lr,Cr,Lm,N,Vin);
        vnew = ia*Rload;
        if abs(vnew-v)<0.01, v=vnew; break; end
        v = 0.6*v+0.4*vnew;
    end
    fprintf('%-22s M=%.3f (Vout=%.2f)\n', cfgs{c,1}, v/Vin, v);
end
fprintf('\n实测 105k: M=%.3f (Vout=47.80)\n', 47.8/Vin);
