-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetOutputGain(0.3)

DataPath = "..\\..\\..\\VAData_";
HRIRDataset = "ITA-Kunstkopf_HRIR_Mess01_D180_1x5_128.daff";

-- Schallquelle erzeugen
S = core:CreateSoundSource("Source 1")
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\lang_short.wav")
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Listener 1")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)

-- Tests
print("Stop");
sleep(2000);

print("Play");
core:SetAudiofileSignalSourcePlayState(X, 2);
sleep(2000);

print("Pause");
core:SetAudiofileSignalSourcePlayState(X, 1);
sleep(3000);

print("Continue");
core:SetAudiofileSignalSourcePlayState(X, 2);
sleep(3000);

print("Stop + Rewind");
core:SetAudiofileSignalSourcePlayState(X, 5);
sleep(1000);

print("Play again");
core:SetAudiofileSignalSourcePlayState(X, 2);
sleep(2000);

print("Turn on loop");
core:SetAudiofileSignalSourcePlayState(X, 8);
sleep(12000);

print("Turning off loop");
core:SetAudiofileSignalSourcePlayState(X, 16);
sleep(12000);
