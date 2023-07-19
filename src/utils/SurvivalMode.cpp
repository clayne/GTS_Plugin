#include "managers/animation/AnimationManager.hpp"
#include "managers/damage/AccurateDamage.hpp"
#include "managers/GtsSizeManager.hpp"
#include "magic/effects/common.hpp"
#include "utils/papyrusUtils.hpp"
#include "managers/explosion.hpp"
#include "managers/highheel.hpp"
#include "managers/footstep.hpp"
#include "utils/actorUtils.hpp"
#include "utils/SurvivalMode.hpp"
#include "utils/findActor.hpp"
#include "data/persistent.hpp"
#include "data/transient.hpp"
#include "data/runtime.hpp"
#include "spring.hpp"
#include "scale/scale.hpp"
#include "colliders/RE.hpp"
#include "colliders/actor.hpp"
#include "timer.hpp"
#include "node.hpp"
#include "utils/av.hpp"
#include "colliders/RE.hpp"

using namespace RE;
using namespace Gts;

namespace Gts {

    void SurvivalMode_RemoveAllSpells(Actor* actor, SpellItem* stage0, SpellItem* stage1, SpellItem* stage2, SpellItem* stage3, SpellItem* stage4, SpellItem* stage5) {
		actor->RemoveSpell(stage0);
		actor->RemoveSpell(stage1);
		actor->RemoveSpell(stage2);
		actor->RemoveSpell(stage3);
		actor->RemoveSpell(stage4);
		actor->RemoveSpell(stage5);
        log::info("Removing all survival spells");
	}

	void SurvivalMode_RefreshSpells(Actor* actor, float currentvalue) {
		auto stage0 = Runtime::GetSpell("Survival_HungerStage0");
		auto stage1 = Runtime::GetSpell("Survival_HungerStage1");
		auto stage2 = Runtime::GetSpell("Survival_HungerStage2");
		auto stage3 = Runtime::GetSpell("Survival_HungerStage3");
		auto stage4 = Runtime::GetSpell("Survival_HungerStage4");
		auto stage5 = Runtime::GetSpell("Survival_HungerStage5");

		auto stage1threshold = Runtime::GetFloat("Survival_HungerStage1Value");
		auto stage2threshold = Runtime::GetFloat("Survival_HungerStage2Value");
		auto stage3threshold = Runtime::GetFloat("Survival_HungerStage3Value");
		auto stage4threshold = Runtime::GetFloat("Survival_HungerStage4Value");
		auto stage5threshold = Runtime::GetFloat("Survival_HungerStage5Value");

		SurvivalMode_RemoveAllSpells(actor, stage0, stage1, stage2, stage3, stage4, stage5);
        log::info("Adjust current value, value: {}", currentvalue);
		if (currentvalue <= stage1threshold) {
			Runtime::AddSpell(actor, "Survival_HungerStage1");
		} else if (currentvalue <= stage2threshold) {
			Runtime::AddSpell(actor, "Survival_HungerStage2");
		} else if (currentvalue <= stage3threshold) {
			Runtime::AddSpell(actor, "Survival_HungerStage3");
		} else if (currentvalue <= stage4threshold) {
			Runtime::AddSpell(actor, "Survival_HungerStage4");
		} else if (currentvalue <= stage5threshold) {
			Runtime::AddSpell(actor, "Survival_HungerStage5");
		}
	}

	void SurvivalMode_AdjustHunger(Actor* giant, float naturalsize, bool IsDragon, bool IsLiving, float type) {
		if (giant->formID != 0x14) {
			return; //Only for Player
		} 
        auto Survival = Runtime::GetGlobal("Survival_ModeEnabled");
        if (!Survival) {
            return; // Abort if it doesn't exist
        }
        float SurvivalEnabled = Runtime::GetFloat("Survival_ModeEnabled");
        if (!SurvivalEnabled) {
            return;
        }
		float HungerNeed = Runtime::GetFloat("Survival_HungerNeedValue");
        float restore = 0;

        float modifier = naturalsize;

        if (IsDragon) {
            modifier *= 8.0; // Dragons are huge, makes sense to give a lot of value
        } if (!IsLiving) {
            modifier *= 0.20; // Less effective on non living targets
        }

        if (type == 0) {
            restore = Runtime::GetFloat("Survival_HungerRestoreSmallAmount");
        } else if (type == 1) {
            restore = Runtime::GetFloat("Survival_HungerRestoreSmallAmount") * 6;
        }
        log::info("Adjusting HungerNeed, restore: {}, type: {}", restore, type);
		HungerNeed->value -= restore * modifier;
		if (HungerNeed->value <= 0.0) {
			HungerNeed->value = 0.0; // Cap it at 0.0
		}
		float value = HungerNeed->value;
		SurvivalMode_RefreshSpells(giant, value);
	}
}