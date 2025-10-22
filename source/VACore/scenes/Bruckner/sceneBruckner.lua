--[[

	Scene: Bruckner
	Quellen: 57
	Autoren: Jonas Stienen und Ingo Witew

	Todo Jonas: Code verschönern, Gruppen zusammenfassen und kleine Funktionen zur Manipulation der Lautstärke / Muten einbauen
	Todo Ingo: 	Quelldaten durch besseres Resampling etwas rauschfreier machen, mehr Stücke

--]]

-- VAcore
core = VACore()
core:Reset()
core:SetOutputGain(1)

-- Pfade
DataPath = "$(VADataDir)"
SceneHomePath = DataPath .."\\Scenes\\Bruckner"
HRIRDataset = "$(VADefaultHRIRDataset)";

-- Einzelne Musiker des Orchesters erzeugen
function MusikerErstellen(name, audiodatei, x, y, z, yaw, pitch, roll, volume)
	S = core:CreateSoundSource(name)
	X = core:CreateAudiofileSignalSource(SceneHomePath.."\\Audiofiles_44100Hz\\"..audiodatei, name)
	core:SetSoundSourceSignalSource(S,X)
	core:SetSoundSourcePositionOrientationYPR(S, x, y, z, yaw, pitch, roll)
	core:SetSoundSourceVolume(S, volume)
	core:SetAudiofileSignalSourcePlayState(X, "play,loop")
	return S
end

function grad2rad(grad)
	radval = grad*3.1415927/180
	return radval
end

function db2lin(db)
	lin = 10^(db/10)
	return lin
end

-- Radien und Höhen, alles in [m]

dirigent_y = 1.7 + 0.2 -- Stehhöhe des Dirigenten (+20cm Podest)
p1_y = 1.2 -- Sitzhöhe erster Zirkel
p2_y = 1.4 -- Sitzhöhe zweiter Zirkel
p3_y = 1.6 -- Sitzhöhe dritter Zirkel
p4_y = 1.8 -- Sitzhöhe vierter Zirkel

r1 =  2.0 -- Radius eins
r2 =  4.0 -- Radius zwei
r3 =  6.0 -- Radius drei
r4 =  8.0 -- Radius vier
r5 = 10.0 -- Radius fuenf
r6 = 12.0 -- Radius sechs


-- Lautstärke

global_volume = 10 -- Globale Lautstärke zur Anpassung der Eingangsdaten (die sind etwas leise)


-- Orchester eintreten lassen

core:LockScene()


-- STREICHER

-- Gruppe Violinen 1

v1p1 = MusikerErstellen("V1 Pult 1", "bruckner_vl1a_6.wav", r1*cos(grad2rad(180)), p1_y, r1*sin(grad2rad(180)), -90,0,0, global_volume);
v1p2 = MusikerErstellen("V1 Pult 2", "bruckner_vl1b_6.wav", r2*cos(grad2rad(180)), p1_y, r2*sin(grad2rad(180)), -90,0,0, global_volume);
v1p3 = MusikerErstellen("V1 Pult 3", "bruckner_vl1c_6.wav", r2*cos(grad2rad(180+25)), p1_y, r2*sin(grad2rad(180+25)), -(90+25),0,0, global_volume);
v1p4 = MusikerErstellen("V1 Pult 4", "bruckner_vl1d_6.wav", r3*cos(grad2rad(180)), p1_y, r3*sin(grad2rad(180)), -90,0,0, global_volume);
v1p5 = MusikerErstellen("V1 Pult 5", "bruckner_vl1e_6.wav", r3*cos(grad2rad(180+16)), p1_y, r3*sin(grad2rad(180+16)), -(90+16),0,0, global_volume);
v1p6 = MusikerErstellen("V1 Pult 6", "bruckner_vl1f_6.wav", r3*cos(grad2rad(180+16+16)), p1_y, r3*sin(grad2rad(180+16+16)), -(90+16+16),0,0, global_volume);
v1p7 = MusikerErstellen("V1 Pult 7", "bruckner_vl1g_6.wav", r4*cos(grad2rad(180)), p2_y, r4*sin(grad2rad(180)), -90,0,0, global_volume);
v1p8 = MusikerErstellen("V1 Pult 8", "bruckner_vl1h_6.wav", r4*cos(grad2rad(180+10)), p2_y, r4*sin(grad2rad(180+10)), -(90+10),0,0, global_volume);

-- Gruppe Violinen 2

