#include "managers/GtsSizeManager.hpp"
#include "managers/GtsManager.hpp"
#include "managers/Attributes.hpp"
#include "managers/highheel.hpp"
#include "scale/scale.hpp"
#include "data/runtime.hpp"
#include "data/persistent.hpp"
#include "data/transient.hpp"
#include "magic/effects/common.hpp"
#include "timer.hpp"

using namespace SKSE;
using namespace RE;
using namespace REL;
using namespace Gts;

// TODO move away from polling

namespace {
	void SetINIFloat(std::string_view name, float value) {
		auto ini_conf = GameSettingCollection::GetSingleton();
		std::string s_name(name);
		Setting* setting = ini_conf->GetSetting(s_name.c_str());
		if (setting) {
			setting->data.f=value; // If float
			ini_conf->WriteSetting(setting);
		}
	}

	bool HasGrowthPerk(Actor* actor) {
		if (!Runtime::HasPerk(actor, "GrowthOfStrength")) {
			return false;
		}
		if (Runtime::HasMagicEffect(actor, "explosiveGrowth1")||Runtime::HasMagicEffect(actor, "explosiveGrowth2")||Runtime::HasMagicEffect(actor, "explosiveGrowth3")) {
			return true;
		}
		return false;
	}

	void ManagePerkBonuses(Actor* actor) {
		auto player = PlayerCharacter::GetSingleton();
		auto& SizeManager = SizeManager::GetSingleton();
		float BalancedMode = SizeManager::GetSingleton().BalancedMode();
		float gigantism = 1.0 + SizeManager::GetSingleton().GetEnchantmentBonus(actor)/100;

		float BaseGlobalDamage = SizeManager::GetSingleton().GetSizeAttribute(actor, 0);
		float BaseSprintDamage = SizeManager::GetSingleton().GetSizeAttribute(actor, 1);
		float BaseFallDamage = SizeManager::GetSingleton().GetSizeAttribute(actor, 2);

		float ExpectedGlobalDamage = 1.0;
		float ExpectedSprintDamage = 1.0;
		float ExpectedFallDamage = 1.0;
		float HighHeels = 1.0 + (HighHeelManager::GetSingleton().GetBaseHHOffset(actor).Length()/100);

		//log::info("High Heels Length: {}", HighHeels);
		///Normal Damage
		if (Runtime::HasPerk(actor, "Cruelty")) {
			ExpectedGlobalDamage += 0.35/BalancedMode;
		}
		if (Runtime::HasPerk(actor, "RealCruelty")) {
			ExpectedGlobalDamage += 0.65/BalancedMode;
		}
		if (HasGrowthPerk(actor)) {
			ExpectedGlobalDamage *= (1.0 + (0.35/BalancedMode));
		}

		ExpectedGlobalDamage *= HighHeels; // Apply Base HH damage.

		///Sprint Damage
		if (Runtime::HasPerk(actor, "SprintDamageMult1")) {
			ExpectedSprintDamage += 0.25/BalancedMode;
		}
		if (Runtime::HasPerk(actor, "SprintDamageMult2")) {
			ExpectedSprintDamage += 1.0/BalancedMode;
		}
		///Fall Damage
		if (Runtime::HasPerk(actor, "MightyLegs")) {
			ExpectedFallDamage += 0.5/BalancedMode;
		}
		///Buff by enchantment
		ExpectedGlobalDamage *= gigantism;
		ExpectedSprintDamage *= gigantism;
		ExpectedFallDamage *= gigantism;

		if (BaseGlobalDamage != ExpectedGlobalDamage) {
			SizeManager.SetSizeAttribute(actor, ExpectedGlobalDamage, 0);
			log::info("SizeManager Normal Actor {} value: {}, expected Value: {}", actor->GetDisplayFullName(), SizeManager.GetSizeAttribute(actor, 0), ExpectedGlobalDamage);
			//log::info("Setting Global Damage: {}, gigantism: {}", ExpectedGlobalDamage, gigantism);
		}
		if (BaseSprintDamage != ExpectedSprintDamage) {
			SizeManager.SetSizeAttribute(actor, ExpectedSprintDamage, 1);
			log::info("SizeManager Sprint Actor {} value: {}, expected Value: {}", actor->GetDisplayFullName(), SizeManager.GetSizeAttribute(actor, 1), ExpectedSprintDamage);
			//log::info("Setting Sprint Damage: {}, gigantism: {}", ExpectedSprintDamage, gigantism);
		}
		if (BaseFallDamage != ExpectedFallDamage) {
			SizeManager.SetSizeAttribute(actor, ExpectedFallDamage, 2);
			log::info("SizeManager Fall Actor {} value: {}, expected Value: {}", actor->GetDisplayFullName(), SizeManager.GetSizeAttribute(actor, 2), ExpectedFallDamage);
			//log::info("Setting Fall Damage: {}, gigantism: {}", ExpectedFallDamage, gigantism);
		}
	}

