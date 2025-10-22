-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()

core:SetOutputGain(0.04)

DataPath = "..\\..\\..\\VAData";
HRIRDataset = "ITA-KK_Mess01_1x5.daff";

-- Schallquelle erzeugen
S = core:CreateSoundSource("Pendelnder Lautsprecher")
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\sine440_60s.wav")
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Hoerer")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)

-- Startposition
z1 = -10;
z2 = -8;
v = 3;
T = 0.001;
N = 10/T/v;

core:SetListenerPositionOrientationVU(L, 0,0,0, 0,0,-1, 0,1,0);

for i=0,N do
    t = i*T;
	z = z2 + (z1-z2)*sin(t*v*2*3.1415);
	core:SetSoundSourcePositionOrientationVU(S, 0,0,z, 1,0,0, 0,1,0)
	sleep(T*1000)
end
