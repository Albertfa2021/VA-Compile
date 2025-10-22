using UnityEngine;
using VA;

namespace VAUnity
{
    public class VAUTextToSpeechSignalSource : VAUSignalSource
    {
        public string Name; // Versatile name

        [Tooltip("Text which should be read out.")]
        public string Text = "hi there, my name is rachel. virtual acoustics is a real-time auralization framework for scientific research in Virtual Reality created by the institute of technical acoustics, RWTH aachen university. thank you for testing the VA unity C sharp scripts for controlling a VA server.";
        public bool DirectPlayback = false;

        private string _TextIdentifier = "";

        private VANet _va = null;

        void Start()
        {
            _va = VAUnity.VA; 
            
            if (!_va.IsConnected())
            {
                Debug.LogError( "Could not create text to speech signal source, not connected to VA" );
                return;
            }
            
            _ID = _va.CreateSignalSourceTextToSpeech(Name);
            
            _va.TextToSpeechPrepareText(_ID, _TextIdentifier, Text);
            
            if (DirectPlayback)
                _va.TextToSpeechPlaySpeech(_ID, _TextIdentifier);
        }

        void OnDestroy()
        {
            if (_ID.Length > 0)
                _va.DeleteSignalSource(_ID);
        }
    }
}
