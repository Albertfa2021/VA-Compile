using System;
using UnityEngine;

namespace VAUnity
{
    public abstract class VAUAuralizationMode : MonoBehaviour
    {
        [Tooltip("Toggle Direct sound path between a sound source and a sound receiver")]
        [SerializeField] private bool DirectSound = true;

        [Tooltip("Toggle Specular reflections off walls, that correspond to early arrival time of a complex source-receiver-pair.")]
        [SerializeField] private bool EarlyReflections = false;

        [Tooltip("Toggle Diffuse decay part of a the arrival time of a complex source-receiver-pair. Mostly used in the context of room acoustics.")]
        [SerializeField] private bool DiffuseDecay = false;

        [Tooltip("Toggle Sound source directivity function, the angle dependent radiation pattern of an emitter.")]
        [SerializeField] private bool SourceDirectivity = true;

        [Tooltip("Toggle Acoustic energy attenuation due to absorbing capability of the medium")]
        [SerializeField] private bool MediumAbsorption = false;

        [Tooltip("Toggle Statistics-driven fluctuation of sound resulting from turbulence and time-variance of the medium (the atmosphere).")]
        [SerializeField] private bool TemporalVariation = false;

        [Tooltip("Toggle Diffuse scattering off non-planar surfaces.")]
        [SerializeField] private bool Scattering = false;

        [Tooltip("Toggle Diffraction off and around obstacles.")]
        [SerializeField] private bool Diffraction = false;

        [Tooltip("Toggle Acoustic phenomena caused by near field effects (in contrast to far field assumptions).")]
        [SerializeField] private bool NearFieldEffects = false;

        [Tooltip("Toggle Doppler frequency shifts based on relative distance changes.")]
        [SerializeField] private bool DopplerShifts = true;

        [Tooltip("Toggle Distance dependent spreading loss, i.e. for spherical waves. Also called 1/r-law or (inverse) distance law.")]
        [SerializeField] private bool SphericalSpreadingLoss = true;
        
        [Tooltip("Toggle Transmission of sound energy through solid structures like walls and flanking paths.")]
        [SerializeField] private bool Transmission = true;
        
        [Tooltip("Toggle Sound absorption by material.")]
        [SerializeField] private bool Absorption = true;


        protected class AuralizationMode : System.Object
        {
            private bool _directSound = true;
            private bool _earlyReflections = true;
            private bool _diffuseDecay = true;
            private bool _sourceDirectivity = true;
            private bool _mediumAbsorption = true;
            private bool _temporalVariation = true;
            private bool _scattering = true;
            private bool _diffraction = true;
            private bool _nearFieldEffects = true;
            private bool _dopplerShifts = true;
            private bool _sphericalSpreadingLoss = true;
            private bool _transmission = true;
            private bool _absorption = true;
            
            private bool _forceUpdate = true;
            
