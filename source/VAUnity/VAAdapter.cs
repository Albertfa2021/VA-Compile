using UnityEngine;
using VA;

namespace VAUnity
{
    /// <summary>
    /// establish and close the connection from Unity to VA, get called by VAUnity
    /// </summary>
    [CreateAssetMenu(fileName = "VAAdapter", menuName = "VAUnity/VAAdapter", order = 1)]
    public class VAAdapter : ScriptableObject
    {
        [Tooltip("Ip address of the VA Server, default is localhost.")]
        [SerializeField] private string server = "localhost";
        
        [Tooltip("Port of the VA Server, default is 12340.")]
        [SerializeField] private int port = 12340;
        
        [Tooltip("Toggle Reset Server on Start")]
        [SerializeField] private bool resetOnStart = true;
        
        [Tooltip("Toggle Reset Server on Stop")]
        [SerializeField] private bool resetOnStop = true;

        public bool IsLocalhost => server == "localhost" || server == "127.0.0.1";

        private VANet _va;

        /// <summary>
        /// Is called in the Scene by VAUnity
        /// </summary>
        public VANet Init()
        {
            // if (isInit) return;
            
            _va = new VANet();
            if (!_va.Connect(server, port))
            {
                Debug.LogError("Could not connect to VA server on " + server + " using port " + port);
                return _va;
            }
            
            if( resetOnStart )
                _va.Reset ();

            return _va;
            // isInit = true;
        }

        /// <summary>
        /// reset and disconnect server 
        /// </summary>
        public void OnDestroy()
        {
            if( resetOnStop )
                _va.Reset ();
            _va.Disconnect();
            // isInit = false;
        }
    }
}