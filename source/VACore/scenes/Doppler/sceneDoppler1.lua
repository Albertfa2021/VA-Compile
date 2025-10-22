-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetGlobalAuralizationMode("default")

DataPath = "$(VADataDir)";
HRIRDataset = "omni_ir_2ch.daff"
AudioFile = "sine800Hz_long.wav"

-- Schallquelle erzeugen
S = core:CreateSoundSource("Sine Signal 800 Hz")
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\"..AudioFile)
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Doppler Effect Receiver")
H = core:LoadHRIRDataset(DataPath.."\\HRIR\\"..HRIRDataset)
core:SetListenerHRIRDataset(L, H)
core:SetActiveListener(L)

-- Positionen und Orientierungen setzen
core:LockScene()
core:SetListenerPositionOrientationVU(L, 0,0,0, 0,0,-1, 0,1,0)
core:SetAudiofileSignalSourcePlayState(X, "play, loop")
core:UnlockScene()


v = 4 -- in meter pro sekunde
f = 10 -- Abtastrate in Hertz

-- Startposition
x = -20
y = 2
z = -2

-- Schleifenverzögerung vorbereiten
dt = 1/f
setTimer(dt)

-- Losfliegen!
while x<20 do
	x = x + v/f
	
	waitForTimer()
	core:SetSoundSourcePositionOrientationVelocityVU(S, x,y,z, 0,0,1, 0,1,0, v,0,0)
		
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then
		break
	end	
end

core:SetAudiofileSignalSourcePlayState(X, "stop")