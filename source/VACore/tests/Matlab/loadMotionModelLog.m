function [ out ] = loadMotionModelLog( filename )
    RawData = readmatrix(filename);
    
    nDataSets = size(RawData, 2);
    assert(nDataSets >= 10, 'Unexpected file format: Expecting at least 10 data columns.');
    
    %Ignoring additional entries for now
    %For Input Log-File: Typically 18 collumns;
    %For Output Log-File: Typically 15 collumns;
    
    out = struct;
    out.time = RawData(:,1);
    out.pos = RawData(:, 2:4);
    out.view = RawData(:, 5:7);
    out.up = RawData(:, 8:10);
end

