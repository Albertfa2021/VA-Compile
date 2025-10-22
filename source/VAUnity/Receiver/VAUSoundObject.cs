using UnityEngine;
using VA;

namespace VAUnity
{
    public abstract class VAUSoundObject : MonoBehaviour
    {
        [Tooltip("Internal VA identifier")]
        protected int _ID;
        public int ID
        {
            get
            {
                return _ID;
            }
        }
        
        [Tooltip("Descriptive name")]
        public string Name = "SoundReceiver";
        protected VANet _VA = null;

        protected void SetSoundReceiverPositionOrientation(out VAVec3 vaPosition,out VAVec3 vaOrientationView,
            out VAVec3 vaOrientationUp)
        {
            var t = transform;
            var q = t.rotation;
            var p = t.position;
            var up = q * Vector3.up;
            var view = q * Vector3.forward;
            var viewOgl = new Vector3(view.x, view.y, -view.z);
            var upOgl = new Vector3(up.x, up.y, -up.z);
            vaPosition = new VAVec3(p.x, p.y, -p.z);
            vaOrientationView = new VAVec3(viewOgl.x, viewOgl.y, viewOgl.z);
            vaOrientationUp = new VAVec3(upOgl.x, upOgl.y, upOgl.z);
        }

    }
}