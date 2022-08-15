#include "magic/effects/shrink_foe.hpp"
#include "magic/effects/common.hpp"
#include "magic/magic.hpp"
#include "scale/scale.hpp"
#include "data/runtime.hpp"

namespace Gts {
	ShrinkFoe::ShrinkFoe(ActiveEffect* effect) : Magic(effect) {
		auto base_spell = GetBaseEffect();
		auto& runtime = Runtime::GetSingleton();

		if (base_spell == runtime.explosiveGrowth1) {
			this->power = 1.0;
			this->efficiency = 0.34;
		} else if (base_spell == runtime.explosiveGrowth2) {
			this->power = 1.12;
			this->efficiency = 0.36;
		} else {
			// ShrinkEnemyAOEMast
			this->power = 1.35;
			this->efficiency = 0.40;
		}
	}

	bool ShrinkFoe::StartEffect(EffectSetting* effect) {
		auto& runtime = Runtime::GetSingleton();
		if (effect == runtime.ShrinkEnemy || effect == runtime.ShrinkEnemyAOE || effect == runtime.ShrinkEnemyAOEMast) {
			return true;
		} else {
			return false;
		}
	}

	void ShrinkFoe::OnUpdate() {
		auto caster = GetCaster();
		
		if (!caster) {
			return;
		}
		auto caster = GetCaster();
		if (!target) {
			return;
		}
		
		bool smallMassiveThreat = caster->HasMagicEffect(Runtime::GetSingleton().smallMassiveThreat);
		if (Runtime::GetSingleton().ProtectEssentials->value == 1.0 && target.IsEssential() == true) {
			return;
		}
		transfer_size(caster, target, IsDualCasting(), this->power, this->efficiency, smallMassiveThreat);
		
		//if (EssentialProtection != 1.0 || target->IsEssential() == false) // Essential Protection
		//{;}
	}
}
