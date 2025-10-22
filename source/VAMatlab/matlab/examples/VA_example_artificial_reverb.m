VA_example_simple

rend_mods = va.get_rendering_modules();
rend_mod_id = '';
for n = 1:numel( va.get_rendering_modules )
    rend_mod = rend_mods( n );
    if strcmpi( rend_mod.class, 'BinauralArtificialReverb' )
        rend_mod_id = rend_mod.id;
        break
    end
end
if isempty( rend_mod_id )
    error 'Could not find an artificial reverb renderer in the list of active renderers'
end

va.set_global_auralization_mode( '-ds' )
va.get_rendering_module_parameters('MyBinauralArtificialReverb', struct() )

reverbs = struct();
pause( 1 )

%%
reverbs.room_reverberation_times = [ 8 4 2 ];
va.set_rendering_module_parameters( 'MyBinauralArtificialReverb', reverbs )

%%
reverbs.room_reverberation_times = 2; % -> factor of sqrt(2) 1 sqrt( 1/2 ) for low mid high bands
va.set_rendering_module_parameters( 'MyBinauralArtificialReverb', reverbs )
va.get_rendering_module_parameters('MyBinauralArtificialReverb', struct() ).room_reverberation_times

%%
reverbs.room_reverberation_times = 1; % -> factor of sqrt(2) 1 sqrt( 1/2 ) for low mid high bands
va.set_rendering_module_parameters( 'MyBinauralArtificialReverb', reverbs )
va.get_rendering_module_parameters('MyBinauralArtificialReverb', struct() ).room_reverberation_times

%% 
reverbs.room_reverberation_times = sqrt(1/2); % -> factor of sqrt(2) 1 sqrt( 1/2 ) for low mid high bands
va.set_rendering_module_parameters( 'MyBinauralArtificialReverb', reverbs )
va.get_rendering_module_parameters('MyBinauralArtificialReverb', struct() ).room_reverberation_times
