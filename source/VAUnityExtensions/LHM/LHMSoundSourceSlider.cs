using UnityEngine;
using System.Collections;
using UnityEngine.UI;

public class LHMSoundSourceSlider : MonoBehaviour {
    private Slider sl;
    private VAUSoundSource[] SoundSources;
    private VAUSoundSource activeSoundSource;
    private Text Tooltip;

    public VAUSoundSource actSoundSource
    {
        get
        {
            return activeSoundSource;
        }
    }

    public delegate void OnActSoundSourceChangedDelegate(VAUSoundSource actSoundSource);
    public event OnActSoundSourceChangedDelegate ActSoundSourceChanged;

    // Use this for initialization
    void Start () {
        sl = gameObject.GetComponent<Slider>();
        SoundSources = FindObjectsOfType<VAUSoundSource>();
        sl.maxValue = SoundSources.Length - 1;
        if (sl.maxValue == 0)
        {
            GetComponentInParent<Renderer>().enabled = false;
            //return;
        }
        sl.onValueChanged.AddListener(delegate { ChangeSoundSource(); });
        activeSoundSource = SoundSources[(int)sl.value];
        ActSoundSourceChanged(activeSoundSource);
        Tooltip = transform.parent.GetComponentInChildren<Text>();
        Tooltip.text = "SoundSource" + activeSoundSource.ID;
	}
	void ChangeSoundSource()
    {
        activeSoundSource = SoundSources[(int)sl.value];
        ActSoundSourceChanged(activeSoundSource);
        Tooltip.text = "SoundSource" + activeSoundSource.ID;
    }
	
}
