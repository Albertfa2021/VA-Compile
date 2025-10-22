using UnityEngine;
using VA;

namespace VAUnity
{
    [RequireComponent(typeof(VAUSoundReceiver))]
    public class VAUAuralizationModeReceiver : VAUAuralizationMode
    {
        private VAUSoundReceiver[] _vauSoundReceivers;

        private VANet _va = null; 
        
        private void Awake()
        {
            _vauSoundReceivers = GetComponents<VAUSoundReceiver>();
        }
        private void Start()
        {
            _va = VAUnity.VA;
            
            AuraStringChanged += OnSoundReceiverAuralizationModeChanged;
        }
        
        private void OnDisable()
        {
            AuraStringChanged -= OnSoundReceiverAuralizationModeChanged;
        }
        
        private void OnSoundReceiverAuralizationModeChanged(string AuraMode)
        {
            foreach (var vauSoundReceiver in _vauSoundReceivers)
            {
                _va.SetSoundReceiverAuralizationMode(vauSoundReceiver.ID, AuraMode);
            }
        }
    }
}