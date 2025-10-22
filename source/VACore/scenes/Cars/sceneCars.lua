--[[

	Szene: 			Cars
	Beschreibung:	Cars Szene wie in Inside, zum testen
	Autor: 			Jonas Stienen

--]]

--= Parameter =--

DataPath = "$(VADataDir)";
HRIRDataset = "$(VADefaultHRIRDataset)";

pos = { 6.5, 0.55, 0.0 }

car_volume = 1.0
fountain_volume = 0.04

tVehicleDefinitions = {
	{
		name = "Dodge Viper RT/10",
		soundsource = "dodge_viper",
		model = "viper.ac",
		materials = "viper.object_materials",
	},
	{
		name = "McLaren F1",
		soundsource = "mclaren_f1",
		model = "mclaren.ac",
		materials = "mclaren.object_materials",
	},
	{
		name = "1957 Ford Thunderbird",
		soundsource = "ford_thunderbird",
		model = "thunderbird.ac",
		materials = "thunderbird.object_materials",
	},
	{
		name = "1957 Chevrolet Bel-Air",
		soundsource = "chevy",
		model = "chevy.ac",
		materials = "chevy.object_materials",
	},
	{
		name = "Tractor",
		soundsource = "tractor",
		model = "tractor.ac",
		materials = "tractor.object_materials",
	}
}

tFountainDefinitions = {
	{
		name = "Fountain right low 1",
		soundsource = "fountain_rl1",
		audiofile = "fountain_side.wav",
		pos = { 0, 0, -7.5 },
	},
	{
		name = "Fountain right low 2",
		soundsource = "fountain_rl2",
		audiofile = "fountain_center.wav",
		pos = { 0, 0, -9.5 },
	},
	{
		name = "Fountain right low 3",
		soundsource = "fountain_rl3",
		audiofile = "fountain_side.wav",
		pos = { 0, 0, -11.5 },
	},
	{
		name = "Fountain left low 1",
		soundsource = "fountain_ll1",
		audiofile = "fountain_side.wav",
		pos = { 0, 0, 7.5 },
	},
	{
		name = "Fountain left low 2",
		soundsource = "fountain_ll2",
		audiofile = "fountain_center.wav",
		pos = { 0, 0, 9.5 },
	},
	{
		name = "Fountain left low 3",
		soundsource = "fountain_ll3",
		audiofile = "fountain_side.wav",
		pos = { 0, 0, 11.5 },
	},
}

-- Schallquelle erzeugen und konfigurieren
function CreateCar( tVehicle )
	tVehicle.S = core:CreateSoundSource( tVehicle.name )
	tVehicle.X = core:CreateAudiofileSignalSource( DataPath.."\\Scenes\\Cars\\Audiofiles\\".. tVehicle.soundsource.."\\idle.wav" )
	core:SetSoundSourceSignalSource( tVehicle.S, tVehicle.X )
	core:SetSoundSourcePositionOrientationVU( tVehicle.S, pos[1], pos[2], pos[3], 0,0,1, 0,1,0 );
	core:SetSoundSourceVolume( tVehicle.S, car_volume )
end

function SetActiveCar( pSoundSource )
	for i, tVehicle in pairs( tVehicleDefinitions ) do
		if pSoundSource == tVehicle.soundsource then
			print( "Active car: "..tVehicle.name )
			core:SetAudiofileSignalSourcePlayState( tVehicle.X, "play, loop" )
		else
			core:SetAudiofileSignalSourcePlayState( tVehicle.X, "stop" )
		end
	end
end

function CreateFountain( tFountain )
	tFountain.S = core:CreateSoundSource( tFountain.name )
	tFountain.X = core:CreateAudiofileSignalSource( DataPath.."\\Scenes\\Cars\\Audiofiles\\".. tFountain.audiofile )
	core:SetSoundSourceSignalSource( tFountain.S, tFountain.X )
	core:SetSoundSourcePositionOrientationVU( tFountain.S, tFountain.pos[1], tFountain.pos[2], tFountain.pos[3], 0,0,1, 0,1,0 );
	core:SetSoundSourceVolume( tFountain.S, fountain_volume )
	core:SetAudiofileSignalSourcePlayState( tFountain.X, "play, loop" )
end


--= Skript =--

-- Zugriff auf Kern-Instanz holen --
core = VACore()
core:Reset()
core:SetOutputGain(1)

core:LockScene()

-- Listener
L = core:CreateListener( "Listener" )
H = core:LoadHRIRDataset( HRIRDataset );
core:SetListenerHRIRDataset( L, H );
core:SetListenerPositionOrientationVU( L, 5,1.7,0, 1,0,0, 0,1,0 );

-- Cars
print( "Creating cars" )
for i, tCarDef in pairs( tVehicleDefinitions ) do
	CreateCar( tCarDef )
end

iCarToggleIndex = 1
function T()
	iCarToggleIndex = 1 + ( iCarToggleIndex % 4 )
	print( iCarToggleIndex )
	SetActiveCar( tVehicleDefinitions[iCarToggleIndex].soundsource )
	print( "done." )
end

SetActiveCar( tVehicleDefinitions[iCarToggleIndex].soundsource )

-- Fountains
print( "Creating fountains" )
for i, tFountain in pairs( tFountainDefinitions ) do
	CreateFountain( tFountain )
end


core:UnlockScene()

core:SetActiveListener(L)
