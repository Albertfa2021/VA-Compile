-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetOutputGain(0.3)

DataPath = "..\\..\\..\\VAData";
HRIRDataset = "ITA-Kunstkopf_HRIR_Mess01_D180_1x5_128.daff";

-- Schallquelle erzeugen
S = core:CreateSoundSource("Source 1")
X = core:CreateSamplerSignalSource("Sampler")
print("Created sampler signal source: ID = "..X)
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Listener 1")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)

-- Samples laden
ufftata = core:LoadSample(DataPath.."\\Audiofiles\\Samples\\Ufftata.wav");
feddich = core:LoadSample(DataPath.."\\Audiofiles\\Samples\\Feddich.wav");
gehtab = core:LoadSample(DataPath.."\\Audiofiles\\Samples\\Gehtab.wav");

-- Tests
pb = core:AddSoundPlayback(X, ufftata, 0, 0)
print("Got playback ID = "..pb)
sleep(2000);

pb = core:AddSoundPlayback(X, feddich, 0, 0)
print("Got playback ID = "..pb)

sleep(2000);
pb = core:AddSoundPlayback(X, gehtab, 0, 0)
print("Got playback ID = "..pb)

