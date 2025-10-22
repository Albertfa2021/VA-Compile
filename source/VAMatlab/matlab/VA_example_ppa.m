%% VA offline simulation/auralization example for outputs of the propagation path algorithm

% Requires VA to run with a virtual audio device that can be triggered by
% the user. Also the generic path prototype rendering module(s) has to record the output
% to hard drive.

% Requires ITA-Toolbox, obtain from http://www.ita-toolbox.org

warning( 'off', 'all' )

%output_folder = fullfile( 'C:\Users\jonas\sciebo\Thesis stuff\auralization\resources', 'example_ppa/s1_r1' );
output_folder = fullfile( 'D:\Users\stienen\Sciebo\Thesis stuff\auralization\resources', 'example_ppa/s1_r1' );
frame_rate = 128 / 44100; % block length / sampling frequency
sort_by_date = false;

record_paths = false;
ppa_diffraction_model = 'utd';
%single_path_id = 'path_003ca6d3a74540310873574908219251';
max_order_auralization = 0;

S = 1;
R = 1;

ppa_example_mode = questdlg( 'Please select', 'VA example ppa mode selection', ...
    'simulation', 'auralization', 'all', 'auralization');


%% Prepare run data
if any( strcmpi( ppa_example_mode, { 'simulation', 'all' } ) )
    
    if ~exist( output_folder, 'dir' )
        mkdir( output_folder )
    end
    
    ppa_folder = '../../../dist/win32-x64.vc12/bin/UrbanTrajectory';
    
    if ~exist( ppa_folder, 'dir' )
        ppa_folder = '../../../ITAGeometricalAcoustics/ITAPropagationPathSim/tests/CombinedModel/UrbanTrajectory';
    end
    
    if ~exist( ppa_folder, 'dir' )
        error( 'Could not find trajectory source folder "%s".', ppa_folder )
    end
    
    file_listing = dir( strcat( ppa_folder, '/*.json' ) );
    
    f = ita_ANSI_center_frequencies;
    c = 343;
    
    all_path_names = cell(1);
    path_count = 0; %The total number of paths, NOTE -NOT a count of paths in current frame, if paths are deleted, they are not removed from this count
    all_paths_data = struct;
    
    
    N = numel( file_listing );
    disp( [ 'Simulation result duration: ' num2str( N * frame_rate ) ' s' ] )
    
    h = waitbar( 0, 'Hold on, running simulation of path data for each frame' );
    
    % Iterate over frames
    n_continue = 1;
    for n = n_continue:N
        
        % Load propagation paths for current frame
        ppa_file_path = fullfile( file_listing( n ).folder, file_listing( n ).name );
        pps = ita_propagation_load_paths( ppa_file_path ); %pps = struct containing the current time frame
        pps = ita_propagation_paths_add_identifiers( pps );
        
        if n == n_continue
            pps_new = pps;
            pps_del = [];
            pps_common = [];
        else
            [ pps_new, pps_del, pps_common ] = ita_propagation_paths_diff( pps_last, pps );
        end
        pps_last = pps;
        
        
        % Update source (first anchor)
        if isa( pps( 1 ).propagation_anchors, 'struct' )
            source_pos = pps( 1 ).propagation_anchors( 1 ).interaction_point(1:3); % OpenGL coordinates?! -> transform using
            receiver_pos = pps( 1 ).propagation_anchors( end ).interaction_point(1:3); % OpenGL coordinates?! -> transform using
        else
            source_pos = pps( 1 ).propagation_anchors{ 1 }.interaction_point(1:3); % OpenGL coordinates?! -> transform using
            receiver_pos = pps( 1 ).propagation_anchors{ end }.interaction_point(1:3); % OpenGL coordinates?! -> transform using
        end
        
        paths_update = struct();
        
        % Delete non-available paths
        for p = 1:numel( pps_del )
            pu = struct(); % Path update
            pu.source = S;
            pu.receiver = R;
            pu.identifier = pps_del( p ).identifier;
        end
        
        for p = 1:numel( pps_new )
            
            pp = pps_new( p ); % Propagation path
            pu = struct(); % Path update
            
            % Assemble DSP settings (gain, delay & filter coefficients)
            pu.source = S;
            pu.receiver = R;
            pu.identifier = pp.identifier;
            [ refl_order, diffr_order ] = ita_propagation_get_orders( pp );
            pu.reflection_order = refl_order;
            pu.diffraction_order = diffr_order;
            
            path_count = path_count + 1;
            all_path_names{path_count} = pp.identifier;
            
            if( record_paths )
                [ frequency_mags, gain, delay, valid_path, path_data ] = ita_propagation_path_get_data( pp, f, c, 'record_paths' );
                all_paths_data.( strcat( 'path_', pu.identifier ) ) = path_data;
            else
                [ frequency_mags, gain, delay, valid_path ] = ita_propagation_path_get_data( pp, f, c );
            end
            
            if valid_path
                pu.gain = gain;
                pu.frequency_magnitudes = abs(frequency_mags);
            end
            pu.frequencies = f;
            pu.delay = delay;
            if isa(pp.propagation_anchors, 'struct')
                pu.position = pp.propagation_anchors( end-1 ).interaction_point( 1:3 ); % next to last anchor
            else
                pu.position = pp.propagation_anchors{ end-1 }.interaction_point( 1:3 ); % next to last anchor
            end
            pu.delete = false;
            pu.audible = true;
            
            paths_update.( strcat( 'path_', pu.identifier ) ) = pu;
        end
        
        for p = 1:numel( pps_common )
            
            pp = pps_common( p ); % Propagation path
            pu = struct(); % Path update
            
            % Assemble DSP settings (gain, delay & filter coefficients)
            pu.source = S;
            pu.receiver = R;
            pu.identifier = pp.identifier;
            
            if( record_paths )
                [ frequency_mags, gain, delay, valid_path, path_data ] = ita_propagation_path_get_data( pp, f, c, 'record_paths' );
                all_paths_data.( strcat( 'path_', pu.identifier ) ) = path_data;
            else
                [ frequency_mags, gain, delay, valid_path ] = ita_propagation_path_get_data( pp, f, c );
            end
            
            if valid_path
                pu.gain = gain;
                pu.frequency_magnitudes = abs(frequency_mags);
            end
            pu.frequencies = f;
            pu.delay = delay;
            if isa( pp.propagation_anchors, 'struct' )
                pu.position = pp.propagation_anchors( end-1 ).interaction_point( 1:3 ); % next to last anchor
            else
                pu.position = pp.propagation_anchors{ end-1 }.interaction_point( 1:3 ); % next to last anchor
            end
            pu.delete = false;
            pu.audible = true;
            
            paths_update.( strcat( 'path_', pu.identifier ) ) = pu;
        end
        
        file_name = sprintf( 'frame%04idata.mat', n );
        file_path = fullfile( output_folder, file_name );
        save( file_path, 'paths_update', 'source_pos', 'receiver_pos' ); % save current frame data
        if( record_paths )
            save( file_path, 'all_paths_data', '-append' );
        end
        
        waitbar( n / N )
        
    end
    close( h )
    
    if record_paths
        save( fullfile( output_folder, 'analysis_data.mat' ), 'all_path_names' ) %save variables to file
    end
    
    fprintf( 'Finished generating path data for every frame. Data is saved into separate files in folder ''%s''\n', output_folder );
    
    
    
    %% Run postprocessing
    
    disp( 'Starting post-processing' )
    
    % Load data
    
    file_listing = dir( strcat( output_folder, '/frame*data.mat' ) );
    N = numel( file_listing );
    fprintf( 'Found data on %i frames in folder ''%s''\n', N, output_folder )
    
    if sort_by_date
        frame_datenums = zeros( N, 1 );
        for n=1:N
            frame_datenums( n ) = file_listing( n ).datenum;
        end
        [ ~, idx ] = sort( frame_datenums );
    else
        idx = 1:N;
    end
    
    h = waitbar( 0, 'Hold on, running postprocessing on generated data' );
    
    % Run postprocessing over frames from front and back
    for n = 1:N
        
        idx_current_front = idx( n );
        load( fullfile( output_folder, file_listing( idx_current_front ).name ), '-mat' );
        
        pathnames = fieldnames( paths_update );
        for p = 1:numel( pathnames )
            path_id = pathnames{ p };
            if ~isfield( postproc_all_paths, path_id ) || ~isfield( postproc_all_paths.( path_id ), 'first_frame' )
                postproc_all_paths.( path_id ).first_frame = n;
            end
        end
        
        idx_current_back = idx( N + 1 - n );
        load( fullfile( output_folder, file_listing( idx_current_back ).name ), '-mat' );
        
        pathnames = fieldnames( paths_update );
        for p = 1:numel( pathnames )
            path_id = pathnames{ p };
            if ~isfield( postproc_all_paths, path_id ) || ~isfield( postproc_all_paths.( path_id ), 'last_frame' )
                postproc_all_paths.( path_id ).last_frame = N + 1 - n;
            end
        end
        
        fprintf( 'Number of paths modified in post processing: %i\n', numel( fieldnames( postproc_all_paths ) ) )
        
        waitbar( n / N )
        
    end
    
    save( fullfile( output_folder, 'postprocessing_data.mat' ), 'postproc_all_paths' )
    
    close( h )
