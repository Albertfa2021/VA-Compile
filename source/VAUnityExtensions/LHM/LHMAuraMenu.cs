using UnityEngine;
using System.Collections;
using VA;
using UnityEngine.UI;

public class LHMAuraMenu : MonoBehaviour {

    public Color setcolor;
    public Color notsetcolor;
    private VAUAuralizationMode auralizationMode;
    private string auraMode;
    private bool DS;
    private bool ER;
    private bool DD;
    private bool DIR;
    private bool AA;
    private bool TV;
    private bool SC;
    private bool DIF;
    private bool NF;
    private bool DP;
    private bool SL;

    void Start()
    {
        auralizationMode = FindObjectOfType<VAUAdapter>().gameObject.GetComponent<VAUAuralizationMode>();
        if (auralizationMode == null)
            Debug.Log("Global AuralizationMode can't be set: No VAUAuralizationMode on VA-Object.");
        auralizationMode.AuraStringChanged += AuralizationMode_AuraStringChanged;   
        RefreshAuraMode();
    }

    private void AuralizationMode_AuraStringChanged(string sAuraString)
    {
        RefreshAuraMode();
    }

    void RefreshAuraMode()
    {
        DS = auralizationMode.DirectSound;
        ER = auralizationMode.EarlyReflections;
        DD = auralizationMode.DiffuseDecay;
        DIR = auralizationMode.SourceDirectivity;
        AA = auralizationMode.AirAbsorption;
        TV = auralizationMode.AtmosphericTemporalVariations;
        SC = auralizationMode.Scattering;
        DIF = auralizationMode.Diffraction;
        NF = auralizationMode.NearFielEffects;
        DP = auralizationMode.DopplerShifts;
        SL = auralizationMode.SphericalSpreadingLoss;

        gameObject.transform.FindChild("DSButton").GetComponent<Image>().color = (DS) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("ERButton").GetComponent<Image>().color = (ER) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("DDButton").GetComponent<Image>().color = (DD) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("DIRButton").GetComponent<Image>().color = (DIR) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("AAButton").GetComponent<Image>().color = (AA) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("TVButton").GetComponent<Image>().color = (TV) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("SCButton").GetComponent<Image>().color = (SC) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("DIFButton").GetComponent<Image>().color = (DIF) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("NFButton").GetComponent<Image>().color = (NF) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("DPButton").GetComponent<Image>().color = (DP) ? setcolor : notsetcolor;
        gameObject.transform.FindChild("SLButton").GetComponent<Image>().color = (SL) ? setcolor : notsetcolor;
    }

    public void AllClicked()
    {
        auralizationMode.TriggerAuraStringChanged("all");
    }
    public void DefaultClicked()
    {
        auralizationMode.TriggerAuraStringChanged("default");
    }
    public void DSClicked()
    {
        if (DS)
        {
            auralizationMode.TriggerAuraStringChanged("-DS");
            DS = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+DS");
            DS = true;
        }
        gameObject.transform.Find("DSButton").GetComponent<Image>().color = (DS) ? setcolor : notsetcolor;
    }
    public void DDClicked()
    {
        if (DD)
        {
            auralizationMode.TriggerAuraStringChanged("-DD");
            DD = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+DD");
            DD = true;
        }
        gameObject.transform.Find("DDButton").GetComponent<Image>().color = (DD) ? setcolor : notsetcolor;
        
    }
    public void DIRClicked()
    {
        if (DIR)
        {
            auralizationMode.TriggerAuraStringChanged("-DIR");
            DIR = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+DIR");
            DIR = true;
        }
        gameObject.transform.Find("DIRButton").GetComponent<Image>().color = (DIR) ? setcolor : notsetcolor;
        
    }
    public void AAClicked()
    {
        if (AA)
        {
            auralizationMode.TriggerAuraStringChanged("-AA");
            AA = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+AA");
            AA = true;
        }
        gameObject.transform.Find("AAButton").GetComponent<Image>().color = (AA) ? setcolor : notsetcolor;
        
    }
    public void TVClicked()
    {
        if (TV)
        {
            auralizationMode.TriggerAuraStringChanged("-TV");
            TV = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+TV");
            TV = true;
        }
        gameObject.transform.Find("TVButton").GetComponent<Image>().color = (TV) ? setcolor : notsetcolor;
        
    }
    public void SCClicked()
    {
        if (SC)
        {
            auralizationMode.TriggerAuraStringChanged("-SC");
            SC = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+SC");
            SC = true;
        }
        gameObject.transform.Find("SCButton").GetComponent<Image>().color = (SC) ? setcolor : notsetcolor;
        
    }
    public void DIFClicked()
    {
        if (DIF)
        {
            auralizationMode.TriggerAuraStringChanged("-DIF");
            DIF = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+DIF");
            DIF = true;
        }
        gameObject.transform.Find("DIFButton").GetComponent<Image>().color = (DIF) ? setcolor : notsetcolor;
        
    }
    public void NFClicked()
    {
        if (NF)
        {
            auralizationMode.TriggerAuraStringChanged("-NF");
            NF = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+NF");
            NF = true;
        }
        gameObject.transform.Find("NFButton").GetComponent<Image>().color = (NF) ? setcolor : notsetcolor;
        
    }
    public void DPClicked()
    {
        if (DP)
        {
            auralizationMode.TriggerAuraStringChanged("-DP");
            DP = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+DP");
            DP = true;
        }
        gameObject.transform.Find("DPButton").GetComponent<Image>().color = (DP) ? setcolor : notsetcolor;
        
    }
    public void SLClicked()
    {
        if (SL)
        {
            auralizationMode.TriggerAuraStringChanged("-SL");
            SL = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+SL");
            SL = true;
        }
        gameObject.transform.Find("SLButton").GetComponent<Image>().color = (SL) ? setcolor : notsetcolor;
    }
    public void ERClicked()
    {
        if (ER)
        {
            auralizationMode.TriggerAuraStringChanged("-ER");
            ER = false;
        }
        else
        {
            auralizationMode.TriggerAuraStringChanged("+ER");
            ER = true;
        }
        gameObject.transform.Find("ERButton").GetComponent<Image>().color = (ER) ? setcolor : notsetcolor;
    }

}
