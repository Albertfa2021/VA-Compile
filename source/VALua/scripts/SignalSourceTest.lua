print("\n")
print(" ----- Test von allen Listener Methoden !  -------")
print("\n")
core = VACore() -- Zgruff auf dem Kern-Instanz
print(core:GetVersionInfo())
print("\n")

signalSourceID = core:CreateAudiofileSignalSource( "lang_mono.wav", "Audio" )
print("Signal Source ID : " .. signalSourceID)

SampleID = core:CreateSamplerSignalSource( "Sample1" )
print("Sampler Signal Source ID : " .. signalSourceID)

deviceInputID = core:CreateDeviceInputSignalSource( 1, "Input1" )
print("Device Input Source ID : " .. deviceInputID)

-- function to delete Signal Source :
function delete()
Del = core:DeleteSignalSource( signalSourceID )

if Del == true then
print("Signal Source is successfully deleted")
else
print("Signal Source cannot deleted")
end

end