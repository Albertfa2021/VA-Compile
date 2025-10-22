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


v0 = 10 -- in meter pro sekunde
fs = 10 -- Abtastrate in Hertz

-- Startposition
x = -25
y = 2
z = -2

-- Losfliegen!
core:SetSoundSourcePositionOrientationVelocityVU(S, x,y,z, 0,0,1, 0,1,0, v0,0,0)
core:UnlockScene()

sleep(5*1000)

core:SetAudiofileSignalSourcePlayState(X, "stop")