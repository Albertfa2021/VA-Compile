--[[

	Szene: 			Eurogress
	Beschreibung:	Erste Szene mit Raumakustik
	Autor: 			Jonas

--]]

DataPath = "..\\..\\..\\VAData"

core = VACore()
core:Reset()
core:SetGlobalAuralizationMode("default")

core:LoadScene(DataPath.."\\Raven\\Eurogress.rpf")

H = core:LoadHRIRDataset(DataPath.."\\HRIR\\hrir_omni_2ch.daff")
L = core:CreateListener("Listener")
core:SetListenerHRIRDataset(L, H)
core:SetListenerPositionOrientationYPR(L, 0,1.7,-1, 180,0,0)
S = core:CreateSoundSource("Trumpet")
core:SetSoundSourcePositionOrientationYPR(S, 0,1,1, 0,0,0)
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\bass.wav")
core:SetSoundSourceSignalSource(S, X)
core:SetAudiofileSignalSourcePlayState(X, "play, loop")

core:SetActiveListener(L)