            public void UpdateAuraModeSettings(VAUAuralizationMode am, bool forceUpdate = false)
            {
                _forceUpdate = _forceUpdate || forceUpdate;
                    if (_directSound != am.DirectSound || _forceUpdate)
                {
                    _directSound = am.DirectSound;
                    am.AuraStringChanged?.Invoke(am.DirectSound ? "+DS" : "-DS");
                }

                if (_earlyReflections != am.EarlyReflections || _forceUpdate)
                {
                    _earlyReflections = am.EarlyReflections;
                    am.AuraStringChanged?.Invoke(am.EarlyReflections ? "+ER" : "-ER");
                }

                if (am.DiffuseDecay != _diffuseDecay || _forceUpdate)
                {
                    _diffuseDecay = am.DiffuseDecay;
                    am.AuraStringChanged?.Invoke(_diffuseDecay ? "+DD" : "-DD");
                }

                if (am.SourceDirectivity != _sourceDirectivity || _forceUpdate)
                {
                    _sourceDirectivity = am.SourceDirectivity;
                    am.AuraStringChanged?.Invoke(_sourceDirectivity ? "+SD" : "-SD");
                }

                if (am.MediumAbsorption != _mediumAbsorption || _forceUpdate)
                {
                    _mediumAbsorption = am.MediumAbsorption;
                    am.AuraStringChanged?.Invoke(am.MediumAbsorption ? "+MA" : "-MA");
                }

                if (am.TemporalVariation != _temporalVariation || _forceUpdate)
                {
                    _temporalVariation = am.TemporalVariation;
                    am.AuraStringChanged?.Invoke(_temporalVariation ? "+TV" : "-TV");
                }

                if (am.Scattering != _scattering || _forceUpdate)
                {
                    _scattering = am.Scattering;
                    am.AuraStringChanged?.Invoke(am.Scattering ? "+SC" : "-SC");
                }

                if (am.Diffraction != _diffraction || _forceUpdate)
                {
                    _diffraction = am.Diffraction;
                    am.AuraStringChanged?.Invoke(_diffraction ? "+DF" : "-DF");
                }

                if (am.NearFieldEffects != _nearFieldEffects || _forceUpdate)
                {
                    _nearFieldEffects = am.NearFieldEffects;
                    am.AuraStringChanged?.Invoke(am.NearFieldEffects ? "+NF" : "-NF");
                }

                if (am.DopplerShifts != _dopplerShifts || _forceUpdate)
                {
                    _dopplerShifts = am.DopplerShifts;
                    am.AuraStringChanged?.Invoke(_dopplerShifts ? "+DP" : "-DP");
                }

                if (am.SphericalSpreadingLoss != _sphericalSpreadingLoss || _forceUpdate)
                {
                    _sphericalSpreadingLoss = am.SphericalSpreadingLoss;
                    am.AuraStringChanged?.Invoke(_sphericalSpreadingLoss ? "+SL" : "-SL");
                }
                
                if (am.Transmission != _transmission || _forceUpdate)
                {
                    _transmission = am.Transmission;
                    am.AuraStringChanged?.Invoke(_transmission ? "+TR" : "-TR");
                }
                
                if (am.Absorption != _absorption || _forceUpdate)
                {
                    _absorption = am.Absorption;
                    am.AuraStringChanged?.Invoke(_absorption ? "+AB" : "-AB");
                }

                _forceUpdate = false;
            }

            public override bool Equals(System.Object other)
            {
                return (GetHashCode() == other.GetHashCode());
            }

            public override int GetHashCode()
            {
                var iCompare = 0;

                iCompare += _directSound ? 1 << 1 : 0;
                iCompare += _earlyReflections ? 1 << 2 : 0;
                iCompare += _diffuseDecay ? 1 << 3 : 0;
                iCompare += _sourceDirectivity ? 1 << 4 : 0;
                iCompare += _mediumAbsorption ? 1 << 5 : 0;
                iCompare += _temporalVariation ? 1 << 6 : 0;
                iCompare += _scattering ? 1 << 7 : 0;
                iCompare += _diffraction ? 1 << 8 : 0;
                iCompare += _nearFieldEffects ? 1 << 9 : 0;
                iCompare += _dopplerShifts ? 1 << 10 : 0;
                iCompare += _sphericalSpreadingLoss ? 1 << 11 : 0;
                iCompare += _transmission ? 1 << 12 : 0;
                iCompare += _absorption ? 1 << 13 : 0;

                return iCompare;
            }

            public static bool operator ==(AuralizationMode left, AuralizationMode right)
            {
                return left.Equals(right);
            }

            public static bool operator !=(AuralizationMode left, AuralizationMode right)
            {
                return !(left.Equals(right));
            }
        }

        private AuralizationMode _auraMode = new AuralizationMode();

        public delegate void OnAuraStringChangedDelegate(string sAuraString);
        public event OnAuraStringChangedDelegate AuraStringChanged;


        void Update()
        {
            _auraMode.UpdateAuraModeSettings(this);
        }
    }
}
