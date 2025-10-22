function [ out ] = va_matlab2openGL( in )
%VA_MATLAB2OPENGL converts either an Nx3 cartesian coordinates matrix or an
%itaCoordinates object (ITA-Toolbox) from the Matlab (mathematic) to the
%right-handed OpenGL world coordinate system.
%
%       MATLAB                  OpenGL
%   
%        (+Z)                    (+Y)
%          |                       |
%          |                       |
%          . - - (+X)              . - - (+X)
%         /                       /
%        /                       /
%     (-Y)                    (+Z)


MATRIX_MATLAB2OPENGL= [1 0 0; 0 0 -1; 0 1 0];

if isa(in,'itaCoordinates')
    in=in.cart;
end
if ~(size(in,2)==3)
    error('Input has to be Nx3 matrix or itaCoordinates object.')
end

out=in*(MATRIX_MATLAB2OPENGL);
