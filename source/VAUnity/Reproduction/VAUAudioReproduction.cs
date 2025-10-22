using UnityEngine;
using VA;

namespace VAUnity
{
	public class VAUAudioReproduction : MonoBehaviour
	{
		[Tooltip("Reproduction module identifier")]
		[SerializeField] protected string ID = "";

		[Tooltip("Mute/unmute reproduction output")]
		public bool OutputMuted = false;

		[Tooltip("Control reproduction output gain")]
		public double OutputGain = 1.0;

		private bool OutputMutedShadow;
		private double OutputGainShadow;

		protected VANet _va = null; 
		
		protected void Start ()
		{
			_va = VAUnity.VA;
			
			_va.SetReproductionModuleMuted( ID, OutputMuted );
			_va.SetReproductionModuleGain( ID, OutputGain );
		
			OutputMutedShadow = OutputMuted;
			OutputGainShadow = OutputGain;
		}
	
		protected void Update()
		{
			if( OutputMuted != OutputMutedShadow )
			{
				_va.SetReproductionModuleMuted( ID, OutputMuted );
				OutputMutedShadow = OutputMuted;
			}
			if( OutputGain != OutputGainShadow )
			{
				_va.SetReproductionModuleGain( ID, OutputGain );
				OutputGainShadow = OutputGain;
			}
		}
	}
}
