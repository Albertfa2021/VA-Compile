using VA;

namespace VAUnity
{
	public class VAURendererArtificialReverb : VAUAudioRenderer
	{
		public double RoomReverberationTime = 1.2; // s
		public double RoomSurfaceArea = 220.0; // m^2
		public double RoomVolume = 600.0; // m^3
	
		private double RoomReverberationTimeShadow;
		private double RoomSurfaceAreaShadow;
		private double RoomVolumeShadow;
		
		
		protected override void Start ()
		{
			base.Start();

			_va.SetArtificialReverberationTime( ID, RoomReverberationTime );
			_va.SetArtificialSurfaceArea( ID, RoomSurfaceArea );
			_va.SetArtificialRoomVolume( ID, RoomVolume );
		
			RoomReverberationTimeShadow = RoomReverberationTime;
			RoomSurfaceAreaShadow = RoomSurfaceArea;
			RoomVolumeShadow = RoomVolume;
		}
	
		protected override void Update()
		{
			base.Update();
			
			if( RoomReverberationTime != RoomReverberationTimeShadow )
			{
				_va.SetArtificialReverberationTime( ID, RoomReverberationTime );
				RoomReverberationTimeShadow = RoomReverberationTime;
			}
			if( RoomSurfaceArea != RoomSurfaceAreaShadow )
			{
				_va.SetArtificialSurfaceArea( ID, RoomSurfaceArea );
				RoomSurfaceAreaShadow = RoomSurfaceArea;
			}
			if( RoomVolume != RoomVolumeShadow )
			{
				_va.SetArtificialRoomVolume( ID, RoomVolume );
				RoomVolumeShadow = RoomVolume;
			}
		}
	}
}
