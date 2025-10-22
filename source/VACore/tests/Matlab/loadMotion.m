function [ s ] = loadMotion( filename )
    fid = fopen(filename, 'r');
    A = fscanf(fid, '%f');
    fclose(fid);

    n = round(length(A)/17);
    A = reshape(A,17,n);

    s = struct;
    s.t = A(1,:);
    s.x = A(2,:);
    s.y = A(3,:);
    s.z = A(4,:);
    s.xVelo = A(5,:);
    s.yVelo = A(6,:);
    s.zVelo = A(7,:);
    s.yaw   = A(14,:);
    s.pitch = A(15,:);
    s.roll  = A(16,:);
    
    
end

