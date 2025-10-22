using UnityEngine;

namespace VAUnity
{
    public class VAUAdditionalSearchPath : MonoBehaviour
    {
        public string Path = "";

        void Start ()
        {
            VAUnity.VA.AddSearchPath( Path );
        }
    }
}
