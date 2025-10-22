using UnityEngine;
using VA;

namespace VAUnity
{
	public class VAUSoundReceiverTracking : MonoBehaviour
	{    
		[Tooltip("Set the corresponding receiver that should be updated")]
		public VAUSoundReceiver Receiver = null;

		[Tooltip("Updates position of user")]
		public bool PositionTracking = true;

		[Tooltip("Updates orientation of user")]
		public bool OrientationTracking = true;

		private VANet _va;
		private Transform t;

		void Awake()
		{
			t = transform;
		}

		void Start()
		{
			_va = VAUnity.VA;
			
			if( Receiver && Receiver.ID > 0 )
				SetSoundReceiverRealWorldHeadPose();
		}

		void Update()
		{
			if( Receiver && Receiver.ID > 0 )
				SetSoundReceiverRealWorldHeadPose();
		}

		protected void SetSoundReceiverRealWorldHeadPose()
		{
			Vector3 p = t.position; // Unity coordinate system
			VAVec3 v3PosOpenGL = new VAVec3(p.x, p.y, -p.z); // OpenGL coordinate system

			Quaternion q = t.rotation; // Unity coordinate system
			VAQuat qOrientOpenGL = new VAQuat( q.x, q.y, -q.z, q.w ); // OpenGL coordinate system

			if( !PositionTracking )
			{
				v3PosOpenGL = _va.GetSoundReceiverRealWorldPosition (Receiver.ID);
			}
			if( !OrientationTracking )
			{
				qOrientOpenGL = _va.GetSoundReceiverRealWorldOrientation (Receiver.ID);
			}

			_va.SetSoundReceiverRealWorldHeadPose (Receiver.ID, v3PosOpenGL, qOrientOpenGL);
		}
	}
}
