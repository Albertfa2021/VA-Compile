using UnityEngine;
using System.Collections;
using VA;
using UnityEngine.UI;
using System;


public class VAUVRTKReverbTimeText : MonoBehaviour {

    private VANet _VA = null;
    private Text reverbTimeText;
    private VAUListener listener;
    void Start()
    {
        _VA = VAUAdapter.VA;
        reverbTimeText = GetComponent<Text>();
        listener = FindObjectOfType<VAUListener>();
        listener.ReverbTimeChanged += Listener_ReverbTimeChanged;
        reverbTimeText.text = 0f + "s";
    }

    private void Listener_ReverbTimeChanged(double reverbTime)
    {
        reverbTime = Math.Round(reverbTime, 3);
        reverbTimeText.text = reverbTime + "s";
    }

    

    
}
