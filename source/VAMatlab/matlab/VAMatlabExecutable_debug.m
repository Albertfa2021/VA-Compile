% This debug script is for debugging the VA-to-Matlab connection
% without the use of the facade class "VA"
% To use MSVS for debugging the MEX library, in VS select "Debugging >
% Append to process" ... then select the MATLAB.exe process and start to
% execute commands in Matlab that are calling functions of the MEX file


%% Pathes to required libs & dlls (cumbersome, but necessary)
vam_path = 'D:\Users\stienen\dev\VA\VAMatlab';
build_dir = 'build_win32-x64.vc12';

vanet_dir = 'D:\Users\stienen\dev\VA';
vista_dir = 'D:\Users\stienen\dev\ViSTA';
natnet_libs = 'X:\VR\dev\ExternalLibs\NatNetSDK\NatNetSDK-2.7\lib\x64';

addpath( fullfile( vam_path, build_dir, 'lib' ) ) % VAMatlabD
addpath( fullfile( vanet_dir, build_dir, 'lib' ) ) % VABaseD, VANetD
addpath( fullfile( vista_dir, build_dir, 'lib' ) ) % VistaBaseD, VistaAspectsD, VistaInterProcCommD
addpath( fullfile( natnet_libs ) ) % NatNetSDK (OptiTrack)

copy_dlls = true;
if( copy_dlls ) % Be careful with this option
    dest = fullfile( vam_path, build_dir, 'lib' );
    copyfile( which( 'VABaseD.dll' ), dest )
    copyfile( which( 'VANetD.dll' ), dest )
    copyfile( which( 'VistaBaseD.dll' ), dest )
    copyfile( which( 'VistaAspectsD.dll' ), dest )
    copyfile( which( 'VistaInterProcCommD.dll' ), dest )
    copyfile( which( 'NatNetLib.dll' ), dest )
    warning( 'DLLs have been copied, make sure to use the corresponding versions during debugging.' )
end


%% Connection
conn = VAMatlabD( 'connect', 'localhost:12340' );
renderers = VAMatlabD( 'get_rendering_modules', conn, false );
disp( renderers )
reproductions = VAMatlabD( 'get_reproduction_modules', conn, false );
disp( reproductions )

%% Tests

L = VAMatlabD( 'create_sound_receiver', conn, 'VA_Tracked_Listener', 'default', -1 );

VAMatlabD( 'set_tracked_sound_receiver', conn,L )
%VAMatlabD( 'set_tracked_sound_source', conn,L )
VAMatlabD( 'set_tracked_sound_receiver_head_rigid_body_index', conn, 1 )
VAMatlabD( 'set_tracked_sound_receiver_head_rigid_body_translation', conn, [ 0 0 3 ] )
q = ita_quaternion( [ pi/sqrt(2) 0 1/sqrt(2) 0 ] ) 
q = q.normalize

VAMatlabD( 'SetRigidBodyRotation', conn, q.e(:)' )

VAMatlabD( 'connect_trracker', conn )
b = VAMatlabD( 'get_tracker_connected', conn )
VAMatlabD( 'disconnect_tracker', conn )


%VAMatlabD( 'connect_tracker', conn, '137.226.61.85', '137.226.61.85' )



modinfos = VAMatlabD( 'get_modules', conn );


istruct = struct;
istruct.info = '';
ostruct = VAMatlabD( 'call_mdule', conn, 'PrototypeGenericPath:MyGenericRenderer', istruct )

istruct = struct();
istruct.command = 'UPDATE';
istruct.ListenerID = 1;
istruct.SourceID = 1;

if false 
    istruct.type = 'FilePath';
    istruct.FilePath = '$(VADataDir)/HPEQ/HD600_all_eq_128_stereo.wav';
end

if true
    istruct.type = 'DATA';
    istruct.IRDataCh1 = [ 0.9 zeros( 1, 2048 ) ];
    istruct.IRDataCh2 = [ 0.1 zeros( 1, 2048 ) ];
end

if false
    sweep = ita_time_crop( ita_generate_sweep, [1 1350], 'samples' ) * 0.03;
    istruct.type = class( sweep );
    istruct.itaAudio = ita_merge( sweep, sweep );
end

ostruct = VAMatlabD( 'call_module', conn, 'PrototypeGenericPath:MyGenericRenderer', istruct );

%VAMatlabD( 'call_module', conn, 'hprep', { 'print', 'help' } ) % will print on std::cout @ real VACore, i.e. VAGUI or Console
%lhs_arg = VAMatlabD( 'call_module', conn, 'hprep', { 'get', 'gain' } )
%VAMatlabD( 'call_module', conn, 'hprep', { 'set', 'gain', 'value', lhs_arg / 3 } )
%lhs_arg_new = VAMatlabD( 'call_module', conn, 'hprep', { 'get', 'gain' } )

VAMatlabD( 'disconnect', conn )
clear VAMatlabD % release to allow write access for msvc
