print("\n")
print(" ----- Test von allen Listener Methoden !  -------")
print("\n")
core = VACore() -- Zgruff auf dem Kern-Instanz
print(core:GetVersionInfo())
print("\n")

listenerID = core:CreateListener( "Ahmed", 3 )

name = core:GetListenerName( listenerID )
print("Listener name : " .. name)

core:SetListenerName( listenerID, "Frank" ) 
name = core:GetListenerName( listenerID )
print("Changed Listener name : " .. name)

Auramode = core:GetListenerAuralizationMode( listenerID )
print("Listener Auralization mode : " .. Auramode)

core:SetListenerAuralizationMode( listenerID, 7 )
Auramode = core:GetListenerAuralizationMode( listenerID )
print("Changed Listener Auralization mode : " .. Auramode)

iID = core:GetActiveListener()
print("active Listener : " .. iID)

core:SetActiveListener( listenerID )

core:SetListenerPosition( listenerID, 18, 67, 24 )
x,y,z = core:GetListenerPosition( listenerID )
print("\nListenerPosition : ")
print("x = "..x..", y = "..y..", z="..z)

--core:SetListenerOrientationYPR( listenerID, 4, 5, 6 )
--print("\nListenerOrientationYPR : ")
--yaw, pitch, roll = core:GetListenerOrientationYPR( listenerID )
--print("x = "..yaw..", y = "..pitch..", z="..roll)

core:SetListenerOrientationVU( listenerID, 80, 34, 12, 44, 16, 52 )
vx,vy,vz,ux,uy,uz = core:GetListenerOrientationVU( listenerID )
print("\nListenerOrientationVU : ")
print("vx = "..vx..", vy = "..vy..", vz = "..vz.." ;ux = "..ux..", uy = "..uy..", uz = "..uz)



--core:SetListenerPositionOrientationYPR( listenerID, 1,2,3, 10,15,35 )
--print("\nListenerPositionOrientationYPR : ")
--x,y,z,yaw,pitch,roll  = core:GetListenerPositionOrientationYPR( listenerID )
--print("x = "..x..", y = "..y..", z = "..z.." ;yaw = "..yaw..", pitch = "..pitch..", roll = "..roll)

core:SetListenerPositionOrientationVU( listenerID, 1,2,3, 10,11,12, 20,22,23 )
px,py,pz,vx,vy,vz,ux,uy,uz = core:GetListenerPositionOrientationVU( listenerID )
print("\nListenerPositionOrientationVU : ")
print("px = "..px..", py = "..py..", pz = "..pz.." ; vx = "..vx..", vy = "..vy..", vz = "..vz.." ; ux = "..ux..", uy = "..uy..", uz = "..uz)


-- function to delete Listener :
function delete()

Del = core:DeleteListener( listenerID ) 

if Del == 0 then
print("Listener is successfully deleted ..")
else
print("Listener cannot deleted ..")
end

end

