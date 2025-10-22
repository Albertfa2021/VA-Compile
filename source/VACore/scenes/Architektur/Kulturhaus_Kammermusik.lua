--[[

	Szene: 			Kulturhaus (Architektur) Kammermusik
	Beschreibung:	Demo des Architekturmodels
	Autor: 			Jonas Stienen (jst@akustik.rwth-aachen.de)

--]]

-- Parameter und Abkürzungen
DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"
DIRDataset = DataPath.."\\Directivity\\Slenczka_2005_energetic_3d_octave\\Trompete1.daff"
Audiofile = DataPath.."\\Audiofiles\\JazzCombo-BlackOrpheus.wav"
c = VACore()
c:Reset()
GeoFile = "$(RavenDataBasePath)/Models/Kulturhaus/Kulturhaus_Raven_Kammermusik.ac"

-- Audioquellen
X = c:CreateAudiofileSignalSource(Audiofile)
--XMike = "audioinput1";

-- Datensätze
H = c:LoadHRIRDataset(HRIRDataset, "Default HRIR dataset")
D = c:LoadDirectivity(DIRDataset, "Slenczka 2005 Trompete 1")

-- Geometrie [immer zuerst]
c:LoadScene(GeoFile)

-- Hörer
L = c:CreateListener("Listener", "DS, ER, DD, DIR, DP")
c:SetListenerPositionOrientationVU(L, 265,13,-440, 0,0,-1, 0,1,0)
c:SetListenerHRIRDataset(L, H);

-- Schallquellen
S = c:CreateSoundSource("Trompete", "DS, ER, DD, DIR, DP")
c:SetSoundSourcePositionOrientationVU(S, 285,10,-440, 0,0,1, 0,1,0)
c:SetSoundSourceDirectivity(S, D)

-- Status setzen
c:SetSoundSourceSignalSource(S, X)
c:SetAudiofileSignalSourcePlayState(X, "play, loop")
--c:SetSoundSourceSignalSource(S, XMike)

c:SetActiveListener(L)

print("Quader Demo: start")

c:SetSoundSourceVolume(S, 0.1)
--c:SetSoundSourceVolume(S, 1)

waitForKey()