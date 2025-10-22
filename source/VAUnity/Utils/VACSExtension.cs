using UnityEngine;

namespace VA
{
    public partial class VAVec3
    {
        /// <summary>
        /// Converts Unity's Vector3 in Unity coordinates to a VAVec3 in OpenGL coordinates 
        /// </summary>
        /// <param name="vec"></param>
        public VAVec3(Vector3 vec)
        {
            x = vec.x;
            y = vec.y;
            z = -vec.z;
        }
    }
    
    public partial class VAQuat
    {
        /// <summary>
        /// Converts Unity's Quaternion in Unity coordinates to a VAQuat in OpenGL mcoordinates
        /// </summary>
        /// <param name="quat"></param>
        public VAQuat(Quaternion quat)
        {
            x = quat.x;
            y = quat.y;
            z = -quat.z;
            w = quat.w;
        }
    }
}