	void BoostCarry(Actor* actor, float power) {
		auto actor_data = Persistent::GetSingleton().GetData(actor);
		if (!actor_data) {
			return;
		}
		float last_carry_boost = actor_data->bonus_carry;
		const ActorValue av = ActorValue::kCarryWeight;
		float max_stamina = actor->GetPermanentActorValue(ActorValue::kStamina);
		float visual_scale = get_visual_scale(actor);
		float native_scale = get_natural_scale(actor);
		float scale = visual_scale;//native_scale;
		float base_av = actor->GetBaseActorValue(av);

		float boost = 0.0;
		if (scale > 1.0) {
			boost = (base_av) * ((scale-1.0) * power);
		} else {
			// Linearly decrease carry weight
			//   at scale=0.0 we adjust by -base_av
			boost = base_av * (scale-1.0);
		};
		actor->RestoreActorValue(ACTOR_VALUE_MODIFIER::kTemporary, av, boost - last_carry_boost);
		actor_data->bonus_carry = boost; 
	}

	void BoostJump(Actor* actor, float power) {
		float scale = get_visual_scale(actor);

		if (fabs(power) > 1e-5) { // != 0.0
			SetINIFloat("fJumpHeightMin", 76.0 + (76.0 * (scale - 1) * power));
			SetINIFloat("fJumpFallHeightMin", 600.0 + ( 600.0 * (scale - 1) * power));
		} else {
			SetINIFloat("fJumpHeightMin", 76.0);
			SetINIFloat("fJumpFallHeightMin", 600.0 + ((-scale + 1.0) * 300 * power));
		}
	}

	void BoostAttackDmg(Actor* actor, float power) {
		float scale = get_visual_scale(actor);
		float bonus = scale * power;
		if (actor->formID == 0x14) {
			if (Runtime::HasMagicEffect(actor, "SmallMassiveThreat")) {
				bonus *= 3.0;
			}
		}
		actor->SetBaseActorValue(ActorValue::kAttackDamageMult, bonus);
	}

	void BoostSpeedMulti(Actor* actor, float power) {
		float scale = get_visual_scale(actor);
		auto actor_data = Transient::GetSingleton().GetData(actor);
		float SMTBonus = Persistent::GetSingleton().GetData(actor)->smt_run_speed/2.5;
		float base_speed = actor_data->base_walkspeedmult;
		float bonusSpeedMax = Runtime::GetFloat("bonusSpeedMax");
		float speedEffectiveSize = (bonusSpeedMax / (100 * power)) + 1.0;
		if (speedEffectiveSize > scale) {
			speedEffectiveSize = scale;
		}

		static Timer timer = Timer(0.15); // Run every 0.5s or as soon as we can
		if (timer.ShouldRunFrame()) {
			if (scale > 1) {
				actor->SetActorValue(ActorValue::kSpeedMult, 100 + ((speedEffectiveSize - 1) * (100 * power)));
			} else if (scale < 1) {
				actor->SetActorValue(ActorValue::kSpeedMult, 100 * (scale * 0.90 +0.10));
			} else {
				actor->SetActorValue(ActorValue::kSpeedMult, 100);
			}
		}
	}

