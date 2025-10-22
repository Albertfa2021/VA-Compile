function [ out ] = va_openGL2matlab( in )
%VA_OPENGL2MATLAB converts either an Nx3 cartesian coordinates matrix or an
%itaCoordinates object (ITA-Toolbox) from the right-handed OpenGL world to
%the Matlab (mathematic) coordinate system.
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


MATRIX_OPENGL2MATLAB = [1 0 0; 0 0 1; 0 -1 0];

if isa(in,'itaCoordinates')
    in=in.cart;
end
if ~(size(in,2)==3)
    error('Input has to be Nx3 matrix or itaCoordinates object.')
end

out=in*(MATRIX_OPENGL2MATLAB);
