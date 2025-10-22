-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetGlobalAuralizationMode("default")

DataPath = "$(VADataDir)";
HRIRDataset = "$(VADefaultHRIRDataset)"

-- Schallquelle erzeugen
S = core:CreateSoundSource("Bauer")
--D = core:LoadDirectivity("monopole.daff", "Unity sphere")
D = core:LoadDirectivity(DataPath.."\\Directivity\\Slenczka_2005_energetic_3d_octave\\".."Saenger.daff", "Saenger")
core:SetSoundSourceDirectivity(S,D)
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\sine800Hz_long.wav")
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Listener")
H = core:LoadHRIRDataset(HRIRDataset)
core:SetListenerHRIRDataset(L, H)
core:SetActiveListener(L)

-- Positionen und Orientierungen setzen
core:LockScene()
core:SetListenerPositionOrientationVU(L, 0,1.7,0, 0,0,-1, 0,1,0)
core:SetAudiofileSignalSourcePlayState(X, "play, loop")
core:UnlockScene()


v0 = 5 -- in meter pro sekunde
fs = 10 -- Abtastrate in Hertz

-- Startposition
x = -3.1415 / 2
y = 2
z = -2

-- Schleifenverzögerung vorbereiten
dt = 1/f
setTimer(dt)

-- Losfliegen!
f = 0
while (f < 100) do

	v = v0 * sin( 2 * 3.1415 * f / (fs*2) )
	x = x + v/fs
	waitForTimer()
	core:SetSoundSourcePositionOrientationVelocityVU(S, x,y,z, 0,0,1, 0,1,0, v,0,0)
	
	f = f + 1
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then
		break
	end	
end

core:SetAudiofileSignalSourcePlayState(X, "stop")