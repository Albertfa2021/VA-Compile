--[[

	Szene: 			Eurogress
	Beschreibung:	Die Eurogress Demo für die CAVE mit Raumakustik und Reinsprechen
	Autor: 			Jonas Stienen (stienen@akustik.rwth-aachen.de)

--]]

-- Parameter und Abkürzungen

DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"
c = VACore()
GeoFile = DataPath.."\\Models\\Eurogress\\europasaal-single-cave.ac"

-- Audioquellen
XMike = c:CreateDeviceInputSignalSource(0)

-- Geometrie [immer zuerst]
--c:LoadScene(GeoFile)

c:LockScene() -- Alles in eine neue Szene verpacken
-- Hörer
L = c:CreateListener("Listener", "all")
c:SetListenerPositionOrientationVU(L, 0, 1.6, 0, 0,0,-1, 0,1,0)

-- Schallquellen
SMike = c:CreateSoundSource("Mikrophon")
c:SetSoundSourceAuralizationMode(SMike, "-DS")
c:SetSoundSourcePositionOrientationVU(SMike, 0, 1.5, -3, 0,0,1, 0,1,0)
c:UnlockScene()
print( "Schallquelle ", SMike, " fertig" )

-- Status setzen
c:SetSoundSourceSignalSource(SMike, "audioinput1")

c:SetActiveListener(L)

print("Eurogress: Vorhang auf - Sprechen!")

waitForKey()