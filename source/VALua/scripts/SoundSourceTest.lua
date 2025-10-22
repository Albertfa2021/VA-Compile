print("\n")
print(" ----- Test von allen Sound Sources Methoden !  -------")
print("\n")
core = VACore() -- Core wird erzeugt

soundSourceID = core:CreateSoundSource("Quelle_1", 3, 4.6) -- erzeugt eine Schallquelle

Auramode = core:GetSoundSourceAuralizationMode( soundSourceID ) 
print("AuralizationMode : " .. Auramode)

SoundSourcename = core:GetSoundSourceName( soundSourceID )
print("Sound Source name : " .. SoundSourcename)

core:SetSoundSourceName( soundSourceID, "Quelle_2" )

SoundSignalSourceID = core:GetSoundSourceSignalSource( soundSourceID )
print("Signal Source ID : " .. SoundSignalSourceID)

core:SetSoundSourceAuralizationMode( soundSourceID, 5) 
Auramode = core:GetSoundSourceAuralizationMode( soundSourceID ) 
print("Changed AuralizationMode : " .. Auramode)

SoundSourceVolume = core:GetSoundSourceVolume( soundSourceID )
print("Sound Source Volume : " .. SoundSourceVolume)

core:SetSoundSourceVolume( soundSourceID, 12.5 )
SoundSourceVolume = core:GetSoundSourceVolume( soundSourceID )
print("Changed Sound Source Volume : " .. SoundSourceVolume)

isMuted = core:IsSoundSourceMuted( soundSourceID )

if isMuted == false then
print("Sound Source is not Muted")
end

isMuted = core:SetSoundSourceMuted( soundSourceID, 1 ) 
if isMuted == true then
print("Sound Source is Muted")
end


core:SetSoundSourcePosition( soundSourceID, 1, 2, 3 )
x, y, z = core:GetSoundSourcePosition( soundSourceID, x, y, z )
print("\nSoundSourcePosition : ")
print("x = "..x..", y = "..y..", z="..z)

--core:SetSoundSourceOrientationYPR( soundSourceID, 4, 5, 6 )
--yaw, pitch, roll = core:GetSoundSourceOrientationYPR( soundSourceID )
--print("x = "..yaw..", y = "..pitch..", z="..roll)

core:SetSoundSourceOrientationVU( soundSourceID, 1,2,3,4,5,6)
vx, vy, vz, ux, uy, uz = core:GetSoundSourceOrientationVU( soundSourceID )
print("\nSoundSourceOrientationVU : ")
print("vx = "..vx..", vy = "..vy..", vz = "..vz.." ;ux = "..ux..", uy = "..uy..", uz = "..uz)

--core:SetSoundSourcePositionOrientationYPR( soundSourceID, 1,2,3, 10,15,35 )
--x,y,z,yaw,pitch,roll  = core:GetSoundSourcePositionOrientationYPR( soundSourceID )
--print("x = "..x..", y = "..y..", z = "..z.." ;yaw = "..yaw..", pitch = "..pitch..", roll = "..roll)

core:SetSoundSourcePositionOrientationVU( soundSourceID, 1,2,3, 10,11,12, 20,22,23 )
px,py,pz,vx,vy,vz,ux,uy,uz = core:GetSoundSourcePositionOrientationVU( soundSourceID )
print("\nSoundSourcePositionOrientationVU : ")
print("px = "..px..", py = "..py..", pz = "..pz.." ; vx = "..vx..", vy = "..vy..", vz = "..vz.." ; ux = "..ux..", uy = "..uy..", uz = "..uz)

-- function to delete Sound Source :
function delete()

Del = core:DeleteSoundSource( soundSourceID ) 

if Del == 0 then
print("Sound Source is successfully deleted ..")
else
print("Sound Source is not deleted ..")
end

end

