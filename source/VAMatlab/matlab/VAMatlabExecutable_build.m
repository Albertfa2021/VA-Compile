%% Settings

% Paths
deploy_dir = './';
src_dir = '../src';

% Flags
vamatlab_with_local_core = false;
vamatlab_with_optitrack_tracker = false;
preprocessor_flags = '';
if regexpi( system_dependent('getos'), 'win' )
    preprocessor_flags = [ preprocessor_flags '-DWIN32' ];
end

% Includes
vabase_include = '../../VABase/include';
vanet_include = '../../VANet/include';
vista_include = 'C:\dev\ViSTA\VistaCoreLibs';
natnet_include = '../../../ExternalLibs/NatNetSDK/NatNetSDK-2.7/include';

% Lib dirs
dist_dir_name = 'build_win32-x64.vc12';
va_lib_dir = fullfile( '../../', dist_dir_name, 'lib' );
vabase_lib_dir = fullfile( '../../VABase', dist_dir_name, 'lib' );
vanet_lib_dir = fullfile( '../../VANet', dist_dir_name, 'lib' );
vista_lib_dir = fullfile( 'C:\dev\ViSTA\VistaCoreLibs', dist_dir_name, 'lib' );
natnet_lib_dir = fullfile( '../../../ExternalLibs/NatNetSDK/NatNetSDK-2.7', 'lib', 'x64' );
% addpath( va_lib_dir, vabase_include, vanet_lib_dir, vista_lib_dir, natnet_lib_dir )

% Libs
vabase_lib = 'VABase.lib';
vanet_lib = 'VANet.lib';
vista_lib = 'VistaAspects.lib -lVistaBase.lib -lVistaInterProcComm.lib';
natnet_lib = 'NatNetLib.lib';

%% Compiler & project

% Determine the C++ compiler configuration
cxx = mex.getCompilerConfigurations( 'C++', 'Selected' );
if isempty( cxx )
    error( 'No C++ compiler installed or selected for Matlab. Please run ''mex -setup''.' );
end

% Output file
outfile = fullfile( deploy_dir, [ 'VAMatlab.' mexext ] );

% Source files
source_files = { 'MatlabHelpers.cpp', 'VAMatlabConnection.cpp', 'VAMatlabExecutable.cpp', 'VAMatlabTracking.cpp' };
srcs = fullfile( src_dir, source_files );
srcs = sprintf( '%s ', srcs{:} );

%% Build

fprintf( 'Building VA Matlab executable\n\tCompiler: ''%s''\n\tOutput: ''%s''\n', cxx.Name, outfile )

% Create the destination directory if it does not exist
if ~exist( deploy_dir, 'dir' )
    mkdir( deploy_dir );
end

if vamatlab_with_local_core
    preprocessor_flags = [ preprocessor_flags ' -DVAMATLAB_WITH_LOCAL_CORE=1' ];
end
if vamatlab_with_optitrack_tracker
    preprocessor_flags = [ preprocessor_flags ' -DVAMATLAB_WITH_OPTITRACK_TRACKER=1' ];
end

% Note: We need to call mex via 'system' here, because we used the symbol
% 'mex' with '.' above and Matlab would complain when doing 'mex ...'
cmd = sprintf( 'mex -O %s -I%s -I%s -I%s -I%s -L%s -L%s -L%s -L%s -L%s -l%s -l%s -l%s -l%s %s -output %s', ...
    preprocessor_flags, ...
    vabase_include, vanet_include, vista_include, natnet_include, ...
    va_lib_dir, vabase_lib_dir, vanet_lib_dir, vista_lib_dir, natnet_lib_dir, ...
    vabase_lib, vanet_lib, vista_lib, natnet_lib, ...
    srcs, ...
    outfile );

disp( cmd );

[ errorcode, result ] = system( cmd, '-echo' );
if errorcode ~= 0
    error( 'Building VA Matlab executable failed' )
end

fprintf( 'VA Matlab executable successfully built to ''%s''\n', outfile )
