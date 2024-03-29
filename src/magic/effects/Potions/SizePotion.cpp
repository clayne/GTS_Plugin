
#include "managers/GtsSizeManager.hpp"
#include "managers/GtsManager.hpp"
#include "magic/effects/Potions/SizePotion.hpp"
#include "magic/effects/common.hpp"
#include "magic/magic.hpp"
#include "scale/scale.hpp"
#include "data/persistent.hpp"
#include "data/runtime.hpp"


namespace Gts {
	std::string SizePotion::GetName() {
		return "SizePotion";
	}


	SizePotion::SizePotion(ActiveEffect* effect) : Magic(effect) {

		auto base_spell = GetBaseEffect();

		if (base_spell == Runtime::GetMagicEffect("EffectSizePotionExtreme")) {
			this->Strenght = 0.35;
		} else if (base_spell == Runtime::GetMagicEffect("EffectSizePotionStrong")) {
			this->Strenght = 0.20;
		} else if (base_spell == Runtime::GetMagicEffect("EffectSizePotionNormal")) {
			this->Strenght = 0.15;
		} else if (base_spell == Runtime::GetMagicEffect("EffectSizePotionWeak")) {
			this->Strenght = 0.10;
		}
		//log::info("Strenght is {}", this->Strenght);
	}


	void SizePotion::OnStart() {
		auto caster = GetCaster();
		if (!caster) {
			return;
		}
		float Gigantism = 1.0 + SizeManager::GetSingleton().GetEnchantmentBonus(caster)/100;
		auto saved_data = Persistent::GetSingleton().GetData(caster);
		if (saved_data) {
			float PotionPower = this->Strenght;
			float BonusSize = Runtime::GetFloat("sizeLimit") * PotionPower * Gigantism; // add % of scale, that's why we read sizeLimit value
			saved_data->bonus_max_size = BonusSize;
		}
	}

	void SizePotion::OnUpdate() {
		auto caster = GetCaster();
		if (!caster) {
			return;
		}
		float PotionPower = this->Strenght;
		float Gigantism = 1.0 + SizeManager::GetSingleton().GetEnchantmentBonus(caster)/100;
		float expected = Runtime::GetFloat("sizeLimit") * PotionPower * Gigantism;
		auto saved_data = Persistent::GetSingleton().GetData(caster);
		if (saved_data) {
			if (saved_data->bonus_max_size < expected) {
				saved_data->bonus_max_size += expected; // Just to be sure
				return;
			}
		}
	}

	void SizePotion::OnFinish() {
		auto caster = GetCaster();
		if (!caster) {
			return;
		}
		auto saved_data = Persistent::GetSingleton().GetData(caster);
		if (saved_data) {
			saved_data->bonus_max_size = 0;
		}
	}
}
