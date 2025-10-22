using UnityEngine;
using System.Collections;
using VRTK;
using VA;
public class VAUVRTKUpdateAuraMode : MonoBehaviour {

    private VAUAuralizationMode auralizationMode = new VAUAuralizationMode();
    private string AuraMode;
    public void UpdateGridLayoutValue(int selectedIndex)
    {
        AuraMode = "";
        auralizationMode = FindObjectOfType<VAUAdapter>().gameObject.GetComponent<VAUAuralizationMode>();
        if (auralizationMode == null)
            Debug.Log("Global AuralizationMode can't be set: No VAUAuralizationMode on VA-Object.");
        AuraMode = (selectedIndex == 0) ? "all" : "";
        AuraMode = (selectedIndex == 1) ? "default" : "";
        AuraMode = (selectedIndex == 2) ? "+DS" : "-DS";
        AuraMode = (selectedIndex == 3) ? "+ER" : "-ER";
        AuraMode = (selectedIndex == 4) ? "+DD" : "-DD";
        AuraMode = (selectedIndex == 5) ? "+DIR" : "-DIR";
        AuraMode = (selectedIndex == 6) ? "+AA" : "-AA";
        AuraMode = (selectedIndex == 7) ? "+TV" : "-TV";
        AuraMode = (selectedIndex == 8) ? "+SC" : "-SC";
        AuraMode = (selectedIndex == 9) ? "+DIF" : "-DIF";
        AuraMode = (selectedIndex == 10) ? "+NF" : "-NF";
        AuraMode = (selectedIndex == 11) ? "+DP" : "-DP";
        AuraMode = (selectedIndex == 12) ? "+SL" : "-SL";
        auralizationMode.TriggerAuraStringChanged(AuraMode);
    }
}
