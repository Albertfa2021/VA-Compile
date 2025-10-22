using UnityEngine;
using VA;

namespace VAUnity
{
    public class VAUListener : VAUSoundReceiver {
    
        [Tooltip("Number of reverbzones used for determining the current reverb.")]
        public int NumMaxReverbZones = 2;

        public string VAAudioRenderer = "MyBinauralArtificialReverb";
    
        [Tooltip("Anthropometric head width, for HRIR individualization or generic binaural cues")]
        public double HeadWidth = 0.12;
        [Tooltip("Anthropometric head height, for HRIR individualization or generic binaural cues")]
        public double HeadHeight = 0.10; 
        [Tooltip("Anthropometric head depth, for HRIR individualization or generic binaural cues")]
        public double HeadDepth = 0.15;

        private double ShadowHeadWidth;
        private double ShadowHeadHeight;
        private double ShadowHeadDepth;
    
        private AudioReverbZone[] reverbZones;
        private float shadowReverbTime;

        public delegate void ReverbTimeChangedDelegate(double reverbTime);
        public event ReverbTimeChangedDelegate ReverbTimeChanged;

        private VANet _va = null;
        
        void Start()
        {
            _va = VAUnity.VA;
            
            _ID = _va.CreateSoundReceiver(Name);
            _va.SetSoundReceiverAuralizationMode(_ID, "all");
            if (Directivity)
                _va.SetSoundReceiverDirectivity(_ID, Directivity.ID);

            _va.SetSoundReceiverAnthropometricData(_ID, HeadWidth, HeadHeight, HeadDepth);
            ShadowHeadWidth = HeadWidth;
            ShadowHeadHeight = HeadHeight;
            ShadowHeadDepth = HeadDepth;

            UpdateSoundReceiverPositionOrientation();

            _va.SetArtificialReverberationTime(VAAudioRenderer, 0.3f);
            reverbZones = FindObjectsOfType(typeof(AudioReverbZone)) as AudioReverbZone[];
        }

  

        void Update ()
        {
            UpdateSoundReceiverPositionOrientation();
            SetActiveReverbZones();

            if( HeadWidth != ShadowHeadWidth || HeadHeight != ShadowHeadHeight || HeadDepth != ShadowHeadDepth )
            {
                _va.SetSoundReceiverAnthropometricData(_ID, HeadWidth, HeadHeight, HeadDepth);
                ShadowHeadWidth = HeadWidth;
                ShadowHeadHeight = HeadHeight;
                ShadowHeadDepth = HeadDepth;
            }
        }

        void SetActiveReverbZones()
        {
            if (reverbZones == null)
                return;

            float actReverbTime = 0f;
            int i = 0;
            foreach (AudioReverbZone reverbZone in reverbZones)
            {
                if (i >= NumMaxReverbZones)
                    break;
                float actDistance = Vector3.Distance(reverbZone.transform.position, gameObject.transform.position);
                if ((actDistance < reverbZone.maxDistance))
                {
                    if (actDistance > reverbZone.minDistance)
                    {
                        actReverbTime += reverbZone.decayTime * (reverbZone.maxDistance - actDistance) / (reverbZone.maxDistance - reverbZone.minDistance);
                    }
                    else
                    {
                        actReverbTime += reverbZone.decayTime;
                    }
                    i++;
                }
            }
            if (shadowReverbTime == actReverbTime)
                return;


            if (actReverbTime < 0.3f)
                _va.SetRenderingModuleMuted("MyBinauralArtificialReverb", true);
            else
                _va.SetRenderingModuleMuted("MyBinauralArtificialReverb", false);

            shadowReverbTime = actReverbTime;

            if (i > 0)
                actReverbTime /= i;

            if (ReverbTimeChanged != null)
                ReverbTimeChanged(actReverbTime);

            _va.SetArtificialReverberationTime(VAAudioRenderer, actReverbTime);

        }
    }
}
