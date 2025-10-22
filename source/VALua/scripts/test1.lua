-- Zugriff auf Kern-Instanz holen --
core = VACore()
print(core:GetVersionInfo())

-- Schallquelle erzeugen
source1 = core:CreateSoundSource("Quelle1")
print("Created sound source, ID = " .. source1)

-- Namen ermitteln
print("Name = " .. core:GetSoundSourceName(source1))

-- Quelle bewegen
for x=0,2,0.1 do
	core:SetSoundSourcePositionOrientationVU(source1, x,0,0, 1,0,0, 0,1,0)
	sleep(100)
end

