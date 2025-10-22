-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()

core:SetOutputGain(0.3)

DataPath = "..\\..\\..\\VAData";
HRIRDataset = "ITA-Kunstkopf_HRIR_Mess01_D180_1x5_128.daff";

-- Schallquelle erzeugen
S = core:CreateSoundSource("Vorbeifahrendes Auto")
--X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\sine440_60s.wav")
--X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\lang_short.wav")
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\Krankenwagen.wav")
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Hoerer")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)

-- Startposition
x1 = -50;
v = 20;
T = 0.001;
N = 80/T/v;

core:SetListenerPositionOrientationVU(L, 0,0,2, 0,0,-1, 0,1,0);
--core:SetSourcePositionOrientationVU(S, x1,0,-2, 0,0,-1, 0,1,0);
core:SetAudiofileSignalSourcePlayState(X, 2);

for i=0,N do
    t = i*T;
	x = x1 + v*t;
	core:SetSoundSourcePositionOrientationVU(S, x,0,0, 1,0,0, 0,1,0)
	sleep(T*1000)
end

core:SetAudiofileSignalSourcePlayState(X, 1);
core:SetOutputGain(0);