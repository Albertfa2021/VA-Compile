using UnityEngine;
using VA;

namespace VAUnity
{
	public class VAUVolumeController : MonoBehaviour
	{
		[Tooltip("Mute global input (audio in)")]
		public bool InputMuted = false;

		[Tooltip("Control global input gain (audio in level)")]
		public double InputGain = 1.0;

		[Tooltip("Mute global output (audio out)")]
		public bool OutputMuted = false;

		[Tooltip("Control global output gain (audio out level)")]
		public double OutputGain = 1.0;
	
		private bool InputMutedShadow;
		private double InputGainShadow;
		private bool OutputMutedShadow;
		private double OutputGainShadow;

		private VANet _va = null;

		void Start ()
		{
			_va = VAUnity.VA;
			
			_va.SetInputMuted( InputMuted );
			_va.SetInputGain( InputGain );
			_va.SetOutputMuted( OutputMuted );
			_va.SetOutputGain( OutputGain );
		
			InputMutedShadow = InputMuted;
			InputGainShadow = InputGain;
			OutputMutedShadow = OutputMuted;
			OutputGainShadow = OutputGain;
		}
	
		void Update()
		{
			if( InputMuted != InputMutedShadow )
			{
				_va.SetInputMuted( InputMuted );	
				InputMutedShadow = InputMuted;
			}
			if( InputGain != InputGainShadow )
			{
				_va.SetInputGain( InputGain );	
				InputGainShadow = InputGain;
			}
			if( OutputMuted != OutputMutedShadow )
			{
				_va.SetOutputMuted( OutputMuted );	
				OutputMutedShadow = OutputMuted;
			}
			if( OutputGain != OutputGainShadow )
			{
				_va.SetOutputGain( OutputGain );	
				OutputGainShadow = OutputGain;
			}
		}
	}
}
