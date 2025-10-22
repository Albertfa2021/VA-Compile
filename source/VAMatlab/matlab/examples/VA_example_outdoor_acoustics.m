%% VA offline simulation/auralization example for an outdoor noise scenario

% Requires VA to run with a virtual audio device that can be triggered by
% the user. Also the generic path prototype rendering module(s) has to record the output
% to hard drive.

cont = struct();
conf.free_field_only = false;           % No occlusion, no reflections, will override field component setup
conf.direct_field_component = true;     % If true, direct sound field component is included
conf.diffracted_field_component = true; % If true, direct sound field component is included
conf.reflected_field_component = true;  % Causes heavy phasing due to comb filter effects

conf.update_gain_individually = true;   % If true, updates gain in DSP components, otherwise gain is applied to filter coefficients
conf.with_delay_update = true;          % If true, updates the delay in VDL DSP component, otherwise propagation delay is ignored

buffer_length = 128;
sampling_rate = 44100;
f = ita_ANSI_center_frequencies;

receiver_pos = [ 3 0 3 ];                           % OpenGL coordinates in meter
source_pos_range = [ [ -20 0 -3 ]; [ 20 0 -3 ] ];   % OpenGL coordinates in meter
source_velocity = 30 / 3.6;                         % m/s

% House corner
w = itaInfiniteWedge( [ 1 0 0 ], [ 0 0 -1 ], [ 0 0 0 ] ); % OpenGL coordinates


%% Connect and set up simple scene
va = VA;

try
    
    va.connect;
    dry_run = ~va.get_connected;
    
    va.add_search_path( 'D:/Users/andrew/dev/ITASuite/VA/VACore/data' );
    va.add_search_path( 'D:/Users/andrew/dev/ITASuite/VA/VAMatlab/matlab' );
    va.add_search_path( 'C:/dev/VA/VACore/data' );
    va.add_search_path( 'D:/Users/stienen/dev/VA/VACore/data' );
    va.add_search_path( 'C:/ITASoftware/Raven/RavenDatabase/SoundDatabase' );
    va.add_search_path( 'C:\Users\jonas\sciebo\ITA\Lehre\Masterarbeiten\2019 Henry Andrew\car_pass-by_corner' );
    va.add_search_path( 'D:\Users\stienen\Sciebo\Thesis stuff\auralization\resources' );
    
    c = va.get_homogeneous_medium_sound_speed();
    
    L = va.create_sound_receiver( 'VA_Listener' );
    va.set_sound_receiver_position( L, receiver_pos )
    %H = va.create_directivity_from_file( '$(DefaultHRIR)' );
    H = va.create_directivity_from_file( 'ITA-Kunstkopf_HRIR_AP11_Pressure_Equalized_3x3_256.v17.ir.daff' );

    va.set_sound_receiver_directivity( L, H );

    S = va.create_sound_source( 'VA_Source' );

    %X = va.create_signal_source_buffer_from_file( 'WelcomeToVA.wav' );
    X = va.create_signal_source_buffer_from_file( 'LKW Leerlauf 2.wav' );
    %X = va.create_signal_source_buffer_from_file( 'complex_tonal.wav' );
    %X = va.create_signal_source_buffer_from_file( 'complex_tonal_and_noise.wav' );
    %X = va.create_signal_source_buffer_from_file( 'pinknoise.wav' );
    %X = va.create_signal_source_buffer_from_file( 'Full song - Vulfpeck - Dean Town.wav' );
    va.set_signal_source_buffer_playback_action( X, 'play' )
    va.set_signal_source_buffer_looping( X, true );
    va.set_sound_source_signal_source( S, X )

catch
    
    disp( 'Executing dry run because connection to local VA server failed.' )
    dry_run = true;
    c = 343;
    S = -1;
    L = -1;
    
end

k = 2 * pi * f ./ c;


%% Synchronized scene update & audio processing simulation/auralization

timestep = buffer_length / sampling_rate; % here: depends on block size and sample rate
disp( [ 'Resulting time step resolution: ' num2str( timestep ) 's' ] )

