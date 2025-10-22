close all;

%m1 = loadMotion('motion_dreick.dat');
m1 = loadMotion('test.dat');

n = length(m1.t);
v = zeros(1,n);
for i=1:n-1
    v(i) = sqrt((m1.x(i+1)-m1.x(i))^2 + (m1.y(i+1)-m1.y(i))^2 + (m1.z(i+1)-m1.z(i))^2) / (m1.t(i+1)-m1.t(i));
end
v(n)=0;

%ezplot3(x,y,z)
f=figure
comet(m1.x,m1.y)
% hold on;
% comet(m1.x,m1.y)
figure;
plot(v)