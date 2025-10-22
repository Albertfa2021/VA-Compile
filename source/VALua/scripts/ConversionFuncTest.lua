print("\n")
print(" ----- Test von Conversion functions !  -------")
print("\n")
core = VACore() -- Zgruff auf dem Kern-Instanz
print(core:GetVersionInfo())
print("\n")

AuraMode = core:GetAuralizationModeStr( 5 )
print("Auralization Mode : " .. AuraMode)

Volume = core:GetVolumeStrDecibel( 2.8 )
print("Volume : " .. Volume)