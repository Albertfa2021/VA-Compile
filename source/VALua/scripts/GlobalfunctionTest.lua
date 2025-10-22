print("\n")
print(" ----- Test von Conversion functions !  -------")
print("\n")
core = VACore() -- Zgruff auf dem Kern-Instanz
print(core:GetVersionInfo())
print("\n")

core:SetInputGain( 1, 4.8 )
GainIn = core:GetInputGain( 1 )
print("Input Gain = " ..GainIn )

core:SetOutputGain( 3.6 )
GainOut = core:GetOutputGain()
print("Output Gain : " ..GainOut)