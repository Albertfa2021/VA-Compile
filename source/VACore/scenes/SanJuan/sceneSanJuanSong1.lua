--[[

	Szene: 			San Juan, Song 1, Open 3
	Beschreibung:	Demo der San Juan Kirche
	Autor: 			Jonas Stienen (jst@akustik.rwth-aachen.de)
					Alex
					Frank Wefers

--]]

-- Parameter und Abkürzungen
DataPath = "$(VADataDir)"
HRIRDataset = "$(VADefaultHRIRDataset)"
DIRDataset = DataPath.."\\Directivity\\Slenczka_2005_energetic_3d_octave\\Saenger.daff"
c = VACore()
c:Reset()

choir = true -- Chor aktivieren (6 Quellen)
mike = false -- Mikrofon für Reinsprechen aktivieren (1 Extraquelle)


-- Audioquellen
Audiofile1 = DataPath.."\\Audiofiles\\SanJuan\\Song1\\01-singer1_song1.wav"
if( choir == true ) then
	Audiofile2 = DataPath.."\\Audiofiles\\SanJuan\\Song1\\02-singer2_song1.wav"
	Audiofile3 = DataPath.."\\Audiofiles\\SanJuan\\Song1\\03-singer3_song1.wav"
	Audiofile4 = DataPath.."\\Audiofiles\\SanJuan\\Song1\\04-singer4_song1.wav"
	Audiofile5 = DataPath.."\\Audiofiles\\SanJuan\\Song1\\05-singer5_song1.wav"
	Audiofile6 = DataPath.."\\Audiofiles\\SanJuan\\Song1\\06-singer6_song1.wav"
end
X1 = c:CreateAudiofileSignalSource( Audiofile1 )
if( choir == true ) then
	X2 = c:CreateAudiofileSignalSource(Audiofile2)
	X3 = c:CreateAudiofileSignalSource(Audiofile3)
	X4 = c:CreateAudiofileSignalSource(Audiofile4)
	X5 = c:CreateAudiofileSignalSource(Audiofile5)
	X6 = c:CreateAudiofileSignalSource(Audiofile6)
end
if( mike == true ) then
	XMike = "audioinput1"
end

-- Datensätze
H = c:LoadHRIRDataset(HRIRDataset, "Default HRIR dataset")
D = c:LoadDirectivity(DIRDataset, "Saenger 4")

-- Geometrie
GeoFile = "$(RavenDataBasePath)\\Models\\SanJuan\\SJDoubleChapelCentered.ac"
--c:CallModule( "roomrend", "use", "config", "set", "ImageSources:OrderPrimarySource", 2 )
--c:CallModule( "roomrend", "use", "config", "set", "ImageSources:OrderSecondarySource", 2 )
--c:CallModule( "roomrend", "use", "config", "set", "RayTracer:SphereDetector:EnergyLossThreshold", 63 )
--c:CallModule( "roomrend", "use", "config", "set", "RayTracer:PortalDetector:EnergyLossThreshold", 63 )
print( "Current configuration:" )
--c:CallModule( "roomrend", "print", "config" )
c:LoadScene( GeoFile )

-- Hörer
L = c:CreateListener("Listener", "all")
c:SetListenerPositionOrientationVU(L, 0,1.6,1, 0,0,-1, 0,1,0)
c:SetListenerHRIRDataset(L, H);


-- Schallquellen
M1 = c:CreateSoundSource("Monk 1", "all")
c:SetSoundSourcePositionOrientationVU(M1, -1.5, 1.6, -1.5, 0,0,1, 0,1,0)
c:SetSoundSourceDirectivity(M1, D)
if( choir == true ) then
	M2 = c:CreateSoundSource("Monk 2", "all")
	c:SetSoundSourcePositionOrientationVU(M2, -0.5, 1.6, -1.5, 0,0,1, 0,1,0)
	c:SetSoundSourceDirectivity(M2, D)
	M3 = c:CreateSoundSource("Monk 3", "all")
	c:SetSoundSourcePositionOrientationVU(M3, 0.5, 1.6, -1.5, 0,0,1, 0,1,0)
	c:SetSoundSourceDirectivity(M3, D)
	M4 = c:CreateSoundSource("Monk 4", "all")
	c:SetSoundSourcePositionOrientationVU(M4, 1.5, 1.6, -1.5, 0,0,1, 0,1,0)
	c:SetSoundSourceDirectivity(M4, D)
	M5 = c:CreateSoundSource("Monk 5", "all")
	c:SetSoundSourcePositionOrientationVU(M5, -1, 1.6, -2.2, 0,0,1, 0,1,0)
	c:SetSoundSourceDirectivity(M5, D)
	M6 = c:CreateSoundSource("Monk 6", "all")
	c:SetSoundSourcePositionOrientationVU(M6, 1, 1.6, -2.2, 0,0,1, 0,1,0)
	c:SetSoundSourceDirectivity(M6, D)
end
if( mike == true ) then
	MM = c:CreateSoundSource("Mike", "all")
	c:SetSoundSourceAuralizationMode(MM, "-DS")
	c:SetSoundSourcePositionOrientationVU(MM, 0,1.6,1, 0,0,-1, 0,1,0)
end

-- Status setzen
c:SetSoundSourceSignalSource( M1, X1 )
if( choir == true ) then
	c:SetSoundSourceSignalSource(M2, X2)
	c:SetSoundSourceSignalSource(M3, X3)
	c:SetSoundSourceSignalSource(M4, X4)
	c:SetSoundSourceSignalSource(M5, X5)
	c:SetSoundSourceSignalSource(M6, X6)
end
if( mike == true ) then
	c:SetSoundSourceSignalSource( MM, XMike )
end

c:LockScene()
c:SetAudiofileSignalSourcePlaybackAction( X1, "play" )
c:SetAudiofileSignalSourceIsLooping( X1, 1 )
if( choir == true ) then
	c:SetAudiofileSignalSourcePlaybackAction(X2, "play")
	c:SetAudiofileSignalSourceIsLooping( X2, 1 )
	c:SetAudiofileSignalSourcePlaybackAction(X3, "play")
	c:SetAudiofileSignalSourceIsLooping( X3, 1 )
	c:SetAudiofileSignalSourcePlaybackAction(X4, "play")
	c:SetAudiofileSignalSourceIsLooping( X4, 1 )
	c:SetAudiofileSignalSourcePlaybackAction(X5, "play")
	c:SetAudiofileSignalSourceIsLooping( X5, 1 )
	c:SetAudiofileSignalSourcePlaybackAction(X6, "play")
	c:SetAudiofileSignalSourceIsLooping( X6, 1 )
end
c:UnlockScene()

c:SetActiveListener(L)

print("San Juan Open3 Demo: Song1 start")

c:SetSoundSourceVolume(M1, 0.1)
if( choir == true ) then
	c:SetSoundSourceVolume(M2, 0.1)
	c:SetSoundSourceVolume(M3, 0.1)
	c:SetSoundSourceVolume(M4, 0.1)
	c:SetSoundSourceVolume(M5, 0.1)
	c:SetSoundSourceVolume(M6, 0.1)
end

waitForKey()