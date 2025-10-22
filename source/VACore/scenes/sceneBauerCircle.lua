--[[

	Szene: 			BauerCircle
	Beschreibung:	Eine Quelle die sich auf einer Kreisbahn um den Hörer bewegt
	Autor: 			Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)

--]]

--= Parameter =--

DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"

r = 10		-- Radius [m]
T = 10		-- Umlaufdauer [s] (einer Runde) 
fm = 300	-- Motion update rate [Hz]


--= Hilfsfunktionen =--

function grad2rad(phi)
	return phi*3.1415927/180
end


--= Skript ==-

-- Zugriff auf Kern-Instanz holen
core = VACore()
core:Reset()
core:SetOutputGain(1)

core:SetGlobalAuralizationMode("default")


-- Schallquelle erzeugen
S = core:CreateSoundSource("Bauer")
print("S = "..S)
D = core:LoadDirectivity(DataPath.."\\Directivity\\Slenczka_2005_energetic_3d_octave\\".."Saenger.daff", "Saenger")
core:SetSoundSourceDirectivity(S,D)
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\Bauer.wav")
print("S = "..X)
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Hoerer")
H = core:LoadHRIRDataset(HRIRDataset)
core:SetListenerHRIRDataset(L, H)
core:SetActiveListener(L)

-- Positionen und Orientierungen setzen
core:SetListenerPositionOrientationVU(L, 0,1.7,0, 0,0,-1, 0,1,0)

-- Wiedergabe starten (Looped)
core:SetAudiofileSignalSourcePlaybackAction( X, "play" )
core:SetAudiofileSignalSourceIsLooping( X, 1 )


omega = 360/T	-- Winkelgeschwindigkeit [°/s]
dt = 1/fm		-- Motion update period [s]
phi = -90
dphi = omega/fm

setTimer(dt)

while true do
	x = r*cos( grad2rad(phi) )
	z = r*sin( grad2rad(phi) )

	waitForTimer()
    core:SetSoundSourcePositionOrientationVU(S, x,1.7,z, 1,0,0, 0,1,0)	
	phi = phi+dphi
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then
		break
	end
end

-- Playback beenden
core:SetAudiofileSignalSourcePlaybackAction( X, "stop" )
core:Reset()

print("Ende im Gelaende.")
