--[[

	Szene: 			Quader, Ausmaße: x[-8 .. 8] y[-1.5 .. 1.5] z[-4 .. 4] (AC3D)
	Beschreibung:	Demo eines Quader-Raums zu Testzwecken
	Autor: 			Jonas Stienen (jst@akustik.rwth-aachen.de)

--]]
 
c = VACore()
c:Reset()

c:CallModule( "roomrend", "use", "sched", "print", "info" )
c:CallModule( "roomrend", "use", "sched", "print", "status" )

X = c:CreateAudiofileSignalSource( "Audiofiles/Trompete.wav" )

H = c:LoadHRIRDataset( "$(DefaultHRIR)" )
D = c:LoadDirectivity( "Directivity/Slenczka_2005_energetic_3d_octave/Trompete1.daff", "Slenczka 2005 Trumpet 1" )

print( "Loading scene ... " )
c:LoadScene( "$(RavenDataBasePath)/Models/Quader/quader.ac" )
print( "Done." )

L = c:CreateListener( "Listener", "DS, ER, DD, DIR, DP" )
c:SetListenerPosition( L, 0, 1.6, 1.5 )
c:SetListenerHRIRDataset( L, H );

S = c:CreateSoundSource( "Trumpet", "DS, ER, DD, DIR, DP" )
c:SetSoundSourceVolume( S, 0.1 )
c:SetSoundSourceOrientationYPR( S, 180, 0, 0 )
c:SetSoundSourceDirectivity( S, D )

c:SetSoundSourceSignalSource( S, X )
c:SetAudiofileSignalSourcePlaybackAction( X, "play" )
c:SetAudiofileSignalSourceIsLooping( X, 1 )

c:SetActiveListener( L )

print( "Cubic demo: start" )

setTimer( 0.10 )
df = 0
p = 0
amp = 5.1
px = 0
while( px > -4 ) do
	px = p + amp * math.cos( 2 * math.pi * df ) - amp / 2.0
	c:SetSoundSourcePosition( L, px, 1.6, -1.5 )
	df = math.mod( df, 1.0 ) + 0.01;
	waitForTimer()
end


c:CallModule( "roomrend", "use", "sched", "print", "status" )
