% LLC 谐振腔 60V 改腔设计对比
% 现腔: Z0=181.5, Lr=275uH, Cr=8.35nF, Lm=2mH, fr=105k, λ=0.135
% 改腔: Z0=52.2, Lr=79uH,  Cr=29nF,  Lm=2mH, fr=105k, λ=0.040
clear; clc;
fr = 105000; Vin = 60; R2Q = @(Z,R) Z/(0.8106*R);
M = @(lam,Q,fn) 1./sqrt((1+lam-lam./fn.^2).^2 + Q.^2.*(fn-1./fn).^2);

% ---- 现腔 vs 改腔: 46Ω 跨全的增益曲线 ----
Z0a=181.5; lam_a=0.135;         % 现腔
Z0b=52.2;  lam_b=0.040;         % 改腔 (Lr=Z0b/(2pi fr)=79uH, Cr=1/(Z0b 2pi fr)=29nF)
Qa = R2Q(Z0a,46); Qb = R2Q(Z0b,46);
fs=(85:0.5:135)*1e3; fn=fs/fr;
fprintf('46Ω跨全: 现腔Q=%.2f 改腔Q=%.2f\n',Qa,Qb);
figure('Position',[60 60 880 500]);
plot(fs/1e3, M(lam_a,Qa,fn),'LineWidth',1.6); hold on;
plot(fs/1e3, M(lam_b,Qb,fn),'LineWidth',1.6);
xline(105,'--','fr=105k'); yline(1,'k:','M=1');
xlabel('fs [kHz]'); ylabel('M'); grid on; ylim([0 1.15]);
legend({'现腔 Z0=181 (Q=4.87)','改腔 Z0=52  (Q=1.40)'},'Location','best');
title('46Ω 跨全负载: 改腔前后增益曲线（无损理想）');
saveas(gcf,'llc_rescale_46ohm.png');

% ---- 改腔后 (Z0=52.2) 负载功率阶梯 @60V, M=1 ----
fprintf('\n===== 改腔 Z0=52.2Ω, 60V, 谐振点 M=1 的功率阶梯 =====\n');
loads = {46,'跨全 46 (18+18+8+2)',0; 36,'跨全 36 (18+18)',0; ...
         28,'跨全 28 (18+8+2)',0; 26,'跨全 26 (18+8)',0; 18,'跨全 18',0; ...
         28,'每路 28 (18+8+2)',1; 18,'每路 18',1; 8,'每路 8',1};
fprintf('%-22s %6s %6s %9s %9s\n','负载','R','Q','P@M=1','Vrail@M=1');
for i=1:size(loads,1)
  R=loads{i,1}; per=loads{i,3};
  if per
    Rac=0.8106*4*R; P=2*R*(Vin/2*1)^2/R^2; Vrail=Vin/2*1;
  else
    Rac=0.8106*R;    P=R*1^2/R^2; P=Vin^2*1^2/R; Vrail=NaN;
  end
  Q=Z0b/Rac;
  if per, vrs=sprintf('%d',round(Vrail)); else, vrs='-'; end
  fprintf('%-22s %6.0f %6.2f %9.0f %9s\n',loads{i,2},R,Q,P,vrs);
end

% ---- 改腔后增益曲线随负载（看频率可调范围） ----
fprintf('\n===== 改腔 Z0=52.2: 各频率增益（跨全） =====\n');
fprintf('%12s','R(跨全)');
fk=[95 100 105 110 115 120 125 130];
for f=fk
  fprintf('%8.0fk',f);
end
fprintf('\n');
for R=[46 36 28 18]
  Q=R2Q(Z0b,R);
  fprintf('%-12s',sprintf('%dΩ (Q=%.1f)',R,Q));
  for f=fk
    fprintf('%8.3f',M(lam_b,Q,f*1e3/fr));
  end
  fprintf('\n');
end
