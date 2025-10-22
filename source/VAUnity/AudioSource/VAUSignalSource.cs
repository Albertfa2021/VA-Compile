using UnityEngine;

namespace VAUnity
{
    public abstract class VAUSignalSource : MonoBehaviour {

        protected string _ID = ""; // Identifier string that is either assigned by VA for a managed (dynamic) signal source, or selected string for an unmanaged signal source (like live input)

        public string ID
        {
            get
            {
                return _ID;
            }
        }
    }
}
