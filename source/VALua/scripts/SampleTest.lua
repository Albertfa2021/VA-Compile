print("\n")
print(" ----- Test von allen Sound Samples Methoden !  -------")
print("\n")
core = VACore() -- Zgruff auf dem Kern-Instanz
print(core:GetVersionInfo())
print("\n")

sampleID = core:LoadSample("lang_mono.wav", "Test")
core:PrintSampleInfos()


-- function to delete Sample
function delete()

DelID = core:FreeSample( sampleID )

if DelID == true then 
print("Sample is successfully Deleted")
else
print("Sample cannot Deleted ")
end

end