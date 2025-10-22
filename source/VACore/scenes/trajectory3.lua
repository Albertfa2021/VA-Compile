--[[

	Szene: 			trajectory1 of whisPER listening experiment
	Beschreibung:	Eine Quelle die sich auf einer Kreisbahn um den Hörer bewegt
	Autor: 			Florian Pausch (fpa@akustik.rwth-aachen.de)

--]]

--= Parameter =--

DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"

r = 2		-- Radius [m]
T = 20		-- Umlaufdauer [s] (einer Runde) 
fm = 300	-- Motion update rate [Hz]


--= Hilfsfunktionen =--

function grad2rad(phi)
	return phi*3.1415927/180
end


--= Skript ==-

-- Zugriff auf Kern-Instanz holen
core = VACore()
core:Reset()
core:SetOutputGain(.4)

core:SetGlobalAuralizationMode("default")

core:LockScene()

-- Schallquelle erzeugen
S = core:CreateSoundSource("Bauer")
print("S = "..S)
--D = core:LoadDirectivity(DataPath.."\\Directivity\\Slenczka_2005_energetic_3d_octave\\".."Saenger.daff", "Saenger")
--core:SetSoundSourceDirectivity(S,D)
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\Bauer.wav")
--X = core:CreateAudiofileSignalSource("D:\\Users\\pausch\\Boost\\ListeningExperiments\\Stimuli\\processed_stimuli\\mono_noisetrain_55624_norm.wav")
--X = core:CreateAudiofileSignalSource("D:\\Users\\pausch\\Boost\\ListeningExperiments\\Stimuli\\processed_stimuli\\mono_speech_55624_norm.wav")
print("S = "..X)
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Hoerer")
H = core:LoadHRIRDataset(HRIRDataset)
core:SetListenerHRIRDataset(L, H)
core:SetActiveListener(L)

core:UnlockScene()

-- Positionen und Orientierungen setzen
core:SetListenerPositionOrientationVU(L, 0,1.7,0, 0,0,-1, 0,1,0)

-- Wiedergabe starten (Looped)
core:SetAudiofileSignalSourcePlayState(X, "play, loop")


omega = 360/T	-- Winkelgeschwindigkeit [°/s]
dt = 1/fm		-- Motion update period [s]
phi_start = 160;  -- start at azi = phi_start
dphi = omega/fm

setTimer(dt)

phi = phi_start
while true do
	x = r*cos( grad2rad(phi) )
	z = r*sin( grad2rad(phi) )
	
	waitForTimer()
    core:SetSoundSourcePositionOrientationVU(S, x,1.15,z, 0,0,-1, 0,1,0)
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then
		break
	end
end

-- Playback beenden
core:SetAudiofileSignalSourcePlayState(X, "stop")
core:Reset()

print("Ende im Gelaende.")
