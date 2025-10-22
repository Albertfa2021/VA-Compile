using System;
using UnityEngine;
using VA;

namespace VAUnity
{
	public class VAUSoundSource : VAUSoundObject
	{
		[Tooltip("Activate this if you want to validate & change volume and mute/unmute state on every update loop.")]
		public bool ContinuousGainObserver = false; 

		[Tooltip("Activate this if you dont want to pause the playback while the SoundSource is disabled but rather stop it.")]
		public bool stopOnDisable = false;

		[Tooltip("Insert a custom VAUDirectivity-Script.")] 
		public VAUDirectivity Directivity = null;

		[Tooltip("Insert a custom VAUSignalSource-Script.")]
		public VAUSignalSource SignalSource = null;

		[Tooltip("Set an explicit renderer for this source [todo]")]
		public VAUAudioRenderer Renderer = null;


		[Tooltip("Sound power in Watts (default is 31mW)")]
		public double SoundPower = 0.0031;

		private VANet _va = null;
		private string _AudiofileSignalSource = null;

	

		void Start()
		{
			_va = VAUnity.VA;

			if (!_va.IsConnected())
			{
				Debug.LogError( "Could not create sound source, not connected to VA" );
				return;
			}

			// Add sound source
			_ID = _va.CreateSoundSource(this.name);
			_va.SetSoundSourceAuralizationMode(_ID, "all");
			_va.SetSoundSourceSoundPower (_ID, SoundPower);
			// Connect to directivity, if linked or attached
			if (Directivity)
				_va.SetSoundSourceDirectivity(_ID, Directivity.ID);
			else if (GetComponent<VAUDirectivity>())
				_va.SetSoundSourceDirectivity(_ID, GetComponent<VAUDirectivity>().ID);

			// Connect to signal source, if linked or attached
			if (SignalSource)
			{
				_va.SetSoundSourceSignalSource(_ID, SignalSource.ID);
			}
			else
			{
				Debug.LogError("Signal Source not found", this);
			}

			// Initially, set the pose (otherwise rendering module can not spatialize)
			SetSoundSourcePositionOrientation();
		}

		// Update is called once per frame
		void Update()
		{
			SetSoundSourcePositionOrientation();

			if (ContinuousGainObserver)
			{
				_va.SetSoundSourceMuted(_ID, GetComponent<AudioSource>().mute);
				_va.SetSoundSourceSoundPower(_ID, GetComponent<AudioSource>().volume * 31.69e-3 ); // Volume of 1.0 results in default sound power: 31.67 mW -> 94 dB SPL @ 1m
			}
		}

		void SetSoundSourcePositionOrientation()
		{
			
			SetSoundReceiverPositionOrientation(out var vaPosition, out var vaOrientationView,
				out var vaOrientationUp);
			VAUnity.VA.SetSoundSourcePosition(_ID, vaPosition);
			VAUnity.VA.SetSoundSourceOrientationVU(_ID, vaOrientationView, vaOrientationUp);
		}

	

		public void OnDisable()
		{

			if (_AudiofileSignalSource != null) {
				if (stopOnDisable)
					_va.SetSignalSourceBufferPlaybackAction (_AudiofileSignalSource, "stop");
				else
					_va.SetSignalSourceBufferPlaybackAction (_AudiofileSignalSource, "pause");
			}
		}

		private void OnDestroy()
		{
			//_VA = VAUnity.VA;
			if (_va.IsConnected())
			{
				_va.SetSoundSourceSignalSource(_ID, "");
				_va.DeleteSoundSource(ID);

				// Temptative signal source deletion
				if (_AudiofileSignalSource != null)
					_va.DeleteSignalSource (_AudiofileSignalSource);
			}
		}


		// Starts Playback
		public void Play()
		{
			_va.SetSignalSourceBufferPlaybackAction(SignalSource.ID, "play");
		}

		// Stopps Playback
		public void Stop()
		{
			_va.SetSignalSourceBufferPlaybackAction(SignalSource.ID, "stop");
		}

		// Mutes Sound Source
		public void SetMuted(bool bMute)
		{
			_va.SetSoundSourceMuted(_ID, bMute);
		}

		// Returns Playback state as string (PLAYING or STOPPED)
		public string GetPlaybackState()
		{
			return _va.GetSignalSourceBufferPlaybackState(SignalSource.ID);
		}

		public string GetSignalSource()
		{
			return _va.GetSoundSourceSignalSource(Int32.Parse(SignalSource.ID));
		}

		public void SetSignalSource(VAUSignalSource newSignalSource)
		{
			if (!newSignalSource || newSignalSource == SignalSource) return;
			SignalSource = newSignalSource;
			_va.SetSoundSourceSignalSource(_ID, SignalSource.ID);
		}
	}
}
