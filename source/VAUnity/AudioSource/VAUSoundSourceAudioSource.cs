using System;
using UnityEditor;
using UnityEngine;
using UnityEngine.Serialization;

namespace VAUnity
{
    [RequireComponent(typeof(AudioSource))]
    public class VAUSoundSourceAudioSource : VAUSoundObject
    {
        // [Tooltip("Activate this if you want to validate & change volume and mute/unmute state on every update loop.")]
        [Tooltip("Insert a custom VAUDirectivity-Script.")]
        public VAUDirectivity Directivity = null;

        private string _AudiofileSignalSource = null;
        private AudioSource _audioSource;

        private float _volumeShadow;
        private bool _muteShadow;

        private void Awake()
        {
            _audioSource = GetComponent<AudioSource>();
        }
        
        void Start()
        {
            _VA = VAUnity.VA;

            if (!_VA.IsConnected())
            {
                Debug.LogError("Could not create sound source, not connected to VA");
                return;
            }

            // Add sound source
            _ID = _VA.CreateSoundSource(this.name);

            // Connect to directivity, if linked or attached
            if (Directivity)
                _VA.SetSoundSourceDirectivity(_ID, Directivity.ID);
            else if (GetComponent<VAUDirectivity>())
                _VA.SetSoundSourceDirectivity(_ID, GetComponent<VAUDirectivity>().ID);


            // Set gain control once
            UpdateSoundSource(true);
            
            // Create and connect audio signal source
            if (_audioSource.clip)
            {
                var filePath = _audioSource.clip.name + ".wav";
                var sName = this.name + "_signal";
                var isLooping = _audioSource.loop;
                var playOnAwake = _audioSource.playOnAwake;

                _AudiofileSignalSource = _VA.CreateSignalSourceBufferFromFile(filePath, sName);
                Debug.Assert(_AudiofileSignalSource.Length > 0,
                    "Could not create integrated audio file signal source '" + sName + "' file from path " + filePath);
                _VA.SetSignalSourceBufferLooping(_AudiofileSignalSource, isLooping);
                if (playOnAwake)
                    _VA.SetSignalSourceBufferPlaybackAction(_AudiofileSignalSource, "play");

                _VA.SetSoundSourceSignalSource(_ID, _AudiofileSignalSource);
            }

            // Initially, set the pose (otherwise rendering module can not spatialize)
            SetSoundSourcePositionOrientation();
        }

        // Update is called once per frame
        void Update()
        {
            if (transform.hasChanged)
                SetSoundSourcePositionOrientation();
            transform.hasChanged = false;
            
            UpdateSoundSource();
        }

        private void UpdateSoundSource(bool forceUpdate = false)
        {
            if (_volumeShadow != _audioSource.volume || forceUpdate)
            {
                _VA.SetSoundSourceSoundPower(_ID,
                    _audioSource.volume *
                    31.69e-3); // Volume of 1.0 results in default sound power: 31.67 mW -> 94 dB SPL @ 1m
                _volumeShadow = _audioSource.volume;
            }

            if (_muteShadow != _audioSource.mute || forceUpdate)
            {
                _VA.SetSoundSourceMuted(_ID, _audioSource.mute);
                _muteShadow = _audioSource.mute;
            }
        }

        void SetSoundSourcePositionOrientation()
        {
            SetSoundReceiverPositionOrientation(out var vaPosition, out var vaOrientationView,
                out var vaOrientationUp);
            _VA.SetSoundSourcePosition(_ID, vaPosition);
            _VA.SetSoundSourceOrientationVU(_ID, vaOrientationView, vaOrientationUp);
        }

        public void OnDisable()
        {
            _VA.SetSignalSourceBufferPlaybackAction(_AudiofileSignalSource, "stop");
        }

        private void OnDestroy()
        {
            if (_VA.IsConnected())
            {
                _VA.SetSoundSourceSignalSource(_ID, "");
                _VA.DeleteSoundSource(ID);

                // Temptative signal source deletion
                if (_AudiofileSignalSource != null)
                    _VA.DeleteSignalSource(_AudiofileSignalSource);
            }
        }
    }
}