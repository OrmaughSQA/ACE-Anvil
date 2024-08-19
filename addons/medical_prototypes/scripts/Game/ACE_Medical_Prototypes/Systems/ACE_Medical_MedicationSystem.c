//------------------------------------------------------------------------------------------------
//! Updates to vitals are mostly server side right now.
//! Clients can request values for vitals via ACE_Medical_NetworkComponent
//! TO DO: Inherit from ACE_Medical_BaseSystem once systems support inheritance
class ACE_Medical_MedicationSystem : ACE_Medical_BaseSystem2
{
	protected ACE_Medical_MedicationSystemSettings m_Settings;

	//------------------------------------------------------------------------------------------------
	override protected void OnInit()
	{
		super.OnInit();
		
		ACE_Medical_Settings settings = ACE_SettingsHelperT<ACE_Medical_Settings>.GetModSettings();
		if (settings && settings.m_MedicationSystem)
			m_Settings = settings.m_MedicationSystem;
	}
	
	//------------------------------------------------------------------------------------------------
	override void Update(IEntity entity, float timeSlice)
	{
		super.Update(entity, timeSlice);
		
		ACE_Medical_MedicationComponent component = ACE_Medical_MedicationComponent.Cast(entity.FindComponent(ACE_Medical_MedicationComponent));
		if (!component)
			return;
		
		ACE_Medical_MedicationAdjustments adjustments;
		
		array<ACE_Medical_EDrugType> drugs;
		array<ref array<ref ACE_Medical_Dose>> doses;
		component.GetMedications(drugs, doses);
		map<ACE_Medical_EDrugType, float> concentrations = ComputeConcentrations(drugs, doses);
		
		// Unregister once no drug is left to manage
		if (drugs.IsEmpty())
		{
			Unregister(entity);
			return;
		}
		
		ApplyEffects(entity, concentrations);
	}
	
	//------------------------------------------------------------------------------------------------
	map<ACE_Medical_EDrugType, float> ComputeConcentrations(array<ACE_Medical_EDrugType> drugs, inout array<ref array<ref ACE_Medical_Dose>> allDoses)
	{
		map<ACE_Medical_EDrugType, float> concentrations = new map<ACE_Medical_EDrugType, float>();
		
		for (int i = drugs.Count() - 1; i >= 0; i--)
		{
			ACE_Medical_EDrugType drug = drugs[i];
			array<ref ACE_Medical_Dose> doses = allDoses[i];
			
			float concentration = ComputeConcentration(drug, doses);
			if (concentration > 0)
				concentrations[drug] = concentration;
			
			// Remove drugs that have no doses left
			if (doses.IsEmpty())
			{
				drugs.RemoveOrdered(i);
				allDoses.RemoveOrdered(i);
			}
		}
		
		return concentrations;
	}
	
	//------------------------------------------------------------------------------------------------
	float ComputeConcentration(ACE_Medical_EDrugType drug, inout array<ref ACE_Medical_Dose> doses)
	{
		ACE_Medical_PharmacokineticsConfig config = m_Settings.GetPharmacokineticsConfig(drug);
		float totalConcentration = 0;
		
		// TO DO: Handle IV infusions
		for (int i = doses.Count() - 1; i >= 0; i--)
		{
			ACE_Medical_Dose dose = doses[i];
			float time = dose.GetElapsedTime();
			float concentration = config.m_fActivationRateConstant / (config.m_fActivationRateConstant - config.m_fDeactivationRateConstant);
			concentration *= ACE_Math.Exp(-config.m_fDeactivationRateConstant * time) - ACE_Math.Exp(-config.m_fActivationRateConstant * time);
			concentration *= dose.GetConcentration();
			concentration += ACE_Medical_PharmacokineticsConfig.CONCENTRATION_OFFSET_NM;
			
			// Remove doses that have expired
			if (concentration <= 0)
			{
				doses.RemoveOrdered(i);
				continue;
			}
			
			totalConcentration += concentration;
		}
		
		return totalConcentration;
	}
	
	//------------------------------------------------------------------------------------------------
	void ApplyEffects(IEntity target, map<ACE_Medical_EDrugType, float> concentrations)
	{
		foreach (ACE_Medical_DrugEffectConfig config : m_Settings.m_aPharmacodynamicsConfigs)
		{
			config.ApplyEffect(target, concentrations);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Clear body of all drugs
	override void OnFullHeal(IEntity entity)
	{
		super.OnFullHeal(entity);
		
		ACE_Medical_MedicationComponent component = ACE_Medical_MedicationComponent.Cast(entity.FindComponent(ACE_Medical_MedicationComponent));
		if (component)
			component.Clear();
		
		Unregister(entity);
	}
}
