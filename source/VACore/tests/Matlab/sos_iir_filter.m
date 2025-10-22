%
%  SOS-IIR filter
%

n = 2;
b = [ 1, 0, -1 ];
a = [  1.0000   -1.9927    0.9935 ];

s=0.0032693793622147;
b=b*s;
a=a*s;
v = zeros(1,n+1);

% Dirac
m = 2048;
x = zeros(1,m);
x(1) = 1;

% Filtering
y = zeros(1,m);

for i=1:m
    % Shift
    v(3) = v(2);
    v(2) = v(1);
    
    v(1) = x(i) - a(2)*v(2) - a(3)*v(3);
    y(i) = b(1)*v(1) + b(2)*v(2) + b(3)*v(3);
end

figure
hold on
%plot(ref, 'r');
plot(y);

z = filter(b,a,x);
%plot(z, 'r');