traj_distance = norm( source_pos_range( 1, : ) - source_pos_range( 2, : ) );
traj_duration = traj_distance / source_velocity;
numsteps = ceil( traj_duration / timestep );

fprintf( 'Simulation trajectory distance: %.1fs\n', traj_distance )
fprintf( 'Simulation result duration: %.1fs\n', traj_duration )
fprintf( 'Simulation frames total: %i\n', numsteps )

manual_clock = 0;
if ~dry_run
    va.set_core_clock( 0 );
end

source_pos_traj = zeros( numsteps, 3 );
source_pos_traj( :, 1 ) = linspace( source_pos_range( 1, 1 ), source_pos_range( 2, 1 ), numsteps );
source_pos_traj( :, 2 ) = linspace( source_pos_range( 1, 2 ), source_pos_range( 2, 2 ), numsteps );
source_pos_traj( :, 3 ) = linspace( source_pos_range( 1, 3 ), source_pos_range( 2, 3 ), numsteps );

H_direct_log = []; % Direct field component
H_diffracted_log = []; % Diffracted field component
H_reflected_log = []; % Reflected field component
H_total_field_log = []; % Superimposed field components
IL_log = []; % Insertion loss (total field relative to direct field under free-field condition)

h = waitbar( 0, 'Hold on, running auralization' );
direct_deleted = false; %set to true when the direct path has been deleted
for n = 1:numsteps
    
    source_pos = source_pos_traj( n, : ); % OpenGL coordinates    
    if ~dry_run
        va.set_sound_source_position( S, source_pos );
    end        
    
    %% Direct sound path
    
    % Manually create direct sound path and diffracted sound path
    shadow_zone = ita_diffraction_shadow_zone( w, source_pos, receiver_pos ); %is receiver in shadow zone?
    if n > 1 && last_shadow_zone ~= shadow_zone
        fprintf( 'Shadow zone boundary has been crossed on frame %i (time %.2f)\n', n, n * timestep ) 
    end
    last_shadow_zone = shadow_zone;
    
    distance = norm( source_pos - receiver_pos );
    H_direct = 1 ./ distance .* exp( -1i .* k .* distance );
    
    prop_path_direct = struct();
    prop_path_direct.identifier = 'direct_path';
    prop_path_direct.frequencies = f; %Frequencies corresponding to the mags
    prop_path_direct.source = S; %sound source ID
    prop_path_direct.receiver = L; %sound receiver ID
    prop_path_direct.delete = false; %set to true when an existing path should be deleted
    prop_path_direct.position = source_pos;
    
    if conf.free_field_only
        prop_path_direct.audible = true; % Free field: always audible
    else
        prop_path_direct.audible = ~shadow_zone; % Direct field: may be occluded
    end
    
    if conf.update_gain_individually
        prop_path_direct.gain = 1 / distance;
        prop_path_direct.frequency_magnitudes = ones( 1, 31 );
    else
        prop_path_direct.gain = 1;
        prop_path_direct.frequency_magnitudes = abs( H_direct );
    end
    
    % Set delay (once if disabled)
    if conf.with_delay_update || n == 1
        prop_path_direct.delay = distance / c; %delay from sound emitting from source to being received at listener
    end
    
    
    %% Diffracted sound path
    
    apex = w.get_aperture_point( source_pos, receiver_pos );
    detour = norm( source_pos - apex ) + norm( apex - receiver_pos );
    [ H_diffracted, D, A ] = ita_diffraction_utd( w, source_pos, receiver_pos, f, c );
    rho = norm( apex - source_pos );
        
    prop_path_diffracted = struct();
    prop_path_diffracted.source = S;
    prop_path_diffracted.receiver = L;
    prop_path_diffracted.identifier = 'diffracted_path';
    prop_path_diffracted.delete = false;
    prop_path_diffracted.position = apex;
    prop_path_diffracted.frequencies = f;
    prop_path_diffracted.audible = true;
        
    if conf.update_gain_individually
        prop_path_diffracted.gain = ( -1 ) * A / rho; % Invert phase
        prop_path_diffracted.frequency_magnitudes = abs( D );
    else
        prop_path_diffracted.gain = -1; % Invert phase
        prop_path_diffracted.frequency_magnitudes = abs( H_diffracted );
    end
    
    if ~conf.with_delay_update
        if n == 1
            prop_path_diffracted.delay = distance / c; % Set all-same distance once if disabled
        end
    else
            prop_path_diffracted.delay = detour / c;
    end
    
    
    %% Reflected sound path
    
    reflection_zone = ita_diffraction_reflection_zone( w, source_pos, receiver_pos, true );
    if n > 1 && last_reflection_zone ~= reflection_zone
        fprintf( 'Reflection zone boundary has been crossed on frame %i (time %.2f)\n', n, n * timestep ) 
    end
    last_reflection_zone = reflection_zone;
    
    distance_source_main_face = dot( w.main_face_normal, source_pos - w.location );
    assert( ~reflection_zone || distance_source_main_face >= 0 )
    
    source_pos_image = source_pos - 2 * w.main_face_normal * distance_source_main_face;
    
    distance_image = norm( source_pos_image - receiver_pos );
    
    prop_path_reflected = struct();
    prop_path_reflected.identifier = 'reflected_path';
    prop_path_reflected.frequencies = f; %Frequencies corresponding to the mags
    prop_path_reflected.source = S; %sound source ID
    prop_path_reflected.receiver = L; %sound receiver ID
    prop_path_reflected.delete = false; %set to true when an existing path should be deleted
    prop_path_reflected.position = source_pos_image;
    prop_path_reflected.audible = reflection_zone; % Reflected field: may be inaudible
    
    R = 0.7;
    H_reflected = R ./ distance_image .* exp( -1i .* k ./ distance_image );
    
    if conf.update_gain_individually
        prop_path_reflected.gain = mean( R ) / distance_image;
        prop_path_reflected.frequency_magnitudes = ones( 1, 31 );
    else
        prop_path_reflected.gain = 1;
        prop_path_reflected.frequency_magnitudes = abs( H_reflected );
    end
    
    if ~conf.with_delay_update
        if n == 1
            prop_path_reflected.delay = distance / c; % For all-same delay, set once only if disabled
        end
    else
        prop_path_reflected.delay = distance_image / c;
    end
    
    
    %% Update wave fronts in renderer
    paths_update = struct();
    if conf.free_field_only || conf.direct_field_component
        paths_update.direct_path = prop_path_direct;
    end
    if ~conf.free_field_only && conf.diffracted_field_component
        paths_update.diffracted_path = prop_path_diffracted;
    end
    if ~conf.free_field_only && conf.reflected_field_component
        paths_update.reflected_path = prop_path_reflected;
    end
    
    if ~dry_run
        
        rends = va.get_rendering_modules();
        for rend_idx = 1:numel( rends )
            if strcmpi( rends( rend_idx ).class, 'BinauralOutdoorNoise' )
                va.set_rendering_module_parameters( rends( rend_idx ).id, paths_update );
            end
        end

        % Increment core clock
        manual_clock = manual_clock + timestep;
        va.call_module( 'manualclock', struct( 'time', manual_clock ) );

        % Process audio chain by incrementing one block
        va.call_module( 'virtualaudiodevice', struct( 'trigger', true ) );
        
    end
    
    waitbar( n / numsteps )
    
    
    % Log transfer functions
    
    H_direct_log = [ H_direct_log; H_direct ];
    H_diffracted_log = [ H_diffracted_log; H_diffracted ];
    H_reflected_log = [ H_reflected_log; H_reflected ];
    
    if shadow_zone
        H_total_field = H_diffracted;
    else
        if reflection_zone
            H_total_field = H_reflected + H_diffracted + H_direct;
        else
            H_total_field = H_diffracted + H_direct;
        end
    end
    
    H_total_field_log = [ H_total_field_log; H_total_field ];
    IL_log = [ IL_log; H_total_field ./ H_direct ];
     
end
close( h )

if ~dry_run
    va.disconnect
    disp( 'Stop VA to export simulation results from rendering module(s)' )    
else
    figure
    plot( db( H_total_field_log ) ) % @todo make nicer plot
end
