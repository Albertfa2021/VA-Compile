using UnityEngine;
using VA;

namespace VAUnity
{
	public class VAUHomogeneousMedium : MonoBehaviour
	{
		[Tooltip("Speed of sound [m/s]")]
		public double SpeedOfSound = 341;

		[Tooltip("Relative humidity [percent]")]
		public double RelativeHumidity = 20;

		[Tooltip("Temperature [degree celsius]")]
		public double Temperature = 20;

		[Tooltip("Static pressure [Pascal]")]
		public double StaticPressure = 101125;

		[Tooltip("Shift speed / wind speed [m/s]")]
		public Vector3 ShiftSpeed = new Vector3( 0, 0, 0 );

		[Tooltip("If set to true, the initial values will be received from server and will override the manually set values")]
		public bool OverrideFromServer = false;

		private double SpeedOfSoundShadow;
		private Vector3 ShiftSpeedShadow;
		private double RelativeHumidityShadow;
		private double StaticPressureShadow;
		private double TemperatureShadow;

		private VANet _va = null;

		void Start ()
		{
			_va = VAUnity.VA;
			
			if (OverrideFromServer) {
				SpeedOfSound = _va.GetHomogeneousMediumSoundSpeed();
				VAVec3 _ShiftSpeed = _va.GetHomogeneousMediumShiftSpeed ();
				ShiftSpeed.Set ((float)_ShiftSpeed.x, (float)_ShiftSpeed.y, (float)_ShiftSpeed.z);
				RelativeHumidity = _va.GetHomogeneousMediumRelativeHumidity ();
				StaticPressure = _va.GetHomogeneousMediumStaticPressure ();
				Temperature = _va.GetHomogeneousMediumTemperature ();
			}
			else
			{
				_va.SetHomogeneousMediumSoundSpeed (SpeedOfSound);
				VAVec3 _ShiftSpeed = new VAVec3 (ShiftSpeed.x, ShiftSpeed.y, ShiftSpeed.z);
				_va.SetHomogeneousMediumShiftSpeed (_ShiftSpeed);
				_va.SetHomogeneousMediumRelativeHumidity (RelativeHumidity);
				_va.SetHomogeneousMediumStaticPressure (StaticPressure);
				_va.SetHomogeneousMediumTemperature (Temperature);
			}

			SpeedOfSoundShadow = SpeedOfSound;
			ShiftSpeedShadow = ShiftSpeed;
			RelativeHumidityShadow = RelativeHumidity;
			StaticPressureShadow = StaticPressure;
			TemperatureShadow = Temperature;
		}
	
		void Update()
		{
			if( SpeedOfSound != SpeedOfSoundShadow )
			{
				_va.SetHomogeneousMediumSoundSpeed( SpeedOfSound );	
				SpeedOfSoundShadow = SpeedOfSound;
			}
			if( ShiftSpeed != ShiftSpeedShadow )
			{
				VAVec3 _ShiftSpeed = new VAVec3 (ShiftSpeed.x, ShiftSpeed.y, ShiftSpeed.z);
				_va.SetHomogeneousMediumShiftSpeed( _ShiftSpeed );	
				ShiftSpeedShadow = ShiftSpeed;
			}
			if( RelativeHumidity != RelativeHumidityShadow )
			{
				_va.SetHomogeneousMediumRelativeHumidity( RelativeHumidity );	
				RelativeHumidityShadow = RelativeHumidity;
			}
			if( StaticPressure != StaticPressureShadow )
			{
				_va.SetHomogeneousMediumStaticPressure( StaticPressure );	
				StaticPressureShadow = StaticPressure;
			}
			if( Temperature != TemperatureShadow )
			{
				_va.SetHomogeneousMediumTemperature( Temperature );	
				TemperatureShadow = Temperature;
			}
		}
	}
}
