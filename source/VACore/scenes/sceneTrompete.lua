--[[

	Szene: 			Trompete
	Beschreibung:	Eine Trompete mit Directivity die in der Mitte des Raumes steht
	Autor: 			Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)

--]]

--= Parameter =--

DataPath = "$(VADataDir)";
HRIRDataset = "$(VADefaultHRIRDataset)";

r = 2;			-- Radius [m]
T = 4;			-- Dauer einer Rotation [s]
fm = 50;		-- Motion update rate [Hz]

-- Source position --
x = 0;
y = 1.7;
z = -2;

--= Hilfsfunktionen =--

-- Schallquelle erzeugen und konfigurieren
function CreateSource(name, filename, directivity, x, y, z, volume)
	S = core:CreateSoundSource(name)
	X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\"..filename)
	core:SetSoundSourceSignalSource(S,X)
	
	if ((directivity ~= nil) and (directivity ~= "")) then
		D = core:LoadDirectivity(DataPath.."\\Directivity\\Slenczka_2005_energetic_3d_octave\\"..directivity);
		core:SetSoundSourceDirectivity(S,D);
	end
	
	core:SetSoundSourcePositionOrientationVU(S, x, y, z, 0,1.7,1, 0,1,0);
	core:SetSoundSourceVolume(S, volume)
	core:SetAudiofileSignalSourcePlaybackAction(X, "play")
core:SetAudiofileSignalSourceIsLooping( X, 1 )
	return S
end


--= Skript ==-

-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetOutputGain(1)

core:LockScene()

L = core:CreateListener("Listener")
H = core:LoadHRIRDataset(HRIRDataset)
core:SetListenerPositionOrientationYPR(L, 0,1.7,2, 0,0,0);
core:SetListenerHRIRDataset(L, H)
core:SetActiveListener(L)

S = CreateSource("Trumpet", "Trompete.wav", "Trompete1.daff", x,y,z, 0.2)
core:UnlockScene()


omega = 360/T	-- Winkelgeschwindigkeit [°/s]
dt = 1/fm		-- Motion update period [s]
phi = 0
dphi = omega/fm

setTimer(dt)

-- Trompete rotieren
while true do
	waitForTimer()
	core:SetSoundSourcePositionOrientationYPR(S, x, y, z, phi, 0, 0)
	phi = phi+dphi
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then break end
end

-- ein wenig aufräumen
core:Reset() -- wir müssen resetten, da man den activen Hörer momentan noch nicht löschen kann
--[[
core:LockScene()
core:DeleteListener(L)
core:FreeHRIRDataset(H)
core:DeleteSoundSource(S)
core:FreeDirectivity(D)
core:DeleteSignalSource(X)
core:UnlockScene()
--]]

print("Ende im Gelaende!")
