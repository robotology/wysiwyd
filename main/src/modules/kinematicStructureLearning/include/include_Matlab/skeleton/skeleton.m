%SKELETON produces skeletons of binary images
%
% [skg,rad] = skelgrad(img) computes the skeleton for a binary image, and
% also the local radius at each point.

disp('Compiling...  (You may need to execute ''mex -setup'' first.)');
mex '/home/hjchang/Research/code/Matlab/motion segmentation/Skeleton/skeleton.cpp'