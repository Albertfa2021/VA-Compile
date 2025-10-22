close all;

T = 0.010;
t = linspace(-T, +T, 100);

z = 0.010;
w = cos( t.*pi/(2*z) ).^2;
plot(t,w)
