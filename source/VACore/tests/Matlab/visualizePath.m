close all;

%fid = fopen('..\Release_x64\path.dat', 'r');
fid = fopen('test.dat', 'r');
A = fscanf(fid, '%f');
fclose(fid);

n = length(A)/17;
A = reshape(A,17,n);
% Ersten Werte wegwerfen
%A = A(:,30:end)
n = size(A,2)

k = A(1,:);
t = A(2,:);
t = t - t(1)
d = A(3,:);
od = A(4,:);



% Ableitung
delta = zeros(1,n-1);
for i=1:n-1
    delta(i) = d(i+1)-d(i);
end

if (0)
    %plot(1:n, t);
    plot(1:n, t - (1:n)*0.0029);
    %ylim([2.5 3.1]*0.001)
end

if (0)
    % Deltas der Zeiten
    tdelta = zeros(1,n-1);
    for i=1:n-1
       tdelta(i) = t(i+1)-t(i);
    end

    plot(tdelta);
end

%plot(t,d)

if (0)
    plot(k(1:end-1), delta)
    ylim([-1 1])
end

% d(79:81)
% delta(79:80)

plot(d-od)
ylim([-2 2])
