%% VA source clustering example

%
% Requires VA to run with a "real-time" renderer
%

num_vehicles = 1;
fs = 44100; % Hz
length_sounds_time = 90; % s
vehicle_height = 0.6; % m
radius = 100; % m
update_rate = 60; % Hz
receiver_pos = [ 20 1.6 0 ];

if ~exist( 'engine.wav', 'file' )
    
    disp( 'Engine sound file not found, generating ...' )
    
    fft_degree = ceil( ita_fftDegree( length_sounds_time * fs ) );
    noise = ita_generate_mls( 'fftDegree', fft_degree );
    tonal_1 = ita_generate( 'sine', 0.8 + rand( 1 ) * 0.2, 100 + rand( 1 ) * 10, fs, fft_degree, 'fullperiod' );
    tonal_2 = ita_generate( 'sine', 0.6 + rand( 1 ) * 0.2, 200 + rand( 1 ) * 20, fs, fft_degree, 'fullperiod' );
    tonal_3 = ita_generate( 'sine', 0.6 + rand( 1 ) * 0.2, 400 + rand( 1 ) * 30, fs, fft_degree, 'fullperiod' );
    tonal_4 = ita_generate( 'sine', 0.6 + rand( 1 ) * 0.2, 800 + rand( 1 ) * 40, fs, fft_degree, 'fullperiod' );
    tonal_5 = ita_generate( 'sine', 0.6 + rand( 1 ) * 0.2, 1600 + rand( 1 ) * 50, fs, fft_degree, 'fullperiod' );
    
    engine_sound = ita_merge( noise, tonal_1, tonal_2, tonal_3, tonal_4, tonal_5 );
    
    ita_write( ita_sum( engine_sound ), 'engine.wav' );
end


%% Connect
va = VA( 'localhost' );
va.add_search_path( pwd );


% Validation
rends = va.get_rendering_modules();
if ~any( strcmpi( rends.class, 'BinauralRealTime'))
  %error 'No real-time renderer activated, aborting.'
end


%% Scene set-up

% Create listener with HRTF
L = va.create_sound_receiver( 'VA_Listener' );
va.set_sound_receiver_position( L, receiver_pos )
H = va.create_directivity_from_file( 'ITA-Kunstkopf_HRIR_AP11_Pressure_Equalized_3x3_256.v17.ir.daff' );
va.set_sound_receiver_directivity( L, H );


% Create many sound sources
vehicles = cell( 1, num_vehicles );
for n = 1:num_vehicles

    S = va.create_sound_source( [ 'vehicle_' num2str( n, '%04i' ) ] );
    X = va.create_signal_source_buffer_from_file( 'engine.wav' );
    va.set_signal_source_buffer_playback_action( X, 'play' )
    va.set_signal_source_buffer_looping( X, true );
    va.set_sound_source_signal_source( S, X )

    vehicles{ n } = struct( 'sound_source_id', S, 'signal_id', X );
end

disp( 'Scene has been set up.' )


%% Dynamic scene
num_angles = 360 * 3;
i = 0;
T = 1 / update_rate;
va.set_timer( T );

while 1
    
    va.wait_for_timer; % synchronize update loop with requested update rate
    
    dd = mod( 2 * pi * i / num_angles, 2 * pi );
    i = i + 1;
    
    for n = 1:num_vehicles
        
        vehicle = vehicles{ n };
        p = [ radius * sin( dd ), vehicle_height, radius * cos( dd ) ];
        va.set_sound_source_position( vehicle.sound_source_id, p );
        
    end

end
