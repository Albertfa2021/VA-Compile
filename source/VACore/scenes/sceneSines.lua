--
--   Two coherent sine sources
--   Author: Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)
--

core = VACore()
core:Reset()

core:LockScene()

signal1 = core:CreateAudiofileSignalSource("$(VADataDir)\\Audiofiles\\sine512.wav");
core:SetAudiofileSignalSourcePlayState(signal1, "play, loop");

listener = core:CreateListener("Listener")
hrir = core:LoadHRIRDataset("$(VADefaultHRIRDataset)");
core:SetListenerHRIRDataset(listener, hrir);
core:SetListenerPositionOrientationYPR(listener, 0,0,0, 180,0,0);
core:SetActiveListener(listener)

-- Abstanz genau 1m
source1 = core:CreateSoundSource("Source 1");	
core:SetSoundSourcePosition(source1, 0, 0, -3);
core:SetSoundSourceSignalSource(source1, signal1);


core:UnlockScene()

sleep(3000);


source2 = core:CreateSoundSource("Source 2");	
core:SetSoundSourcePosition(source2, 0, 0, -3);
core:SetSoundSourceSignalSource(source2, signal1);


 sleep(3000);


source3 = core:CreateSoundSource("Source 3");	
core:SetSoundSourcePosition(source3, 0, 0, -3);
core:SetSoundSourceSignalSource(source3, signal1);
