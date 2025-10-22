core = VACore()
core:Reset();

core:LockScene() -- lock to hold back modifications

D = core:LoadDirectivity( "$(HumanDir)" )
X = core:CreateAudiofileSignalSource( "Audiofiles/Bauer.wav" )

S = core:CreateSoundSource( "MyBauerSource" )
core:SetSoundSourceSignalSource( S, X )

L = core:CreateListener( "MyListener" )
H = core:LoadHRIRDataset( "$(DefaultHRIR)" )

core:SetSoundSourceDirectivity( S, D )
core:SetListenerHRIRDataset( L, H )

core:SetSoundSourcePosition( S, 2, 1.7, -2 )
core:SetSoundSourceOrientationYPR( S, 180, 0, 0 )
core:SetListenerPosition( L, 0, 1.7, 0 )

core:SetAudiofileSignalSourcePlaybackAction( X, "play" )
core:SetAudiofileSignalSourceIsLooping( X, 1 )

core:UnlockScene() -- apply synchronized scene modification (all changes are made simultaneously)

core:SetActiveListener( L )
