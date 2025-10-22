%% VA signal source jet engine example

% Preparations 
va = VA;
va.connect
va.reset
va.set_output_gain( .25 )
L = va.create_sound_receiver( 'listener' );
va.set_sound_receiver_position( L, [ 0 1.7 0 ] );
H = va.create_directivity_from_file( '$(DefaultHRIR)' );
va.set_sound_receiver_directivity( L, H );

% Jet engine signal
sspt_jet_engine_conf.class = 'jet_engine';
JE = va.create_signal_source_prototype_from_parameters( sspt_jet_engine_conf );

% Far-away jet engine sound source with 120 dB SWL re 12pW
S = va.create_sound_source( 'jet engine' );
va.set_sound_source_position( S, [ 0 1.7 -300 ] );
va.set_sound_source_sound_power( S, 10^(130/10) * 1e-12 );
va.set_sound_source_signal_source( S, JE );

%% GUI

va.set_signal_source_parameters( JE, struct( 'rpm', 1500 ) )
pause( 2 )
va.set_signal_source_parameters( JE, struct( 'rpm', 1600 ) )
pause( 0.2 )
va.set_signal_source_parameters( JE, struct( 'rpm', 1700 ) )
pause( 0.2 )
va.set_signal_source_parameters( JE, struct( 'rpm', 1800 ) )
pause( 0.4 )
va.set_signal_source_parameters( JE, struct( 'rpm', 2100 ) )
pause( 1.4 )
va.set_signal_source_parameters( JE, struct( 'rpm', 3800 ) )
pause( 4 )
va.set_signal_source_parameters( JE, struct( 'rpm', 5000 ) )