	void BoostHP(Actor* actor) {
		auto actor_data = Persistent::GetSingleton().GetData(actor);
		if (!actor_data) {
			return;
		}
		/*float last_hp_boost = actor_data->bonus_hp;
		const ActorValue av = ActorValue::kHealth;
		float visual_scale = get_visual_scale(actor);
		float native_scale = get_natural_scale(actor);
		float scale = visual_scale;///native_scale;

		float base_av = actor->GetBaseActorValue(av);
		float current_tempav = actor->healthModifiers.modifiers[ACTOR_VALUE_MODIFIERS::kTemporary];

		float boost;
		if (scale > 1.0) {
			boost = base_av * (scale - 1.0) * power;
		} else {
			// Linearly decrease such that:
			//   boost = -base_av when scale==0.0
			//   This way we shouldn't kill them by scaling them
			//   to zero
			boost = base_av * (scale - 1.0);
		}*/

		float current_health_percentage = GetHealthPercentage(actor);

		//actor->healthModifiers.modifiers[ACTOR_VALUE_MODIFIERS::kTemporary] = current_tempav - last_hp_boost + boost;

		//actor_data->bonus_hp = boost;

		SetHealthPercentage(actor, current_health_percentage);
		// Fill up the new healthbar
	}

	void Augmentation(Actor* Player, bool& BlockMessage) {
		auto ActorAttributes = Persistent::GetSingleton().GetData(Player);
		float Gigantism = 1.0 + SizeManager::GetSingleton().GetEnchantmentBonus(Player)/100;
		if (Player->IsSprinting() && Runtime::HasPerk(Player, "NoSpeedLoss") && Runtime::HasMagicEffect(Player, "SmallMassiveThreat")) {
			ActorAttributes->smt_run_speed += 0.003800 * Gigantism;
			if (ActorAttributes->smt_run_speed < 1.0) {
				BlockMessage = false;
			}
		} else if (Player->IsSprinting() && Runtime::HasMagicEffect(Player, "SmallMassiveThreat")) {
			ActorAttributes->smt_run_speed += 0.002600 * Gigantism;
			if (ActorAttributes->smt_run_speed < 1.0) {
				BlockMessage = false;
			}
		} else {
			if (ActorAttributes->smt_run_speed > 0.0) {
				ActorAttributes->smt_run_speed -= 0.045000;
			} else if (ActorAttributes->smt_run_speed <= 0.0) {
				ActorAttributes->smt_run_speed -= 0.0;
				BlockMessage = false;
			} else if (ActorAttributes->smt_run_speed > 1.0) {
				ActorAttributes->smt_run_speed = 1.0;
			} else if (ActorAttributes->smt_run_speed < 1.0) {
				BlockMessage = false;
			} else {
				ActorAttributes->smt_run_speed = 0.0;
				BlockMessage = false;
			}
		}
		if (ActorAttributes->smt_run_speed >= 1.0 && !BlockMessage) {
			BlockMessage = true; // Avoid spamming it
			DebugNotification("You're fast enough to instantly crush someone", 0, true);
		}
		//log::info("SMT Bonus: {}", ActorAttributes->smt_run_speed);
	}

	void UpdatePlayer(Actor* Player, bool& BlockMessage) {
		// Reapply Player Only

		if (!Player) {
			return;
		}
		if (!Player->Is3DLoaded()) {
			return;
		}
		auto& sizemanager = SizeManager::GetSingleton();

		float BalancedMode = SizeManager::GetSingleton().BalancedMode();

		float bonusHPMultiplier = Runtime::GetFloat("bonusHPMultiplier");
		float bonusCarryWeightMultiplier = Runtime::GetFloat("bonusCarryWeightMultiplier");
		float bonusJumpHeightMultiplier = Runtime::GetFloat("bonusJumpHeightMultiplier");
		float bonusDamageMultiplier = Runtime::GetFloat("bonusDamageMultiplier");
		float bonusSpeedMultiplier = Runtime::GetFloat("bonusSpeedMultiplier");

		float size = get_visual_scale(Player);

		static Timer timer = Timer(0.05);

		ManagePerkBonuses(Player);

		if (size > 0) {

			if (!Runtime::GetBool("AllowTimeChange")) {
				BoostSpeedMulti(Player, bonusSpeedMultiplier);
			}
			if (timer.ShouldRunFrame()) {
				Augmentation(Player, BlockMessage);

				BoostJump(Player, bonusJumpHeightMultiplier);

				if (!Runtime::HasPerk(Player, "StaggerImmunity") && size > 1.33) {
					Runtime::AddPerk(Player, "StaggerImmunity");
					return;
				} else if (size < 1.33 && Runtime::HasPerk(Player, "StaggerImmunity")) {
					Runtime::RemovePerk(Player, "StaggerImmunity");
				}
			}
		}
	}

