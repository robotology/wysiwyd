function SVDDtrain_batch(x,y)

%============== SVDD Training Function ================
% [Input]
%   x: training data
% [Output]
%   alpha: Lagrange multiplier
%   SV: Support Vectors
%   nSV: number of Support Vector
%
% Hyung jin Chang 06/13/2007
% hjchang@neuro.snu.ac.kr
%======================================================

global C;
global kernel_param;
global kernel_type;
% global eps;
global alpha;
global SV
global SV_class_idx
global a
global R2
global learning_type;
global num_class
global SET
global g

[ndata, ndim] = size(x);
alpha0 = zeros(ndata,1);            % setting the starting point of Lagrange multiplier
H = zeros(ndata,ndata);

%-- Coefficient Matrices of Lagrange Multiplier --
% for i=1:ndata
%     for j=1:ndata
%         H(i,j) = Kernel_Function(x(i,:),x(j,:),kernel_type,kernel_param);
%     end
% end

H = genKmtx(x,kernel_type,kernel_param);

for i=1:ndata
    F(i)= H(i,i);
end
H = 2 * (H+1e-10*eye(size(H)));         % to avoid singularity
F = -F';                                % minus sign(-) is to get maximum of L using Lagrange multiplier

Aeq = (ones(1, ndata));                 % equality constraint
beq = 1;                                % equality constraint            
Lower_Bound = zeros(ndata, 1);          % lower bound
Upper_Bound = C * ones(ndata, 1);       % upper bound
%-------------------------------------------------

%------------- Lagrange Multiplier ---------------
warning off;
option = optimset('LargeScale','on','MaxIter',100000);
alpha = quadprog(H,F,...
                 [],[],...
                 Aeq,beq,...
                 Lower_Bound,Upper_Bound,...
                 alpha0,option); %quadratic programming
%-------------------------------------------------

%------- condition check of alpha value ----------
idx_SV = find(alpha > eps & alpha < C); % Support Vector
idx_In = find(alpha <= eps);             % Inlier
idx_Out = find(alpha >= C);              % Outlier
alpha(idx_In) = 0;                      % setting Inlier's alpha as 0
alpha(idx_Out) = C;                     % setting Outlier's alpha as C

SV = x(idx_SV,:);                        % Support Vector
nSV = size(SV,1);                        % number of Support Vector

SET.S.x = x(idx_SV,:);
SET.S.y = y(idx_SV,:);
SET.S.alpha = alpha(idx_SV,:);
SET.S.ndata = size(idx_SV,1);

SET.O.x = x(idx_In,:);
SET.O.y = y(idx_In,:);
SET.O.alpha = alpha(idx_In,:);
SET.O.ndata = size(idx_In,1);

SET.E.x = x(idx_Out,:);
SET.E.y = y(idx_Out,:);
SET.E.alpha = alpha(idx_Out,:);
SET.E.ndata = size(idx_Out,1);