MusikerErstellen("V2 Pult 1", "bruckner_vl2a_6.wav", r1*cos(grad2rad(180+60)), p1_y, r1*sin(grad2rad(180+60)), -(90+60),0,0, global_volume);
MusikerErstellen("V2 Pult 2", "bruckner_vl2b_6.wav", r2*cos(grad2rad(180+50)), p1_y, r2*sin(grad2rad(180+50)), -(90+50),0,0, global_volume);
MusikerErstellen("V2 Pult 3", "bruckner_vl2c_6.wav", r2*cos(grad2rad(180+25+50)), p1_y, r2*sin(grad2rad(180+25+50)), -(90+25+50),0,0, global_volume);
MusikerErstellen("V2 Pult 4", "bruckner_vl2d_6.wav", r3*cos(grad2rad(180+48)), p1_y, r3*sin(grad2rad(180+48)), -(90+48),0,0, global_volume);

-- Gruppe Viola

MusikerErstellen("Vla Pult 1", "bruckner_vlaa_6.wav", r1*cos(grad2rad(0-60)), p1_y, r1*sin(grad2rad(0-60)), 90+60,0,0, global_volume);
MusikerErstellen("Vla Pult 2", "bruckner_vlab_6.wav", r2*cos(grad2rad(0-72)), p1_y, r2*sin(grad2rad(0-72)), 90+72,0,0, global_volume);
MusikerErstellen("Vla Pult 3", "bruckner_vlac_6.wav", r2*cos(grad2rad(0-36)), p1_y, r2*sin(grad2rad(0-36)), 90+36,0,0, global_volume);

-- Gruppe Violincello

MusikerErstellen("Vcl Pult 1", "bruckner_vca_6.wav", r1*cos(grad2rad(0)), p1_y, r1*sin(grad2rad(0)), 90,0,0, global_volume);
MusikerErstellen("Vcl Pult 2", "bruckner_vcb_6.wav", r2*cos(grad2rad(0)), p1_y, r2*sin(grad2rad(0)), 90,0,0, global_volume);

-- Gruppe Kontrabass

MusikerErstellen("Kb Pult 1", "bruckner_kba_6.wav", r3*cos(grad2rad(0)), p1_y, r3*sin(grad2rad(0)), 90,0,0, global_volume);
MusikerErstellen("Kb Pult 2", "bruckner_kbb_6.wav", r3*cos(grad2rad(0-16)), p1_y, r3*sin(grad2rad(0-16)), 90+16,0,0, global_volume);

print("Streicher betreten den Saal.")


-- HOLZBLÄSER

-- Gruppe Oboen

MusikerErstellen("Ob Pult 1", "bruckner_ob1_6.wav", r3*cos(grad2rad(270+8)), p1_y, r3*sin(grad2rad(270+8)), 180-8,0,0, global_volume);
MusikerErstellen("Ob Pult 2", "bruckner_ob2_6.wav", r3*cos(grad2rad(270+3*8)), p1_y, r3*sin(grad2rad(270+3*8)), 180-3*8,0,0, global_volume);
MusikerErstellen("Ob Pult 3", "bruckner_ob3_6.wav", r3*cos(grad2rad(270+5*8)), p1_y, r3*sin(grad2rad(270+5*8)), 180-5*8,0,0, global_volume);

-- Gruppe Flöten

MusikerErstellen("Fl Pult 1", "bruckner_fl1_6.wav", r3*cos(grad2rad(180+80)), p1_y, r3*sin(grad2rad(180+80)), -(90+80),0,0, global_volume);
MusikerErstellen("Fl Pult 3", "bruckner_fl3_6.wav", r3*cos(grad2rad(180+64)), p1_y, r3*sin(grad2rad(180+64)), -(90+64),0,0, global_volume);

-- Gruppe Fagotte

MusikerErstellen("Fg Pult 1", "bruckner_bsn1_6.wav", r4*cos(grad2rad(0-85)), p2_y, r4*sin(grad2rad(0-85)), 90+85,0,0, global_volume);
MusikerErstellen("Fg Pult 2", "bruckner_bsn2_6.wav", r4*cos(grad2rad(0-75)), p2_y, r4*sin(grad2rad(0-75)), 90+75,0,0, global_volume);
MusikerErstellen("Fg Pult 3", "bruckner_bsn3_6.wav", r4*cos(grad2rad(0-65)), p2_y, r4*sin(grad2rad(0-65)), 90+65,0,0, global_volume);

