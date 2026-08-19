% LLC 全桥 —— 真实理论增益曲线（无损理想）
% FHA 解析 + 无损时域精确解（含谐波）。回答：低于谐振点是否升压？理论增益应为多少？
% Q 口径：本脚本针对 60V 现腔分析，用功率守恒物理口径 R2Q=Z0/(0.8106·R)（跨全 n=1）。
%   额定 160Ω → Q≈1.4；设计书 §3.2 每路口径（n=2）额定 Q=0.35，差 n²=4 倍，只影响
%   数值标签，增益曲线行为与 Q 口径无关。60V 高 Q 塌陷是硬件事实（见工程开发文档）。
% ⚠️ 分析对象：1kW 现腔（Z0=181.5Ω）接 60V 的表现（历史）。60V 缩尺腔（新腔
%   Lr=8.2µH/Cr=470nF/Lm=26µH）副边增益实测健康（fr≈75kHz 增益=1），其输出压降
%   根因=整流管 RHRP860 VF（换 MBR10100），见 README §5。
clear; clc;

% ===== 元件参数（报告：fr=105k, Z0=181.5, λ=0.135, Lm=2mH） =====
Z0    = 181.5;
fr    = 105000;
Lr    = Z0/(2*pi*fr);          % 275uH
Cr    = 1/(Z0*2*pi*fr);        % 8.35nF
Lm    = Lr/0.135;              % 2.04mH
lam   = Lr/Lm;
N     = 28/28;                 % 原边:副边全绕组=1:1（跨全输出）
Vin   = 60.0;
R2Q   = @(R) Z0/(0.8106*R);    % 跨全负载 -> Q

name = {'轻载 1kΩ','额定 160Ω','测试 46Ω','重载 26Ω','重载 15Ω'};
Rc   = [1000, 160, 46, 26, 15];
Qc   = arrayfun(R2Q, Rc);

% ====================================================================
% 1) FHA 单谐波解析理论
% ====================================================================
fs_axis = (85:0.5:135)*1e3;
fn      = fs_axis/fr;
M_fha   = @(Q,fn) 1./sqrt((1+lam-lam./fn.^2).^2 + Q.^2.*(fn-1./fn).^2);

fprintf('================ FHA 单谐波理论（无损） ================\n');
fprintf('%-16s %7s %9s %9s %9s\n','负载','Q','fr点M','升压起点kHz','升压峰M');
for i=1:numel(Rc)
  fnd = linspace(0.15,1,800);
  Mv  = M_fha(Qc(i),fnd);
  k1  = find(Mv>=1,1);
  [Mpk,~] = max(Mv);
  fprintf('%-16s %7.2f %9.3f %9.1f %9.3f\n', name{i}, Qc(i), M_fha(Qc(i),1), fnd(k1)*fr/1e3, Mpk);
end

% ====================================================================
% 2) 无损时域精确解（含谐波；强制 vCr 直流为0 = 稳态物理约束）
% ====================================================================
function [Iavg, resid] = tank(vout, fs, Lr,Cr,Lm,N,Vin, xin)
  T=1/fs; dt=T/600; ncyc=80; Nst=round(T/dt);
  iLr=xin(1); vCr=xin(2); iLm=xin(3);
  % 前20周期丢弃(瞬态), 后12周期平均
  Iav=0; Iavn=0;
  for cyc=1:ncyc
    vCrDC=0; vCrSum=0;
    for s=1:Nst
      t=(cyc-1)*T + (s-1)*dt;
      Vsq = +Vin; if mod(t,T)>=T/2, Vsq=-Vin; end
      % --- RK4 (四阶) ---
      diLr_no = (Vsq - vCr)/(Lr+Lm);  vp = Lm*diLr_no;
      if abs(vp)<vout
        k1=[diLr_no; iLr/Cr; diLr_no]; iout=0;
      else
        vp = sign(vp)*vout;
        k1=[(Vsq-vCr-vp)/Lr; iLr/Cr; vp/Lm]; iout=abs((iLr-iLm)*N);
      end
      tt=t+dt/2; V2=+Vin; if mod(tt,T)>=T/2, V2=-Vin; end
      x=[iLr,vCr,iLm]+0.5*dt*k1';
      d2=(V2-x(2))/(Lr+Lm); vp2=Lm*d2;
      if abs(vp2)<vout, k2=[d2;x(1)/Cr;d2]; else vp=sign(vp2)*vout; k2=[(V2-x(2)-vp)/Lr;x(1)/Cr;vp/Lm]; end
      x=[iLr,vCr,iLm]+0.5*dt*k2';
      d3=(V2-x(2))/(Lr+Lm); vp3=Lm*d3;
      if abs(vp3)<vout, k3=[d3;x(1)/Cr;d3]; else vp=sign(vp3)*vout; k3=[(V2-x(2)-vp)/Lr;x(1)/Cr;vp/Lm]; end
      x=[iLr,vCr,iLm]+dt*k3';
      V4=+Vin; if mod(t+dt,T)>=T/2, V4=-Vin; end
      d4=(V4-x(2))/(Lr+Lm); vp4=Lm*d4;
      if abs(vp4)<vout, k4=[d4;x(1)/Cr;d4]; else vp=sign(vp4)*vout; k4=[(V4-x(2)-vp)/Lr;x(1)/Cr;vp/Lm]; end
      x=[iLr,vCr,iLm]+dt/6*(k1+2*k2+2*k3+k4)';
      iLr=x(1); vCr=x(2); iLm=x(3);
      vCrSum=vCrSum+vCr;
      if cyc>ncyc-12, Iav=Iav+iout*dt; Iavn=Iavn+dt; end
    end
    vCr = vCr - vCrSum/Nst;   % 稳态物理约束: 每周期平均 vCr=0
  end
  Iavg = Iav/max(Iavn,1e-12);
  resid = abs(Iavg*0 - 0);     % 占位
end

function Vo = solve_vout(fs, R, Lr,Cr,Lm,N,Vin)
  % 定点迭代: vout = R * Iavg(vout), 热启动, 阻尼
  v = 0.85*Vin;  xin=[0,0,0];
  for it=1:25
    [ia,~] = tank(v, fs, Lr,Cr,Lm,N,Vin, xin);
    vn = R*ia;
    if abs(vn-v) < 0.02, v=vn; break; end
    v = 0.5*v + 0.5*vn;
  end
  [ia,~] = tank(v, fs, Lr,Cr,Lm,N,Vin, xin);   % 最终校核
  Vo = v;
  if abs(R*ia-v)/v > 0.05, Vo = NaN; end        % 不收敛 -> 标 NaN
end

freqs = [95,100,105,110,115,120]*1e3;
Rs2   = [46, 160];
for qi=1:numel(Rs2)
  R = Rs2(qi); Q = R2Q(R);
  fprintf('\n===== 无损时域精确解 (Q=%.2f, R=%dΩ) =====\n', Q, R);
  fprintf('%9s %9s %9s\n','fs[kHz]','M_exact','M_fha');
  for j=1:numel(freqs)
    Vo = solve_vout(freqs(j), R, Lr,Cr,Lm,N,Vin);
    fprintf('%9.0f %9.3f %9.3f\n', freqs(j)/1e3, Vo/Vin, M_fha(Q, freqs(j)/fr));
  end
end
fprintf('\n（全零损耗理想模型：无铜阻/无二极管压降/无死区；M_exact=NaN 表示定点未收敛）\n');
