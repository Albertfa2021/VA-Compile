using UnityEngine;
using System.Collections;

public class LHMTurnAroundMenuController : MonoBehaviour {

    public float sensitivity = 100f;
    private float distance = -0.03f;

    private SteamVR_TrackedController trackedObjeckt;
    private SteamVR_Controller.Device device;
    private Transform pos;
    private Vector3 posoffset;
    private Vector2 actPadPos;
    private Vector2 lastPadPos;
    private float dif;
    private bool istouched;

    void Start()
    {
        trackedObjeckt = GetComponentInParent<SteamVR_TrackedController>();
        trackedObjeckt.PadTouched += Controller_PadTouched;
        trackedObjeckt.PadUntouched += Controller_PadUntouched;
        device = SteamVR_Controller.Input((int)trackedObjeckt.controllerIndex);
        pos = gameObject.transform.parent.FindChild("Model").transform;
        posoffset.Set(0f, distance, 0f);
        gameObject.transform.localPosition = posoffset;
    }

    void LateUpdate()
    {

        if (istouched)
        {
            actPadPos = device.GetAxis();
            dif = lastPadPos.x - actPadPos.x;
            gameObject.transform.RotateAround(pos.position + posoffset, pos.forward, dif * sensitivity);
            gameObject.transform.localPosition = posoffset;
            lastPadPos = actPadPos;
        }
    }

    private void Controller_PadUntouched(object sender, ClickedEventArgs e)
    {
        istouched = false;
    }

    private void Controller_PadTouched(object sender, ClickedEventArgs e)
    {
        lastPadPos = device.GetAxis();
        actPadPos = device.GetAxis();
        istouched = true;
    }
}
