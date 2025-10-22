--[[

	Szene: 			Simple
	Beschreibung:	Einfachste Auralisierung der Welt
	Autor: 			Jonas

--]]

--= Parameter =--

DataPath = "..\\..\\..\\VAData"

-- Zugriff auf Kern-Instanz holen --
core = VACore()

core:LockScene()
L = core:CreateListener("Listener")
core:SetListenerPositionOrientationYPR(L, 0,1.7,-1, 180,0,0)
S = core:CreateSoundSource("Trumpet")
core:SetSoundSourcePositionOrientationYPR(S, 0,1,1, 0,0,0)
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\Trompete.wav")
core:SetAudiofileSignalSourcePlayState(X, "play, loop");
core:SetSoundSourceSignalSource(S, X)
core:UnlockScene()

core:SetActiveListener(L)