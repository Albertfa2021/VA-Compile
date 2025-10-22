core = VACore()

hprep_gain = core:CallModule( "hprep", "get", "gain" )

if hprep_gain == 1 then
	core:CallModule( "hprep", "set", "gain", "value", 0  )
	core:CallModule( "nctcrep", "set", "gain", "value", 1  )
else
	core:CallModule( "hprep", "set", "gain", "value", 1  )
	core:CallModule( "nctcrep", "set", "gain", "value", 0  )
end