
r = 100.8		-- Radius [m]
b = 1.5		-- Ellipsiod parameter
T = 15		-- Umlaufdauer [s] (einer Runde) 
fm = 120	-- Motion update rate [Hz]

function grad2rad( phi )
	return phi*3.1415927/180
end

core = VACore()
core:Reset()

S = core:CreateSoundSource( "mycar" )
X = core:CreateEngineSignalSource( "myengine" )
core:SetSoundSourceSignalSource( S, X )

L = core:CreateListener( "mylistener" )
H = core:LoadHRIRDataset( "$(VADefaultHRIRDataset)" )
core:SetListenerHRIRDataset( L, H )
core:SetActiveListener( L )

core:SetListenerPosition( L, 0,1.7,0 )
core:SetSoundSourcePosition( S, 1,1,1 )

omega = 360/T	-- Winkelgeschwindigkeit [°/s]
dt = 1/fm		-- Motion update period [s]
phi = -90
dphi = omega/fm

setTimer(dt)

counter = 0
while true do
	x = b*r*cos( grad2rad(phi) )
	y = 0.7 * sin( grad2rad( phi ) ) + 1.7
	z = r*sin( grad2rad(phi) )
	
	if( math.mod( counter, 10 ) ) then
		counter = counter + 1
		k = 3 * ( math.abs( math.mod( grad2rad( phi/3 ), 1 ) ) + 1.1 )
		core:SetSignalSourceParameters( X, "set", "k", "value", k )		
	end

	waitForTimer()
	--core:SetSoundSourcePositionOrientationVU(S, x,y,z, 1,0,0, 0,1,0)
	phi = phi+dphi
	
	if getShellStatus()==1 then
		break
	end
end

core:Reset()
