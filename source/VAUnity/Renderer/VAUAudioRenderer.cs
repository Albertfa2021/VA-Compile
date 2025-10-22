
using UnityEngine;
using VA;

namespace VAUnity
{
	public abstract class VAUAudioRenderer : MonoBehaviour
	{
		[Tooltip("Rendering module identifier")]
		[SerializeField] protected string ID = "";

		[Tooltip("Mute/unmute rendering output")]
		public bool OutputMuted = false;

		[Tooltip("Control rendering output gain")]
		public double OutputGain = 1.0;
	
		private bool OutputMutedShadow;
		private double OutputGainShadow;

		protected VANet _va;
		
		protected virtual void Start ()
		{
			_va = VAUnity.VA;
			
			_va.SetRenderingModuleMuted( ID, OutputMuted );
			_va.SetRenderingModuleGain( ID, OutputGain );
		
			OutputMutedShadow = OutputMuted;
			OutputGainShadow = OutputGain;
		}
	
		protected virtual void Update()
		{
			if( OutputMuted != OutputMutedShadow )
			{
				_va.SetRenderingModuleMuted( ID, OutputMuted );
				OutputMutedShadow = OutputMuted;
			}
			if( OutputGain != OutputGainShadow )
			{
				_va.SetRenderingModuleGain( ID, OutputGain );
				OutputGainShadow = OutputGain;
			}
		}
	}
}
