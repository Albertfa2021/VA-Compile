namespace VRTK
{
    using UnityEngine;
    using System.Collections;
    using VA;

    public class VAUVRTKOnTeleportMute : MonoBehaviour {

        public float mutetime = 0.2F;
        private float mutetimecounter;
        private VANet _VA;
        void Start()
        {
            _VA = VAUAdapter.VA;
            GetComponent<VRTK_BasicTeleport>().Teleporting += VAUVRTKOnTeleportMute_Teleporting;
            GetComponent<VRTK_BasicTeleport>().Teleported += VAUVRTKOnTeleportMute_Teleported;
        }

        private void VAUVRTKOnTeleportMute_Teleported(object sender, DestinationMarkerEventArgs e)
        {
            _VA.SetOutputMuted(false);
        }

        private void VAUVRTKOnTeleportMute_Teleporting(object sender, DestinationMarkerEventArgs e)
        {
           // mutetimecounter = mutetime;
           _VA.SetOutputMuted(true);
        }

        void Update()
        {
            //if (mutetimecounter > 0F)
            //    mutetimecounter -= Time.deltaTime;

            //if (mutetimecounter < 0F)
            //    mutetimecounter = 0F;

            //if (mutetimecounter == 0f)
            //    _VA.SetOutputMuted(false);
        }
    }
}