	// Todo unify the functions
	void UpdateNPC(Actor* npc) {
		if (!npc) {
			return;
		}
		if (npc->formID == 0x14) {
			return;
		}
		if (!npc->Is3DLoaded()) {
			return;
		}
		static Timer timer = Timer(0.05);
		float size = get_visual_scale(npc);

		if (timer.ShouldRunFrame()) {
			if (npc->IsPlayerTeammate() || Runtime::InFaction(npc, "FollowerFaction")) {
				//BoostHP(npc, 1.0);
				//BoostCarry(npc, 1.0);
				ManagePerkBonuses(npc);
			}
			if (!Runtime::HasPerk(npc, "StaggerImmunity") && size > 1.33) {
				Runtime::AddPerk(npc, "StaggerImmunity");
			} else if (size < 1.33 && Runtime::HasPerk(npc, "StaggerImmunity")) {
				Runtime::RemovePerk(npc, "StaggerImmunity");
			}
			//BoostAttackDmg(npc, 1.0);
		}
	}

}


namespace Gts {
	AttributeManager& AttributeManager::GetSingleton() noexcept {
		static AttributeManager instance;
		return instance;
	}

	std::string AttributeManager::DebugName() {
		return "AttributeManager";
	}

	void AttributeManager::Update() {
		for (auto actor: find_actors()) {
			if (actor->formID == 0x14) {
				UpdatePlayer(actor, this->BlockMessage);
			} else {
				UpdateNPC(actor);
			}
		}
	}

	void AttributeManager::OverrideSMTBonus(float Value) {
		auto ActorAttributes = Persistent::GetSingleton().GetActorData(PlayerCharacter::GetSingleton());
		ActorAttributes->smt_run_speed = Value;
	}

	float AttributeManager::GetAttributeBonus(Actor* actor, float Value) {
		if (!actor) {
			return 1.0;
		}
		float bonusCarryWeightMultiplier = Runtime::GetFloat("bonusCarryWeightMultiplier");
		float bonusHPMultiplier = Runtime::GetFloat("bonusHPMultiplier");
		float bonusDamageMultiplier = Runtime::GetFloat("bonusDamageMultiplier");
		if (!bonusCarryWeightMultiplier || !bonusHPMultiplier || !bonusDamageMultiplier) {
			return 1.0;
		}
		if (!Persistent::GetSingleton().GetActorData(actor)) {
			return 1.0;
		}
		auto transient = Transient::GetSingleton().GetActorData(actor);
		if (!transient) {
			return 1.0;
		}
	
		float Bonus = Persistent::GetSingleton().GetActorData(actor)->smt_run_speed;
		float BalancedMode = SizeManager::GetSingleton().BalancedMode();
		float scale = get_visual_scale(actor);

		if (Value == 1.0) {   // boost hp
			//BoostHP(actor);
			return scale + ((bonusHPMultiplier/BalancedMode) * scale - 1.0);
		} if (Value == 2.0) { // boost Carry Weight
			return scale + ((bonusCarryWeightMultiplier/BalancedMode) * scale - 1.0);
		} if (Value == 3.0) { // Boost SpeedMult
			float speedmult = soft_core(scale, this->getspeed); 
			float PerkSpeed = 1.0;
			if (Runtime::HasPerk(actor, "BonusSpeedPerk")) {
				PerkSpeed = clamp(0.80, 1.0, speedmult);
			}
				transient->speedmult_storage = 1.0 * (Bonus/2.2 + 1.0)/speedmult/speedmult/speedmult/speedmult/speedmult/PerkSpeed;
			if (actor->formID == 0x14) {
				log::info("SpeedMult: {}", transient->speedmult_storage);
			}
			return transient->speedmult_storage; 
		} if (Value == 4.0) { // Boost Attack Damage
				transient->damage_storage = scale + ((bonusDamageMultiplier) * scale - 1.0);
			return transient->damage_storage;
		}
		return 1.0;
	} 
}
