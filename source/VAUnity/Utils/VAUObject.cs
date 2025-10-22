using UnityEngine;
using VA;

namespace VAUnity
{
    /// <summary>
    /// Abstract class to derive most VA MonoBehaviours from.
    /// </summary>
    /// <typeparam name="T">Type of the object's ID in VA</typeparam>
    public abstract class VAUObject<T>:MonoBehaviour
    {
        protected T ID;
        protected VANet _va;

        protected virtual void Start()
        {
            _va = VAUnity.VA;
        }
    }
}