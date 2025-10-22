ccx;
d = importdata('band-20-interpolated_coeffs.dat');
f = d.data(:,1);
m = d.data(:,2);
plot(f,m);
%xlim(f);

h = ita_read('band-20.wav');
h.plot_dat;