--MusikerErstellen("Fg Pult 4", "bruckner_bsn1_5.wav", r4*cos(grad2rad(0-55)), p2_y, r4*sin(grad2rad(0-55)), 90+55,0,0, global_volume);
--MusikerErstellen("Fg Pult 5", "bruckner_bsn2_5.wav", r4*cos(grad2rad(0-45)), p2_y, r4*sin(grad2rad(0-45)), 90+45,0,0, global_volume);
--MusikerErstellen("Fg Pult 6", "bruckner_bsn3_5.wav", r4*cos(grad2rad(0-35)), p2_y, r4*sin(grad2rad(0-35)), 90+35,0,0, global_volume);

-- Gruppe Klarinetten

MusikerErstellen("Cl Pult 1", "bruckner_cl1_6.wav", r4*cos(grad2rad(180+85)), p2_y, r4*sin(grad2rad(180+85)), -90-85,0,0, global_volume);
MusikerErstellen("Cl Pult 2", "bruckner_cl2_6.wav", r4*cos(grad2rad(180+75)), p2_y, r4*sin(grad2rad(180+75)), -90-75,0,0, global_volume);
MusikerErstellen("Cl Pult 3", "bruckner_cl3_6.wav", r4*cos(grad2rad(180+65)), p2_y, r4*sin(grad2rad(180+65)), -90-65,0,0, global_volume);

print("Holzblaeser betreten den Saal.")

-- BLECH

-- Gruppe Hörner

