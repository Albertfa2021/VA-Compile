--[[

	Szene: 			DopplerLine
	Beschreibung:	Eine Quelle die sich auf einer Linie sinusartig "schwingt" (Doppler-Effekte, Baby!)
	Autor: 			Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)

--]]

--= Parameter =--

DataPath = "$(VADataDir)";
HRIRDataset = "$(VADefaultHRIRDataset)";

x0 = 0;			-- Seitliche Verschiebung zum Hörer [m]
rs = 10;		-- Schwindungslänge [m]
rmin = 3;		-- Minimalabstand zum Hörer
T = 4;			-- Dauer [s] (einer Vollschwingung) 
fm = 50;		-- Motion update rate [Hz]


--= Hilfsfunktionen =--

function grad2rad(phi)
	return phi*3.1415927/180
end


--= Skript ==-

-- Zugriff auf Kern-Instanz holen
core = VACore()
core:Reset()
core:SetOutputGain(1)

-- Schallquelle erzeugen
S = core:CreateSoundSource("Quelle")
X = core:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\sine440_60s.wav")
core:SetSoundSourceSignalSource(S,X)

-- Hörer erzeugen, konfigurieren und aktivieren
L = core:CreateListener("Hörer")
H = core:LoadHRIRDataset(HRIRDataset);
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)

-- Positionen und Orientierungen setzen
core:SetListenerPositionOrientationVU(L, 0,1.7,0, 0,0,-1, 0,1,0);

-- Wiedergabe starten (Looped)
core:SetAudiofileSignalSourcePlayState(X, "play, loop");


z0 = rmin + rs/2;	-- Mittelpunkt der Schwingung
omega = 360/T;		-- Winkelgeschwindigkeit [°/s]
dt = 1/fm;			-- Motion update period [s]

phi = 0;
dphi = omega/fm;

setTimer(dt);

while true do
	z = -z0 + rs/2*sin( grad2rad(phi) );

	waitForTimer();
    core:SetSoundSourcePositionOrientationVU(S, x0,1.7,z, 1,0,0, 0,1,0)	
	phi = phi+dphi;
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if getShellStatus()==1 then
		break
	end
end

print("Ende im Gelände!")
