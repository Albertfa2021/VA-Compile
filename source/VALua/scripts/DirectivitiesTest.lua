print("\n")
print(" ----- Test von allen Directivities Methoden !  -------")
print("\n")
core = VACore() -- Zgruff auf dem Kern-Instanz
print(core:GetVersionInfo())
print("\n")

direct1 = core:LoadDirectivity("Floete.ddb", "Directivity1")
