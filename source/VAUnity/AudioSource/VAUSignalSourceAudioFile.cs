using UnityEditor.VersionControl;
using UnityEngine;
using VA;

namespace VAUnity
{
	public class VAUSignalSourceAudioFile : VAUSignalSource
	{
		[Tooltip("Absolute or relative file path (relative to Assets folder or any folder added to search path using AddSearchPath)")]
		public string FilePath;
		
		public string Name; // Versatile name
		[Tooltip("Will loop the audio signal source.")]
		public bool IsLooping = true;  
		[Tooltip("Will immediately start the signal source payback.")]
		public bool PlayOnStart = true;
		[Tooltip("Playback state")]
		public string PlaybackState = "invalid";
		
		private VANet _va = null;
    
		void OnEnable()
		{		
			if (!_va.IsConnected())
			{
				Debug.LogError( "Could not enable signal source '" + FilePath + "', not connected to VA" );
				return;
			}
			_ID = _va.CreateSignalSourceBufferFromFile(FilePath, Name);
			Debug.Assert(_ID.Length > 0, "Could not create audio file signal source '" + Name + "' from file path " + FilePath);
			//Debug.Log("Created audio file signal source with id '" + _ID + "'");
			_va.SetSignalSourceBufferLooping (_ID, IsLooping);
		}

		void Start()
		{
			_va = VAUnity.VA;
			
			if (PlayOnStart)
			{
				_va.SetSignalSourceBufferPlaybackAction(_ID, "PLAY");
			}

			UpdatePlaybackState();
		}

		void OnDestroy()
		{
			if (_ID.Length > 0)
				_va.DeleteSignalSource(_ID);
		}

		void Update()
		{
			UpdatePlaybackState();
			//Debug.Log("Current play state for signal souce " + _ID + ": " + VAUAdapter.VA.GetSignalSourceBufferPlaybackState(_ID));
		}

		private void UpdatePlaybackState()
		{
			if (_ID.Length > 0)
				PlaybackState = _va.GetSignalSourceBufferPlaybackState(_ID);
		}
	}
}
