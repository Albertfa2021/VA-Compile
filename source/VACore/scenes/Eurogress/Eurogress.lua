--[[

	Szene: 			Eurogress
	Beschreibung:	Die Eurogress Demo für die CAVE mit Raumakustik
	Autor: 			Jonas Stienen (stienen@akustik.rwth-aachen.de)

--]]

-- Parameter und Abkürzungen

DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"
c = VACore()
GeoFile = "$(RavenDataBasePath)\\Models\\Eurogress\\europasaal-single.ac"

WithMike = false

--c:Reset()

-- Audioquellen
XBass  = c:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\JazzQuartett\\Einzelspuren\\Night And Day (Swing) - Version 1\\Bass.wav")
XDrums = c:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\JazzQuartett\\Einzelspuren\\Night And Day (Swing) - Version 1\\Drums.wav")
XKeys = c:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\JazzQuartett\\Einzelspuren\\Night And Day (Swing) - Version 1\\Keyboard.wav")
XSax = c:CreateAudiofileSignalSource(DataPath.."\\Audiofiles\\JazzQuartett\\Einzelspuren\\Night And Day (Swing) - Version 1\\Sax.wav")
if (WithMike) then
	XMike = c:CreateDeviceInputSignalSource(0)
end

-- Geometrie [immer zuerst]
c:LoadScene(GeoFile)

-- Datensätze


c:LockScene() -- Alles in eine neue Szene verpacken
-- Hörer
L = c:CreateListener("Listener")
c:LoadHRIRDataset(HRIRDataset, "Default HRIR dataset")
c:SetListenerPositionOrientationVU(L, 0, 1.6, 3, 0,0,-1, 0,1,0)

-- Schallquellen
SBass = c:CreateSoundSource("Jazz Quartet Bass")
c:SetSoundSourcePositionOrientationVU(SBass, -3, 1.6, -1, 0,0,1, 0,1,0)
SDrums = c:CreateSoundSource("Jazz Quartet Drums")
c:SetSoundSourcePositionOrientationVU(SDrums, -1, 1.6, -1, 0,0,1, 0,1,0)
SKeys = c:CreateSoundSource("Jazz Quartet Keys")
c:SetSoundSourcePositionOrientationVU(SKeys,  1, 1.6, -1, 0,0,1, 0,1,0)
SSax = c:CreateSoundSource("Jazz Quartet Sax")
c:SetSoundSourcePositionOrientationVU(SSax,   3, 1.6, -1, 0,0,1, 0,1,0)
if (WithMike) then
	SMike = c:CreateSoundSource("Microphone", "-DS, ER, DD, DIR, AA, -TV, -DP, -AA")
	c:SetSoundSourcePositionOrientationVU(SMike, 0, 1.6, 3, 0,0,1, 0,1,0)
end
c:UnlockScene()

-- Status setzen
c:SetSoundSourceSignalSource(SBass, XBass)
c:SetSoundSourceSignalSource(SDrums, XDrums)
c:SetSoundSourceSignalSource(SKeys, XKeys)
c:SetSoundSourceSignalSource(SSax, XSax)
if (WithMike) then
	c:SetSoundSourceSignalSource(SMike, XMike)
end
c:LockScene() -- Gleichzeitiger Start (wichtig)
c:SetAudiofileSignalSourcePlayState(XBass, "play, loop")
c:SetAudiofileSignalSourcePlayState(XDrums, "play, loop")
c:SetAudiofileSignalSourcePlayState(XKeys, "play, loop")
c:SetAudiofileSignalSourcePlayState(XSax, "play, loop")
c:UnlockScene()

c:SetActiveListener(L)

if (WithMike) then
	print("Europasaal: Vorhang auf und sprechen an")
else
	print("Europasaal: Vorhang auf")
end
	
waitForKey()