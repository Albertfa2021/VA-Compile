--[[

	Szene: 			BauerCircle
	Beschreibung:	Eine Quelle die sich auf einer Kreisbahn um den Hörer bewegt
	Autor: 			Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)

--]]

--= Parameter =--

DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"

r = 4		-- Radius [m]
T = 6		-- Umlaufdauer [s] (einer Runde) 
fm = 1000		-- Motion update rate [Hz]


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
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\sine440_60s.wav")
print("S = "..X)
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Hoerer")
H = core:LoadHRIRDataset(HRIRDataset)
core:SetListenerHRIRDataset(L, H)
core:SetActiveListener(L)

-- Positionen und Orientierungen setzen
core:SetListenerPositionOrientationVU(L, 0,0.0,0, 0,0,-1, 0,1,0)

-- Wiedergabe starten (Looped)
core:SetAudiofileSignalSourcePlayState(X, "play, loop")


omega = 360/T	-- Winkelgeschwindigkeit [°/s]
dt = 1/fm		-- Motion update period [s]
phi = -90
dphi = omega/fm

setTimer(dt)

while true do
	x = r*cos( grad2rad(phi) )
	z = r*sin( grad2rad(phi) )

	waitForTimer()
    core:SetSoundSourcePositionOrientationVU(S, x,0.0,z, 1,0,0, 0,1,0)	
	phi = phi+dphi
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then
		break
	end
end

-- Playback beenden
core:SetAudiofileSignalSourcePlayState(X, "stop")
core:Reset()

print("Ende im Gelaende.")
