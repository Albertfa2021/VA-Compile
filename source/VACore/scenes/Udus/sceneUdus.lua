core = VACore()
core:Reset()
core:SetOutputGain( 0.7 )

SceneHomePath = "$(VADataDir)\\Scenes\\Udus";

function CreateListener()
	L = core:CreateListener( "Listener" )
	H = core:LoadHRIRDataset( "$(VADefaultHRIRDataset)" );
	core:SetListenerHRIRDataset( L, H );
	core:SetListenerPositionOrientationYPR(L, 0,1.7,0, 0,0,0);
	return L
end

function CreateSource( name, filename, x, y, z, yaw, pitch, roll, volume )
	S = core:CreateSoundSource( name )
	X = core:CreateAudiofileSignalSource( SceneHomePath.."\\Audiofiles\\"..filename, name )
	core:SetSoundSourceSignalSource( S, X )
	core:SetSoundSourcePositionOrientationYPR( S, x, y, z, yaw, pitch, roll );
	core:SetSoundSourceVolume( S, volume )
	core:SetAudiofileSignalSourcePlaybackAction( X, "PlAy" )
	core:SetAudiofileSignalSourceIsLooping( X, 1 )
	return S
end

core:LockScene()

L = CreateListener()

CreateSource( "Udu 1", "Udu 1.wav", -0,0.35,-1.7, 180,0,0, 1 )
CreateSource( "Udu 2", "Udu 2.wav", -1.5,0.35,-1.2, 180+30,0,0, 1 )
CreateSource( "Udu 3", "Udu 3.wav", 1.5,0.35,-1.2, 180-30,0,0, 1 )

CreateSource( "Udongo", "Udongo.wav", -0.1,0.30,1.8, 10,0,0, 1 )
CreateSource( "Mbwata 1", "Mbwata 1.wav", -1.4,0.50,1.4, -45,0,0, 1 )
CreateSource( "Mbwata 2", "Mbwata 2.wav", 1.5,0.55,1.2, 34,0,0, 1 )

CreateSource( "Caxixi", "Caxixi.wav", 2.2,1.3,-2.6, 0,0,0, 1 )
CreateSource( "Shaker 1", "Shaker 1.wav", 0.6,1.6,2.6, 0,0,0, 1 )
CreateSource( "Shaker 2", "Shaker 2.wav", -1.2,1.3,-2.4, 0,0,0, 1 )

core:UnlockScene()

core:SetActiveListener( L )
