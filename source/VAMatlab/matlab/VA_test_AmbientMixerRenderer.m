%% VA connect
va = VA
va.connect


%% Ambient mixer renderer
rends = va.get_rendering_modules();
rend_id =  rends( 1 ).id;
rend_params = va.get_rendering_module_parameters( rend_id, struct() )

assert( rend_params.SamplerEnabled )
assert( rend_params.Sampler.NumChannels >= 4 )


%% Ambient sampler update
s = struct();
s.NewTrack = 4; % 4 channel track
s.SampleFilePath = 'ITA_FrontYard_BFormat.wav';
va.set_rendering_module_parameters( rend_id, struct( 'sampler', s ) )