MusikerErstellen("Hr Pult 1", "bruckner_corno1_6.wav", r5*cos(grad2rad(270-3.5)), p3_y, r5*sin(grad2rad(270-3.5)), 180+3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 2", "bruckner_corno2_6.wav", r5*cos(grad2rad(270-3*3.5)), p3_y, r5*sin(grad2rad(270-3*3.5)), 180+3*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 3", "bruckner_corno3_6.wav", r5*cos(grad2rad(270-5*3.5)), p3_y, r5*sin(grad2rad(270-5*3.5)), 180+5*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 4", "bruckner_corno4_6.wav", r5*cos(grad2rad(270-7*3.5)), p3_y, r5*sin(grad2rad(270-7*3.5)), 180+7*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 5", "bruckner_corno5_6.wav", r5*cos(grad2rad(270-9*3.5)), p3_y, r5*sin(grad2rad(270-9*3.5)), 180+9*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 6", "bruckner_corno6_6.wav", r5*cos(grad2rad(270-11*3.5)), p3_y, r5*sin(grad2rad(270-11*3.5)), 180+11*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 7", "bruckner_corno7_6.wav", r5*cos(grad2rad(270-13*3.5)), p3_y, r5*sin(grad2rad(270-13*3.5)), 180+13*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 8", "bruckner_corno8_6.wav", r5*cos(grad2rad(270-15*3.5)), p3_y, r5*sin(grad2rad(270-15*3.5)), 180+15*3.5,0,0, global_volume);

--[[
MusikerErstellen("Hr Pult 1", "bruckner_corno1_8.wav", r6*cos(grad2rad(270-3.5)), p4_y, r6*sin(grad2rad(270-3.5)), 180+1*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 2", "bruckner_corno2_8.wav", r6*cos(grad2rad(270-3*3.5)), p4_y, r6*sin(grad2rad(270-3*3.5)), 180+3*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 3", "bruckner_corno3_8.wav", r6*cos(grad2rad(270-5*3.5)), p4_y, r6*sin(grad2rad(270-5*3.5)), 180+5*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 4", "bruckner_corno4_8.wav", r6*cos(grad2rad(270-7*3.5)), p4_y, r6*sin(grad2rad(270-7*3.5)), 180+7*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 5", "bruckner_corno5_8.wav", r6*cos(grad2rad(270-9*3.5)), p4_y, r6*sin(grad2rad(270-9*3.5)), 180+9*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 6", "bruckner_corno6_8.wav", r6*cos(grad2rad(270-11*3.5)), p4_y, r6*sin(grad2rad(270-11*3.5)), 180+11*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 7", "bruckner_corno7_8.wav", r6*cos(grad2rad(270-13*3.5)), p4_y, r6*sin(grad2rad(270-13*3.5)), 180+13*3.5,0,0, global_volume);
MusikerErstellen("Hr Pult 8", "bruckner_corno8_8.wav", r6*cos(grad2rad(270-15*3.5)), p4_y, r6*sin(grad2rad(270-15*3.5)), 180+15*3.5,0,0, global_volume);
--]]

-- Gruppe Trompeten

trompetenvolume_dB = -6 -- die Trompeten sind etwas laut

MusikerErstellen("Tr Pult 1", "bruckner_tr1_6.wav", r5*cos(grad2rad(270+3.5)), p3_y, r5*sin(grad2rad(270+3.5)), 180-3.5,0,0, global_volume*db2lin(trompetenvolume_dB));
MusikerErstellen("Tr Pult 2", "bruckner_tr2_6.wav", r5*cos(grad2rad(270+3*3.5)), p3_y, r5*sin(grad2rad(270+3*3.5)), 180-3*3.5,0,0, global_volume*db2lin(trompetenvolume_dB));
MusikerErstellen("Tr Pult 3", "bruckner_tr3_6.wav", r5*cos(grad2rad(270+5*3.5)), p3_y, r5*sin(grad2rad(270+5*3.5)), 180-5*3.5,0,0, global_volume*db2lin(trompetenvolume_dB));

-- Gruppe Tuba

MusikerErstellen("Tuba Pult 1", "bruckner_tuba_6.wav", r5*cos(grad2rad(270+7*3.5)), p3_y, r5*sin(grad2rad(270+7*3.5)), 180-7*3.5,0,0, global_volume);
--MusikerErstellen("Tuba Pult 2", "bruckner_tuba_5.wav", r5*cos(grad2rad(270+9*3.5)), p3_y, r5*sin(grad2rad(270+9*3.5)), 180-9*3.5,0,0, global_volume);

-- Gruppe Posaunen

MusikerErstellen("Trb Pult 1", "bruckner_trb1_6.wav", r6*cos(grad2rad(270+3.5)), p4_y, r6*sin(grad2rad(270+3.5)), 180-3.5,0,0, global_volume);
MusikerErstellen("Trb Pult 2", "bruckner_trb2_6.wav", r6*cos(grad2rad(270+3*3.5)), p4_y, r6*sin(grad2rad(270+3*3.5)), 180-3*3.5,0,0, global_volume);
MusikerErstellen("Trb Pult 3", "bruckner_trb3_6.wav", r6*cos(grad2rad(270+5*3.5)), p4_y, r6*sin(grad2rad(270+5*3.5)), 180-5*3.5,0,0, global_volume);

print("Das Blech betritt den Saal.")


-- SCHLAGWERK

-- Gruppe Schlagwerk

MusikerErstellen("Timp Pult 3", "bruckner_timp_6.wav", r6*cos(grad2rad(270+8*3.5)), p4_y, r6*sin(grad2rad(270+8*3.5)), 180-8*3.5,0,0, global_volume);

print("Das Schlagwerk betritt den Saal.")


-- DIRIGENT

-- Dirigent/Hoerer
Dirigent = core:CreateListener("Dirigent")
DirigentHRIR = core:LoadHRIRDataset(HRIRDataset);
core:SetListenerHRIRDataset(Dirigent, DirigentHRIR);
core:SetListenerPositionOrientationYPR(Dirigent, 0,dirigent_y,0, 0,0,0);
core:SetActiveListener(Dirigent);

print("Der Dirigent betritt den Saal.")


core:UnlockScene()

print("Es geht los, wenn ich also um Ruhe bitten darf.")


-- An/Aus Funktionen

function Violinen1An()
	core:LockScene()
	core:SetSoundSourceMuted(v1p1, 1)
	core:SetSoundSourceMuted(v1p2, 1)
	core:SetSoundSourceMuted(v1p3, 1)
	core:SetSoundSourceMuted(v1p4, 1)
	core:SetSoundSourceMuted(v1p5, 1)
	core:SetSoundSourceMuted(v1p6, 1)
	core:SetSoundSourceMuted(v1p7, 1)
	core:SetSoundSourceMuted(v1p8, 1)
	core:UnlockScene()
end

function Violinen1Aus()
	core:LockScene()
	core:SetSoundSourceMuted(v1p1, 0)
	core:SetSoundSourceMuted(v1p2, 0)
	core:SetSoundSourceMuted(v1p3, 0)
	core:SetSoundSourceMuted(v1p4, 0)
	core:SetSoundSourceMuted(v1p5, 0)
	core:SetSoundSourceMuted(v1p6, 0)
	core:SetSoundSourceMuted(v1p7, 0)
	core:SetSoundSourceMuted(v1p8, 0)
	core:UnlockScene()
end

-- ... mehr davon.