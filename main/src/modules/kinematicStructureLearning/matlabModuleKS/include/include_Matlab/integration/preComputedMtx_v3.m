function out = preComputedMtx_v3(KineStruct, method)
nNode = KineStruct.num_seg;
nFrm = KineStruct.num_frames;
jnt = KineStruct.joint_center;

intermediate_result = cell(nNode, nNode, nFrm);
out = cell(nNode, nNode, nNode);

switch(method)
    case 'angle'
        for i=1:nNode
            for j=1:nNode
                if i~=j
                    ang_buf = zeros(nFrm,1);
                    parfor frm_idx = 1:nFrm
                        out_struct = struct('vi_jnt_ij',[],'vj_jnt_ij',[],'ang_ij',[]);
                        % angle between i-j
                        out_struct.vi_jnt_ij = KineStruct.seg_center(:,i,frm_idx) - jnt{i,j}(:,frm_idx);
                        out_struct.vj_jnt_ij = KineStruct.seg_center(:,j,frm_idx) - jnt{i,j}(:,frm_idx);
                        
                        out_struct.ang_ij = mod( atan2( det([out_struct.vi_jnt_ij,out_struct.vj_jnt_ij]) , dot(out_struct.vi_jnt_ij,out_struct.vj_jnt_ij) ) , 2*pi );
                        ang_buf(frm_idx) = out_struct.ang_ij;
                        
                        intermediate_result{i,j,frm_idx} = out_struct;
                    end
                    intermediate_result{i,j,1}.ang_buf = ang_buf;
                end
            end
        end
        
    case 'log'
        %% Relative angle based method
        for i=1:nNode
            for j=1:nNode
                if i~=j
                    for frm_idx = 1:nFrm
                        out_struct = struct('vi_jnt_ij',[],'vj_jnt_ij',[],'ang_ij',[],'rotm_z',[]);
                        % angle between i-j
%                         out_struct.vi_jnt_ij = KineStruct.seg_center(:,i,frm_idx) - jnt{i,j}(:,frm_idx);
%                         out_struct.vj_jnt_ij = KineStruct.seg_center(:,j,frm_idx) - jnt{i,j}(:,frm_idx);
                        out_struct.vi_jnt_ij = KineStruct.seg_center(1:2,i,frm_idx) - jnt{i,j}(1:2,frm_idx);
                        out_struct.vj_jnt_ij = KineStruct.seg_center(1:2,j,frm_idx) - jnt{i,j}(1:2,frm_idx);
                        
                        out_struct.ang_ij = atan2( det([out_struct.vi_jnt_ij,out_struct.vj_jnt_ij]) , dot(out_struct.vi_jnt_ij,out_struct.vj_jnt_ij) );
                        out_struct.rotm_z = rotz(out_struct.ang_ij);
                        intermediate_result{i,j,frm_idx} = out_struct;
                    end
                end
            end
        end
        
        temp_idx = 1;
        idx_set = zeros(nNode^3,3);
        for i=1:nNode
            for j=1:nNode
                for k=1:nNode
                    if (i ~= j) && (j ~= k) && (k ~= i)
                        idx_set(temp_idx,:) = [i,j,k];
                        temp_idx = temp_idx + 1;
                    end
                end
            end
        end
        idx_set(temp_idx:end,:) = [];
        idx_set_len = temp_idx - 1;
        
        cal_m_buf = zeros(nFrm,1);
        
        for idx = 1:idx_set_len
            
            i = idx_set(idx,1);
            j = idx_set(idx,2);
            k = idx_set(idx,3);
            
            for frm_idx = 1:5:nFrm
                cal_m_buf(frm_idx) = cal_logm(intermediate_result{i,j,frm_idx}.rotm_z, intermediate_result{i,k,frm_idx}.rotm_z);
            end
            out{i,j,k}.cal_m_F = cal_m_buf;
            out{i,j,k}.min = min(cal_m_buf);
            out{i,j,k}.max = max(cal_m_buf);            
        end
end
end