-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetOutputGain(1)

DataPath = "..\\..\\..\\VAData_";
HRIRDataset = "ITA-Kunstkopf_HRIR_Mess01_D180_1x5_128.daff";

-- Schallquelle erzeugen
S = core:CreateSoundSource("Source 1")
core:SetSoundSourceSignalSource(S,"ss1")

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Listener 1")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)
