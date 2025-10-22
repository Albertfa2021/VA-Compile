-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()

core:SetOutputGain(0.3)

DataPath = "..\\..\\..\\VAData";
HRIRDataset = "ITA-Kunstkopf_HRIR_Mess04_D200_1x5_256.daff";

-- Schallquelle erzeugen
S = core:CreateSoundSource("Source")
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\Bauer.wav")
--X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\sine440_60s.wav")
core:SetSoundSourceSignalSource(S,X)
core:SetSoundSourcePosition(S, 0,0,0);

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Listener")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)
core:SetListenerPosition(L, 0.01,0,0);

-- Play
core:SetAudiofileSignalSourcePlayState(X, 1)
sleep(2000)

-- Pause
core:SetAudiofileSignalSourcePlayState(X, 0)
sleep(2000)

-- Play
core:SetAudiofileSignalSourcePlayState(X, 1)
sleep(2000)

-- Stop = Pause + Rewind
core:SetAudiofileSignalSourcePlayState(X, 2)
sleep(2000)

-- Play
core:SetAudiofileSignalSourcePlayState(X, 1)
sleep(2000)