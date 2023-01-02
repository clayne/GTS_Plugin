#include "managers/RandomGrowth.hpp"
#include "managers/GrowthTremorManager.hpp"
#include "managers/GtsSizeManager.hpp"
#include "managers/GtsManager.hpp"
#include "magic/effects/common.hpp"
#include "util.hpp"
#include "scale/scale.hpp"
#include "data/persistent.hpp"
#include "data/runtime.hpp"
#include "data/time.hpp"
#include "timer.hpp"

using namespace RE;
using namespace Gts;

namespace {
	bool ShouldGrow() {
		auto Player = PlayerCharacter::GetSingleton();
		float MultiplySlider = Runtime::GetFloat("RandomGrowthMultiplyPC");
		if (!Runtime::HasPerk(Player, "GrowthPerk") || MultiplySlider == 0) {
			return false;
		}
		int RandomizeGrowth = rand()% 50;
		this->Randomize = RandomizeGrowth;
		
		if (SizeManager::GetSingleton().BalancedMode() == 2.0) {
			MultiplySlider = 1.0; // Disable effect in Balance Mode, so it's always 1.0
			//log::info("Balance Mode True");
		}
		float Gigantism = 1.0 - SizeManager::GetSingleton().GetEnchantmentBonus(Player)/100;
		int Requirement = ((250 * MultiplySlider) * Gigantism) * SizeManager::GetSingleton().BalancedMode();
		int random = rand() % Requirement;
		int decide_chance = 1;
		//log::info("Requirement: {}", Requirement);
		if (random <= decide_chance) {
			return true;
		} else {
			return false;
		}
	}

	void RestoreStats() {
		auto Player = PlayerCharacter::GetSingleton();
		if (Runtime::HasPerk(Player, "GrowthAugmentation")) {
			float HP = Player->GetPermanentActorValue(ActorValue::kHealth) * 0.00085;
			float MP = Player->GetPermanentActorValue(ActorValue::kMagicka) * 0.00085;
			float SP = Player->GetPermanentActorValue(ActorValue::kStamina) * 0.00085;
			Player->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, HP * TimeScale());
			Player->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kMagicka, SP * TimeScale());
			Player->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kStamina, MP * TimeScale());
		}
	}
}

namespace Gts {
	RandomGrowth& RandomGrowth::GetSingleton() noexcept {
		static RandomGrowth instance;
		return instance;
	}
	void RandomGrowth::CallShake(float value) {
		this->CallInputGrowth = true;
		this->ShakePower = value;
		//log::info("ShakePower is {}", this->ShakePower);
	}

	std::string RandomGrowth::DebugName() {
		return "RandomGrowth";
	}

	void RandomGrowth::Update() {
		auto player = PlayerCharacter::GetSingleton();

		if (!player) {
			return;
		}
		if (!player->Is3DLoaded()) {
			return;
		}

		bool hasSMT = Runtime::HasMagicEffect(player, "SmallMassiveThreat");

		if (this->CallInputGrowth == true) {

			auto& Persist = Persistent::GetSingleton();
			auto actor_data = Persist.GetData(player);
			float delta_time = Time::WorldTimeDelta();

			this->growth_time_input += delta_time;
			actor_data->half_life = 1.0 + this->ShakePower/6;

			GrowthTremorManager::GetSingleton().CallRumble(player, player, this->ShakePower * 15);

			static Timer timer = Timer(0.33);
			if (timer.ShouldRunFrame() && this->ShakePower > 6.0) {
				Runtime::PlaySound("xlRumbleL", player, this->ShakePower/10, 0.0);
			}
			//log::info("Calling Growth Shake, power: {}", this->ShakePower);
			if (this->growth_time_input >= actor_data->half_life) { // Time in seconds" 160tick / 60 ticks per secong ~= 2.6s
				this->CallInputGrowth = false;
				this->growth_time_input = 0.0;
				actor_data->half_life = 1.0;
				// End growing
			}
		}

		if (this->AllowGrowth == false) {
			static Timer timer = Timer(3.0); // Run every 3.0s or as soon as we can
			if (timer.ShouldRun()) {
				if (ShouldGrow()) {
					//log::info("Random Growth True");
					// Start growing
					this->growth_time = 0.0;
					this->AllowGrowth = true;
					// Play sound
					GrowthTremorManager::GetSingleton().CallRumble(player, player, 6.0);
					float Volume = clamp(0.25, 2.0, get_visual_scale(player)/4);
					Runtime::PlaySound("MoanSound", player, 1.0, 0.0);
					Runtime::PlaySound("growthSound", player, Volume, 0.0);
				}
			}

		} else if (this->AllowGrowth == true && !hasSMT) {
			// Do the growing
			float delta_time = Time::WorldTimeDelta();
			int TotalPower = (100 + this->Randomize)/100;
			float Scale = get_visual_scale(player);
			float ProgressionMultiplier = Runtime::GetFloatOr("ProgressionMultiplier", 1.0);
			float base_power = ((0.0018 * TotalPower * 60.0 * Scale) * ProgressionMultiplier);  // Put in actual power please
			RestoreStats(); // Regens Attributes if PC has perk
			mod_target_scale(player, base_power * delta_time); // Use delta_time so that the growth will be the same regardless of fps
			GrowthTremorManager::GetSingleton().CallRumble(player, player, 1.5);
			this->growth_time += delta_time;
			if (this->growth_time >= 2.0) { // Time in seconds" 160tick / 60 ticks per secong ~= 2.6s
				// End growing
				this->AllowGrowth = false;
			}
		}

	}
}
