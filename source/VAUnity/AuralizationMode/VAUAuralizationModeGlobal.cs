namespace VAUnity
{
    public class VAUAuralizationModeGlobal : VAUAuralizationMode
    {
        private void Start()
        {
            AuraStringChanged += OnGlobalAuralizationModeChanged;
        }
        
        private void OnDisable()
        {
            AuraStringChanged -= OnGlobalAuralizationModeChanged;
        }
        
        private void OnGlobalAuralizationModeChanged(string AuraMode)
        {
            VAUnity.VA.SetGlobalAuralizationMode(AuraMode);
        }
    }
}