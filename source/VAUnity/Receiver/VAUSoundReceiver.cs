using UnityEngine;
using VA;

namespace VAUnity
{
    public class VAUSoundReceiver : VAUSoundObject {

    
        [Tooltip("Set an explicit renderer for this receiver [todo]")]
        public VAUAudioRenderer Renderer = null;

        [Tooltip("Connect an directivity to this receiver")]
        public VAUDirectivity Directivity = null;

        private VANet _va = null;
    
        void Start()
        {
            _va = VAUnity.VA;
            
            _ID = _va.CreateSoundReceiver(Name);
            _va.SetSoundReceiverAuralizationMode(_ID, "all");
            if (Directivity)
                _va.SetSoundReceiverDirectivity(_ID, Directivity.ID);
        
            UpdateSoundReceiverPositionOrientation();
        }

  

        void Update()
        {
            if (transform.hasChanged)
                UpdateSoundReceiverPositionOrientation();
        }

        // Uses the View- and Up-Vector to transmit the position of the listener to VA


        protected void UpdateSoundReceiverPositionOrientation()
        {
            SetSoundReceiverPositionOrientation(out var vaPosition, out var vaOrientationView,
                out var vaOrientationUp);
            _va.SetSoundReceiverPosition(_ID, vaPosition);
            _va.SetSoundReceiverOrientationVU(_ID, vaOrientationView, vaOrientationUp);
        }

        private void OnDestroy()
        {
            _va.DeleteSoundReceiver(ID);
        }
    }
}
