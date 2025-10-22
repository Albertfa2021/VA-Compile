core = VACore()

-- Signalquelle erzeugen
af1 = core:CreateAudiofileSignalSource("Bauer.wav", "Bauer")
print("Created audiofile signal source source, ID = " .. af1)

sleep(5000)

print("Pausing");
core:SetAudiofileSignalSourcePlayState(af1, 2);
sleep(5000)

print("Unpausing");
core:SetAudiofileSignalSourcePlayState(af1, 1);
sleep(5000)

print("Stopping");
core:SetAudiofileSignalSourcePlayState(af1, 0);
sleep(5000);

print("Starting");
core:SetAudiofileSignalSourcePlayState(af1, 1);
sleep(5000);

print("Rewinding");
core:SetAudiofileSignalSourcePlayState(af1, 3);
sleep(5000);

print("Stopping");
core:SetAudiofileSignalSourcePlayState(af1, 0);
sleep(5000);

print("Starting");
core:SetAudiofileSignalSourcePlayState(af1, 1);
sleep(5000);

print("Script finished");
