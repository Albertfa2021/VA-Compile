--
--   Flyover of virtual aircraft
--   Author: Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)
--

--= Parameters =--

ScenePath = "$(VADataDir)\\Scenes\\VATSS";


--= Functions =--

function CreateSoundSources()
	-- Common resources
	dirFan = core:LoadDirectivity(ScenePath.."\\Directivity\\calculateNoise_auralisationFanPressureData_interpolated.daff");
	dirJet = core:LoadDirectivity(ScenePath.."\\Directivity\\calculateNoise_auralisationJetPressureData_interpolated.daff");
	
	-- Engine 1: Fan
	sourceFan1 = core:CreateSoundSource("Engine 1: Fan");	
	signalFan1 = core:CreateAudiofileSignalSource(ScenePath.."\\Audiofiles\\Fan.wav");
	core:SetSoundSourceSignalSource(sourceFan1, signalFan1);
	core:SetSoundSourceDirectivity(sourceFan1, dirFan);
	core:SetSoundSourceVolume(sourceFan1, volfan);
	core:SetAudiofileSignalSourcePlaybackAction( signalFan1, "play" );
	core:SetAudiofileSignalSourceIsLooping( signalFan1, 1 );
	
	-- Engine 1: Jet
	sourceJet1 = core:CreateSoundSource("Engine 1: Jet");	
	signalJet1 = core:CreateAudiofileSignalSource(ScenePath.."\\Audiofiles\\whitenoise_01.wav");
	core:SetSoundSourceSignalSource(sourceJet1, signalJet1);
	core:SetSoundSourceDirectivity(sourceJet1, dirJet);
	core:SetSoundSourceVolume(sourceJet1, voljet);
	core:SetAudiofileSignalSourcePlaybackAction( signalJet1, "play" );
	core:SetAudiofileSignalSourceIsLooping( signalJet1, 1 );

	-- Engine 2: Fan
	-- sourceFan2 = core:CreateSoundSource("Engine 2: Fan");	
	-- signalFan2 = core:CreateAudiofileSignalSource(ScenePath.."\\Audiofiles\\Fan.wav");
	-- core:SetSoundSourceSignalSource(sourceFan2, signalFan2);
	-- core:SetSoundSourceDirectivity(sourceFan2, dirFan);
	-- core:SetSoundSourceVolume(sourceFan2, volfan);
	-- core:SetAudiofileSignalSourcePlayState(signalFan2, 2+8);
	
	-- Engine 2: Jet
	-- sourceJet2 = core:CreateSoundSource("Engine 2: Jet");	
	-- signalJet2 = core:CreateAudiofileSignalSource(ScenePath.."\\Audiofiles\\whitenoise_02.wav");
	-- core:SetSoundSourceSignalSource(sourceJet2, signalJet2);
	-- core:SetSoundSourceDirectivity(sourceJet2, dirJet);
	-- core:SetSoundSourceVolume(sourceJet2, voljet);
	-- core:SetAudiofileSignalSourcePlayState(signalJet2, 2+8);
end

-- Set the position of the Aircraft in x-coordinate and height and also velocity
function SetAircraftPositionVelocity(x, h, vx)
	core:LockScene();
	
	-- Set positions of pending sound sources
	core:SetSoundSourcePositionOrientationVelocityYPR(sourceFan1, x+xfan,h,-zengine, -90,0,0, vx,0,0);
	core:SetSoundSourcePositionOrientationVelocityYPR(sourceJet1, x+xjet,h,-zengine, -90,0,0, vx,0,0);
	-- core:SetSoundSourcePositionOrientationVelocityYPR(sourceFan2, x+xfan,h,zengine, -90,0,0, vx,0,0);	
	-- core:SetSoundSourcePositionOrientationVelocityYPR(sourceJet2, x+xjet,h,zengine, 90,0,0, vx,0,0);
	
	core:UnlockScene();
end

--= Main script =--

core = VACore()
core:Reset()
--core:SetOutputMuted(1)

-- Create and setup the listener
L = core:CreateListener("Listener")
core:SetListenerPositionOrientationYPR(L, 0, 1.7, 0, 0, 0, 0);
H = core:LoadHRIRDataset("$(VADefaultHRIRDataset)");
core:SetListenerHRIRDataset(L, H);
core:SetActiveListener(L)

-- Flight parameters --

x0 = -2000
xend = 2000
h0 = 200
v = 80
dt = 0.100

-- Volumes
volfan = 1
voljet = 24

-- Relative -x position of engine components to center of aircraft [m]
xfan = 1
xjet = -1

-- Engine displacement from center of aircraft (z-axis) [m]
zengine = -5

-- ---------------------

-- Turn off playback
--core:SetOutputMuted(1);
--core:SetOutputGain(1);

CreateSoundSources();

t = 0;
x = x0;
h = h0;

setTimer(dt);

while (x < xend) do
    x = x0 + v*t;
	t = t+dt;
	
	waitForTimer();
	SetAircraftPositionVelocity(x, h, v);
	
	-- Stop-Flag abfragen und ggf. die Schleife beenden
	if (getShellStatus()==1) then
		break
	end
end

-- Ruhe im Karton
core:Reset();

