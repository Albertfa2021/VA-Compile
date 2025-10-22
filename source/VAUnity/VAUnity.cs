using UnityEngine;
using VA;
using Debug = UnityEngine.Debug;

namespace VAUnity
{
    /// <summary>
    /// Caller for Scriptable Objects, which Starts VA (VAStarter, optional) and Connect to VA (VAAdapter, compulsory)
    /// handles the connection to VA Server, throw VA
    /// Singleton
    /// </summary>
    public class VAUnity : MonoBehaviour
    {
        [Tooltip("optional, Starts VA with a certain Config")]
        [SerializeField] private VAStarter vaStarter;
        
        [Tooltip("compulsory, Connect with VA, can be local or on other server")]
        [SerializeField] private VAAdapter vaAdapter;
        
        [Tooltip("Toggle if all Unity Audio Source should be deactivated.")]
        [SerializeField] private bool deactivateUnitySoundOnStart = true;

        private static VANet _va;

        /// <summary>
        /// reference to the VANet Object (used for all Communication to VA Server)
        /// </summary>
        public static VANet VA
        {
            get
            {
                if (_va != null)
                    return _va;

                Debug.LogWarning("VAUnity is not init yet.");
                return null;
            }
        }

        /// <summary>
        /// Starts VA Server and connect to it.
        /// Check if it is a Singleton
        /// Unity Callback Function, it is called on the beginning (need to be called at first) 
        /// </summary>
        private void Awake()
        {
            StartAndConnectVaServer();
            CheckIfSingleton();
        }
        
        /// <summary>
        /// Unity callback function, Called after Awake
        /// It also adds the Project Path to Unity, if server is on localhost
        /// Deactivates the Unity Audio Sources, if set
        /// </summary>
        void Start()
        {
            AddProjectPathToVa();
            
            if (deactivateUnitySoundOnStart)
                DeactivateUnitySound();
        }

        /// <summary>
        /// Check if it is really a Singleton
        /// </summary>
        private void CheckIfSingleton()
        {
            var vaUnities = Resources.FindObjectsOfTypeAll<VAUnity>();
            if (vaUnities.Length>1)
                Debug.LogError("More then one VAUnity in Scene. Delete all except one.");
        }

        /// <summary>
        /// Add Asset folder as search path for VA (only works if VA is running on same host PC.
        /// </summary>
        private void AddProjectPathToVa()
        {
            if (vaAdapter.IsLocalhost)
                if (!VA.AddSearchPath(Application.dataPath + "/../"))
                    Debug.LogError(
                        "Could not add application assets folder to VA search path, VA server running on remote host?");
        }

        /// <summary>
        /// Starts VA Server and connect to it
        /// </summary>
        private void StartAndConnectVaServer()
        {
            if (vaStarter != null)
                vaStarter.Init();

            _va = vaAdapter.Init();
        }

        /// <summary>
        /// deactivate all Audio Sources in Unity, because we want to use VA
        /// </summary>
        private void DeactivateUnitySound()
        {
            AudioSource[] audioSources = FindObjectsOfType(typeof(AudioSource)) as AudioSource[];
            foreach (AudioSource audioSource in audioSources)
            {
                audioSource.enabled = false;
            }

            AudioListener[] audioListeners = FindObjectsOfType(typeof(AudioListener)) as AudioListener[];
            foreach (AudioListener audioListener in audioListeners)
            {
                audioListener.enabled = false;
            }
        }
    }
}