end


%% Run auralization
if any( strcmpi( ppa_example_mode, { 'auralization', 'all' } ) )
    
    % Load data
    
	load( fullfile( output_folder, 'analysis_data.mat' ) )
    load( fullfile( output_folder, 'postprocessing_data.mat' ), 'postproc_all_paths' )
    
    file_listing = dir( strcat( output_folder, '/frame*data.mat' ) );
    N = numel( file_listing );
    fprintf( 'Found data on %i frames in folder ''%s''\n', N, output_folder )
    
    if sort_by_date
        frame_datenums = zeros( N, 1 );
        for n=1:N
            frame_datenums( n ) = file_listing( n ).datenum;
        end
        [ ~, idx ] = sort( frame_datenums );
    else
        idx = 1:N;
    end
    
    % Set up VA scene
    
    va = VA;
    va.connect;
    
    va.add_search_path( 'D:/Users/andrew/dev/ITASuite/VA/VACore/data' );
    va.add_search_path( 'D:/Users/andrew/dev/ITASuite/VA/VAMatlab/matlab' );
    va.add_search_path( 'C:/dev/VA/VACore/data' );
    va.add_search_path( 'D:/Users/stienen/dev/VA/VACore/data' );
    va.add_search_path( 'C:\ITASoftware\Raven\RavenDatabase\SoundDatabase' );
    va.add_search_path( 'C:\Users\jonas\sciebo\Thesis stuff\auralization\resources' );
    
    c = va.get_homogeneous_medium_sound_speed();
    
    R = va.create_sound_receiver( 'PPA_sensor' );
    H = va.create_directivity_from_file( 'ITA-Kunstkopf_HRIR_AP11_Pressure_Equalized_3x3_256.v17.ir.daff' );
    va.set_sound_receiver_directivity( R, H );
    
    S = va.create_sound_source( 'PPA_emitter' );
    X = va.create_signal_source_buffer_from_file( 'LKW Leerlauf 2.wav' );
    
    va.set_signal_source_buffer_playback_action( X, 'play' )
    va.set_signal_source_buffer_looping( X, true );
    va.set_sound_source_signal_source( S, X )
    
    manual_clock = 0;
    va.set_core_clock( 0 );
    h = waitbar( 0, 'Hold on, running auralisation using generated data' );
    
    % Run auralization update loop
    for n = 1:N
        
        idx_current = idx( n );
        
        % Make source_pos, receiver_pos and paths_update available for this
        % frame
        load( fullfile( output_folder, file_listing( idx_current ).name ), '-mat' );
        
        fn = fieldnames( paths_update );
        for p = 1:numel( fn )
            path_id = fn{ p };
            if n < postproc_all_paths.( path_id ).last_frame && paths_update.( path_id ).delete
                paths_update.( path_id ).delete = false;
                disp( 'Prevented path from deletion because it will be re-appear in a follow up frame' )
            end
        end
        
        % Update all propagation paths
        source_pos_OpenGL = ita_matlab2openGL( source_pos( 1:3 )' );
        receiver_pos_OpenGL = ita_matlab2openGL( receiver_pos( 1:3 )' );
        
        va.lock_update;
        
        % Update receiver (last anchor)
        va.set_sound_source_position( S, source_pos_OpenGL );
        va.set_sound_receiver_position( R, receiver_pos_OpenGL );
        
        if exist( 'single_path_id', 'var' )
            
            % Define a single_path_id if you want to run an auralization
            % for an individual path, only (see first section)
            if isfield( paths_update, single_path_id )
                single_path_update = struct( 'p', paths_update.( single_path_id ) );
                va.set_rendering_module_parameters( 'MyBinauralOutdoorNoise', single_path_update );
            else
                disp( 'Single path not found in current frame, skipping update' )
            end
            
        else
            
            % Update all paths
            va.set_rendering_module_parameters( 'MyBinauralOutdoorNoise', paths_update );
            
        end
        
        va.unlock_update; % triggers scene update for renderers
        
        % Increment core clock
        manual_clock = manual_clock + frame_rate;
        va.call_module( 'manualclock', struct( 'time', manual_clock ) );
        
        % Process audio chain by incrementing one block
        va.call_module( 'virtualaudiodevice', struct( 'trigger', true ) );
        
        waitbar( n / N )
        
    end
    
    close( h )
    
    va.disconnect
    disp( 'Stop VA to export simulation results from rendering module(s)' )
    
end

warning( 'default', 'all' )