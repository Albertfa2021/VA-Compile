using UnityEngine;

namespace VAUnity
{
	public class VAUAudioRendererGenericPath : VAUAudioRenderer
	{

		[Tooltip("Generic path renderer channel number")]
		public int NumberOfChannels = 2;

		[Tooltip("Generic path renderer channel number")]
		public int FilterLengthSamples = 88200;

		[Tooltip("Generic path renderer sampling rate")]
		public double SamplingRate = 44100;

		[Tooltip("Generic path renderer default impules response file")]
		public string IRFilePath = "";

		[Tooltip("Generic path sound source")]
		public VAUSoundSource SoundSource;

		[Tooltip("Generic path sound receiver")]
		public VAUSoundReceiver SoundReceiver;

		private bool UpdatePerformed = false;

		protected override void Start ()
		{
			base.Start();
		}
	
		protected override void Update()
		{
			base.Update();
			
			if (SoundSource && SoundReceiver && !UpdatePerformed) {
				_va.UpdateGenericPathFromFile (ID, SoundSource.ID, SoundReceiver.ID, IRFilePath);
				if( SoundSource.ID > 0 && SoundReceiver.ID > 0 )
					UpdatePerformed = true;
			}
		}
	}
}
