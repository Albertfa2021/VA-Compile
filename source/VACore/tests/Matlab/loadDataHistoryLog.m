function [ out ] = loadDataHistoryLog( filename )
    RawData = readmatrix(filename);
    
    nDataSets = size(RawData, 2);
    assert(nDataSets > 1, 'Unexpected file format: Expecting at least 2 data columns.');

    out = struct;
    out.time = RawData(:,1);
    out.data = RawData(:,2:end);
end

