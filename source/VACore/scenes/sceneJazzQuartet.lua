-- Jazz Quartet

Song = "Black Orpheus Take 2 - Version 1"

function CreateListener()
	L = core:CreateListener( "Listener" )
	H = core:LoadHRIRDataset( "$(VADefaultHRIRDataset)" )
	core:SetListenerHRIRDataset( L, H )
	core:SetListenerPosition( L, 0,1.7,0 )
	return L
end

function CreateSource( name, filename, x, y, z, volume )
	S = core:CreateSoundSource( name )
	X = core:CreateAudiofileSignalSource( "$(VADataDir)\\Audiofiles\\JazzQuartett\\Einzelspuren\\" .. Song .. "\\" .. filename )
	core:SetSoundSourceSignalSource(S,X)
	core:SetSoundSourcePositionOrientationVU(S, x, y, z, 0,0,1, 0,1,0);
	core:SetSoundSourceVolume(S, volume)
	core:SetAudiofileSignalSourcePlaybackAction( X, "play" )
	core:SetAudiofileSignalSourceIsLooping( X, 1 )
	return S
end

core = VACore()
core:Reset()
core:SetOutputGain(1)

core:LockScene()

L = CreateListener()
CreateSource( "Drums", "Drums.wav", 0, 1.1, -4, 2 )
CreateSource( "Base", "Bass.wav", 2.5, 1.1, -2.5, 2 )
CreateSource( "Piano", "Keyboard.wav", -3, 0.9, -2, 2 )
CreateSource( "Saxophone", "Sax.wav", 0, 1.2, -1.5, 0.5 )

core:UnlockScene()

core:SetActiveListener( L )