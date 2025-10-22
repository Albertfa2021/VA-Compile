local D = "nodebug" -- debug (300 Hz clean sine signal with perfect fade in/out down to the last sample)

core = VACore()
core:Reset()

X = core:CreateMachineSignalSource( "mymotor" )

if D == "debug_short" then
	core:SetMachineSignalSourceStartFile( X, "$(VADataDir)/Audiofiles/sine_300Hz_fade_in.wav" )
	core:SetMachineSignalSourceIdleFile( X, "$(VADataDir)/Audiofiles/sine_300Hz_single_wave.wav" )
	--core:SetMachineSignalSourceIdleFile( X, "$(VADataDir)/Scenes/Cars/Audiofiles/sine_300Hz_more_wave.wav" )
	core:SetMachineSignalSourceStopFile( X, "$(VADataDir)/Audiofiles/sine_300Hz_fade_out.wav" )
elseif D == "debug_long" then
	core:SetMachineSignalSourceStartFile( X, "$(VADataDir)/Audiofiles/sine_300Hz_fade_in_long.wav" )
	core:SetMachineSignalSourceIdleFile( X, "$(VADataDir)/Audiofiles/sine_300Hz_single_wave.wav" )
	--core:SetMachineSignalSourceIdleFile( X, "$(VADataDir)/Scenes/Cars/Audiofiles/sine_300Hz_more_wave.wav" )
	core:SetMachineSignalSourceStopFile( X, "$(VADataDir)/Audiofiles/sine_300Hz_fade_out_long.wav" )
else
	core:SetMachineSignalSourceStartFile( X, "$(VADataDir)/Scenes/Cars/Audiofiles/tractor/tractor_start.wav" )
	core:SetMachineSignalSourceIdleFile( X, "$(VADataDir)/Scenes/Cars/Audiofiles/tractor/tractor_idle.wav" )
	core:SetMachineSignalSourceStopFile( X, "$(VADataDir)/Scenes/Cars/Audiofiles/tractor/tractor_stop.wav" )
end

S = core:CreateSoundSource( "mycar" )
core:SetSoundSourceSignalSource( S, X )

L = core:CreateListener( "mylistener" )
H = core:LoadHRIRDataset( "$(VADefaultHRIRDataset)" )
core:SetListenerHRIRDataset( L, H )
core:SetActiveListener( L )

core:SetListenerPosition( L, 0,1.7,0 )
core:SetSoundSourcePosition( S, 1,1,1 )

setTimer( 1 )
waitForTimer()

core:SetMachineSignalSourceSpeed( X, 1.0 )
core:StartMachineSignalSource( X )
print( "Starting machine" )

setTimer( 1 )
while true do
	waitForTimer()
	state = core:GetMachineSignalSourceStateStr( X )
	if state == "idling" then
		print( "Machine is now idling" )
		break
	end
end

setTimer( 0.1 )
counter = 0
speed = core:GetMachineSignalSourceSpeed( X )
while true do
	counter = counter + 1
	if( counter >= 100 ) then
		counter = 0
	end
	speed = 1.0 + 0.499 * math.sin( 2 * math.pi * counter / 100 )
	core:SetMachineSignalSourceSpeed( X, speed )
	waitForTimer()
	if getShellStatus()==1 then
		core:HaltMachineSignalSource( X )
		print( "Halting machine" )
		break
	end
end

setTimer( 1 )
while true do
	waitForTimer()
	state = core:GetMachineSignalSourceStateStr( X )
	if state == "stopped" then
		print( "Machine stopped" )
		core:Reset()
		break
	end
end

