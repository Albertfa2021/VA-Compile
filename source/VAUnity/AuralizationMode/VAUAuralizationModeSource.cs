using UnityEngine;
using VA;

namespace VAUnity
{
    [RequireComponent(typeof(VAUSoundSource))]
    public class VAUAuralizationModeSource : VAUAuralizationMode
    {
        private VAUSoundSource[] _vauSoundSources;
        
        private VANet _va = null; 
        
        private void Awake()
        {
            _vauSoundSources = FindObjectsOfType<VAUSoundSource>();
        }

        private void Start()
        {
            _va = VAUnity.VA;
            
            AuraStringChanged += OnSoundSourceAuralizationModeChanged;

        }
        private void OnDisable()
        {
            AuraStringChanged -= OnSoundSourceAuralizationModeChanged;
        }
        
        private void OnSoundSourceAuralizationModeChanged(string AuraMode)
        {
            
            foreach (var vauSoundSource in _vauSoundSources)
            {
                _va.SetSoundSourceAuralizationMode(vauSoundSource.ID, AuraMode);
            }
        }


    }
}