using System;
using UnityEngine;
using VA;

namespace VAUnity
{
    public class VAUDirectivity : MonoBehaviour
    {
        [Tooltip("Absolute or relative path (relative to Assets folder), also use AddSearchPath for special folders.")]
        public string FilePath = "";

        [Tooltip("Versatile name for this directivity")]
        public string Name = "";

        protected int _ID = -1; // Stores the internal id that is assigned by VA

        private VANet _va = null;
        
        public int ID
        {
            get
            {
                return _ID;
            }
        }

        private void Start()
        {
            _va = VAUnity.VA;
        }


        void OnEnable()
        {
            if (!_va.IsConnected())
            {
                Debug.LogError( "Could not enable directivity, not connected to VA" );
                return;
            }

            if (FilePath.Length > 0)
                _ID = _va.CreateDirectivityFromFile(FilePath);
        }

        private void OnDestroy()
        {
            if (!_va.IsConnected())
                return;

            if (_ID != -1)
                _va.DeleteDirectivity(_ID);
            _ID = -1;
        }
    }
}
