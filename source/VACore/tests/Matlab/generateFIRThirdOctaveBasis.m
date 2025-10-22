%
%  Das meiste kommt von hier:
%  http://www.mathworks.de/products/dsp-system/demos.html?file=/products/demos/shipping/dsp/octavedemo.html
%

ccx;

BandsPerOctave = 1;
N = 10;           % Filter Order
F0 = 1000;       % Center Frequency (Hz)
Fs = 44100;      % Sampling Frequency (Hz)

f = fdesign.octave(BandsPerOctave,'Class 1','N,F0',N,F0,Fs)

F0 = validfrequencies(f);
Nfc = length(F0);
for i=1:Nfc,
    f.F0 = F0(i);
    Hd(i) = design(f,'butter');
end

f.BandsPerOctave = 3;
f.FilterOrder = 4;
F0 = validfrequencies(f);
Nfc = length(F0);
for i=1:Nfc,
    f.F0 = F0(i);
    Hd3(i) = design(f,'butter');
end

hfvt = fvtool(Hd,'FrequencyScale','log','color','white');
axis([0.01 24 -90 5])
title('Octave-Band Filter Bank')
hfvt = fvtool(Hd3,'FrequencyScale','log','color','white');
axis([0.01 24 -90 5])
title('1/3-Octave-Band Filter Bank')

%%
numSamples = 16384;
dirac = zeros(1, numSamples);
dirac(1) = 1;
h = zeros(Nfc, numSamples);
hx = zeros(numSamples,1);
for i=1:Nfc,
    h(i,:) = filter(Hd3(i),dirac);
    %Pyyw(i) = avgpower(psd(hp,yw(:,i),'Fs',Fs));
    hx = hx +h(i,:)';
end


%% Biquad-Coeffizienten als C code ausgeben
% fprintf('\t// Parameters (g, b0, b1, b2, a0, a1) for biquad bandpasses\n');
% fprintf('\tfloat BIQUAD_PARAMS[%d][6] = {\n', Nfc);
% for i=1:Nfc
%     fprintf('\t\t// Band %d, center frequency %0.1f Hz\n', i, F0(i));
%     fprintf('\t\t{ %0.12fF, %0.12fF, %0.12fF, %0.12fF, %0.12fF, %0.12fF }',...
%            Hd3(i).ScaleValues(1), Hd3(i).SOSMatrix(1,1), Hd3(i).SOSMatrix(1,2), Hd3(i).SOSMatrix(1,3), ...
%             Hd3(i).SOSMatrix(1,5), Hd3(i).SOSMatrix(1,6));
%     if (i<Nfc)
%         fprintf(',\n');
%     else
%         fprintf('\n');
%     end
% end
% fprintf('};\n\n');

X = size( Hd3(i).SOSMatrix );
numbiquads = X(1);
fprintf('\tconst int  NUM_BANDS = %d;\n', Nfc);
fprintf('\tconst int  NUM_BIQUADS_PER_BAND = %d;\n', numbiquads);
fprintf('\t// Parameters (g, b0, b1, b2, a0, a1) for biquad bandpasses\n');
fprintf('\tconst float BIQUAD_PARAMS[%d][%d][6] = {\n', Nfc, numbiquads);
for i=1:Nfc
    fprintf('\t\t{ // Band %d, center frequency %0.1f Hz\n', i, F0(i));
    
    for k=1:numbiquads
        fprintf('\t\t\t{ %0.12fF, %0.12fF, %0.12fF, %0.12fF, %0.12fF, %0.12fF }',...
               Hd3(i).ScaleValues(k), Hd3(i).SOSMatrix(k,1), Hd3(i).SOSMatrix(k,2), Hd3(i).SOSMatrix(k,3), ...
                Hd3(i).SOSMatrix(k,5), Hd3(i).SOSMatrix(k,6));
        if (k<numbiquads)
            fprintf(',\n');
        else
            fprintf('\n');
        end
    end
    fprintf('\t\t}');
    
    if (i<Nfc)
        fprintf(',\n');
    else
        fprintf('\n');
    end    
end
fprintf('\t};\n\n');
    
%     fprintf('float BAND%d_BIQUAD_PARAMS[6] = { %0.12fF, %0.12fF, %0.12fF, %0.12fF, %0.12fF, %0.12fF };\n',...
%             i, Hd3(i).ScaleValues(1), Hd3(i).SOSMatrix(1,1), Hd3(i).SOSMatrix(1,2), Hd3(i).SOSMatrix(1,3), ...
%             Hd3(i).SOSMatrix(1,5), Hd3(i).SOSMatrix(1,6));
