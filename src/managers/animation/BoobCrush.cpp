#include "managers/animation/Utils/AnimationUtils.hpp"
#include "managers/animation/Utils/CrawlUtils.hpp"
#include "managers/animation/AnimationManager.hpp"
#include "managers/damage/AccurateDamage.hpp"
#include "managers/animation/BoobCrush.hpp"
#include "managers/damage/LaunchActor.hpp"
#include "managers/animation/Grab.hpp"
#include "managers/GtsSizeManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/CrushManager.hpp"
#include "managers/explosion.hpp"
#include "managers/footstep.hpp"
#include "managers/highheel.hpp"
#include "utils/actorUtils.hpp"
#include "data/persistent.hpp"
#include "managers/Rumble.hpp"
#include "managers/tremor.hpp"
#include "data/runtime.hpp"
#include "scale/scale.hpp"
#include "data/time.hpp"
#include "events.hpp"
#include "node.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;
using namespace Gts;

/*
GTS_BoobCrush_Smile_On
GTS_BoobCrush_Smile_Off
GTS_BoobCrush_TrackBody        (Enables camera tracking)
GTS_BoobCrush_UnTrackBody  (Disables it)
GTS_BoobCrush_BreastImpact  (Damage everything under breasts, on impact)
GTS_BoobCrush_DOT_Start       (When we want to deal damage over time when we do long idle with swaying legs)
GTS_BoobCrush_DOT_End          
GTS_BoobCrush_Grow_Start
GTS_BoobCrush_Grow_Stop


*/

namespace {

    const std::vector<std::string_view> ALL_RUMBLE_NODES = { // used for body rumble
		"NPC COM [COM ]",
		"NPC L Foot [Lft ]",
		"NPC R Foot [Rft ]",
		"NPC L Toe0 [LToe]",
		"NPC R Toe0 [RToe]",
		"NPC L Calf [LClf]",
		"NPC R Calf [RClf]",
		"NPC L PreRearCalf",
		"NPC R PreRearCalf",
		"NPC L FrontThigh",
		"NPC R FrontThigh",
		"NPC R RearCalf [RrClf]",
		"NPC L RearCalf [RrClf]",
        "NPC L UpperarmTwist1 [LUt1]",
		"NPC L UpperarmTwist2 [LUt2]",
		"NPC L Forearm [LLar]",
		"NPC L ForearmTwist2 [LLt2]",
		"NPC L ForearmTwist1 [LLt1]",
		"NPC L Hand [LHnd]",
        "NPC R UpperarmTwist1 [RUt1]",
		"NPC R UpperarmTwist2 [RUt2]",
		"NPC R Forearm [RLar]",
		"NPC R ForearmTwist2 [RLt2]",
		"NPC R ForearmTwist1 [RLt1]",
		"NPC R Hand [RHnd]",
		"NPC L Breast",
		"NPC R Breast",
		"L Breast03",
		"R Breast03",
	};

    const std::string_view RNode = "NPC R Foot [Rft ]";
	const std::string_view LNode = "NPC L Foot [Lft ]";

	void StartRumble(std::string_view tag, Actor& actor, float power, float halflife) {
		for (auto& node_name: ALL_RUMBLE_NODES) {
			std::string rumbleName = std::format("BoobCrush_{}{}", tag, node_name);
		    Rumble::Start(rumbleName, &actor, power, halflife, node_name);
		}
	}

    void StopRumble(std::string_view tag, Actor& actor) {
		for (auto& node_name: ALL_RUMBLE_NODES) {
			std::string rumbleName = std::format("BoobCrush_{}{}", tag, node_name);
			Rumble::Stop(rumbleName, &actor);
		}
	}

	float GetBoobCrushDamage(Actor* actor) {
        float damage = 1.0;
        if (Runtime::HasPerkTeam(actor, "ButtCrush_KillerBooty")) {
            damage += 0.30;
        } if (Runtime::HasPerkTeam(actor, "ButtCrush_UnstableGrowth")) {
            damage += 0.70;
        }
        return damage;
    }

	void ModGrowthCount(Actor* giant, float value, bool reset) {
        auto transient = Transient::GetSingleton().GetData(giant);
		if (transient) {
			transient->ButtCrushGrowthAmount += value;
            if (reset) {
                transient->ButtCrushGrowthAmount = 0.0;
            }
		}
    }

	float GetGrowthCount(Actor* giant) {
        auto transient = Transient::GetSingleton().GetData(giant);
		if (transient) {
			return transient->ButtCrushGrowthAmount;
		}
        return 1.0;
    }

    void SetBonusSize(Actor* giant, float value, bool reset) {
        auto saved_data = Persistent::GetSingleton().GetData(giant);
        if (saved_data) {
            saved_data->bonus_max_size += value;
            if (reset) {
                mod_target_scale(giant, -saved_data->bonus_max_size);
                if (get_target_scale(giant) < get_natural_scale(giant)) {
                    set_target_scale(giant, get_natural_scale(giant)); // Protect against going into negatives
                }
                saved_data->bonus_max_size = 0;
            }
        } 
    }

	void GrowInSize(Actor* giant) {
        float scale = get_visual_scale(giant);
        float bonus = 0.24 * GetGrowthCount(giant) * (1.0 + (scale/15));
        float target = std::clamp(bonus/2, 0.02f, 0.80f);
        ModGrowthCount(giant, 1.0, false);
        SetBonusSize(giant, bonus, false);
        SpringGrow_Free(giant, bonus, 0.3 / GetAnimationSlowdown(giant), "ButtCrushGrowth");

        float WasteStamina = 60.0 * GetButtCrushCost(giant);
        DamageAV(giant, ActorValue::kStamina, WasteStamina);

        //CameraFOVTask(giant, 1.0, 0.003);
        Runtime::PlaySoundAtNode("growthSound", giant, 1.0, 1.0, "NPC Pelvis [Pelv]");
		Runtime::PlaySoundAtNode("MoanSound", giant, 1.0, 1.0, "NPC Head [Head]");

        StartRumble("CleavageRumble", data.giant, 1.8, 0.70);
	}

	void StartDamageOverTime(Actor* giant) {
		auto gianthandle = giant->CreateRefHandle();
		std::string name = std::format("BreastDOT_{}", giant->formID);
		float damage = GetButtCrushDamage(giant);
		TaskManager::Run(name, [=](auto& progressData) {
			if (!gianthandle) {
				return false;
			}
			auto giantref = gianthandle.get().get();

			auto BreastL = find_node(giantref, "NPC L Breast");
			auto BreastR = find_node(giantref, "NPC R Breast");
			auto BreastL03 = find_node(giantref, "L Breast03");
			auto BreastR03 = find_node(giantref, "R Breast03");
			if (BreastL03 && BreastR03) {
				Rumble::Once("BreastDot_L", giantref, 1.0, 0.025, BreastL03);
				Rumble::Once("BreastDot_R", giantref, 1.0, 0.025, BreastR03);
				DoDamageAtPoint(giant, 20, 2.0 * damage, BreastL03, 400, 0.10, 2.5, DamageSource::Breast);
                DoDamageAtPoint(giant, 20, 2.0 * damage, BreastR03, 400, 0.10, 2.5, DamageSource::Breast);
				return true;
			}
			else if (BreastL && BreastR) {
				Rumble::Once("BreastDot_L", giantref, 1.0, 0.025, BreastL);
				Rumble::Once("BreastDot_R", giantref, 1.0, 0.025, BreastR);
				DoDamageAtPoint(giant, 20, 2.0 * damage, BreastL, 400, 0.10, 2.5, DamageSource::Breast);
                DoDamageAtPoint(giant, 20, 2.0 * damage, BreastR, 400, 0.10, 2.5, DamageSource::Breast);
				return true;
			}
			return false;
		});
	}

	void StopDamageOverTime(Actor* giant) {
		std::string name = std::format("BreastDOT_{}", giant->formID);
		TaskManager::Cancel(name);
	}

	void InflictDamage(Actor* giant) {
		float damage = GetButtCrushDamage(giant);
        auto BreastL = find_node(giant, "NPC L Breast");
		auto BreastR = find_node(giant, "NPC R Breast");
		auto BreastL03 = find_node(giant, "L Breast03");
		auto BreastR03 = find_node(giant, "R Breast03");
		if (BreastL03 && BreastR03) {
			DoDamageAtPoint(giant, 28, 330.0 * damage, ThighL, 4, 0.70, 0.85, DamageSource::Breast);
			DoDamageAtPoint(giant, 28, 330.0 * damage, ThighR, 4, 0.70, 0.85, DamageSource::Breast);
			DoDustExplosion(giant, 1.45 * dust * damage, FootEvent::Right, "NPC L Breast");
			DoDustExplosion(giant, 1.45 * dust * damage, FootEvent::Left, "NPC R Breast");
			DoFootstepSound(giant, 1.25, FootEvent::Right, BreastR);
			DoFootstepSound(giant, 1.25, FootEvent::Left, BreastL);
			DoLaunch(&data.giant, 28.00 * launch * perk, 4.20, 1.4, FootEvent::Breasts, 1.20);
			Rumble::Once("Breast_L", &data.giant, 3.60 * damage, 0.02, "NPC L Breast");
			Rumble::Once("Breast_R", &data.giant, 3.60 * damage, 0.02, "NPC R Breast");
			ModGrowthCount(giant, 0, true); // Reset limit
			return;
		} else if (BreastL && BreastR) {
			DoDamageAtPoint(giant, 28, 330.0 * damage, ThighL, 4, 0.70, 0.85, DamageSource::Breast);
			DoDamageAtPoint(giant, 28, 330.0 * damage, ThighR, 4, 0.70, 0.85, DamageSource::Breast);
			DoDustExplosion(giant, 1.45 * dust * damage, FootEvent::Right, "NPC L Breast");
			DoDustExplosion(giant, 1.45 * dust * damage, FootEvent::Left, "NPC R Breast");
			DoFootstepSound(giant, 1.25, FootEvent::Right, BreastR);
			DoFootstepSound(giant, 1.25, FootEvent::Right, BreastL);
			DoLaunch(&data.giant, 28.00 * launch * perk, 4.20, 1.4, FootEvent::Breasts, 1.20);
			Rumble::Once("Breast_L", &data.giant, 3.60 * damage, 0.02, "NPC L Breast");
			Rumble::Once("Breast_R", &data.giant, 3.60 * damage, 0.02, "NPC R Breast");
			ModGrowthCount(giant, 0, true); // Reset limit
			return;
        } else {
            if (!BreastR) {
                Notify("Error: Missing Breast Nodes"); // Will help people to troubleshoot it. Not everyone has 3BB/XPMS32 body.
                Notify("Error: effects not inflicted");
                Notify("Suggestion: install XP32 Skeleton");
            } else if (!BreastR03) {
				Notify("Error: Missing 3BB Breast Nodes"); // Will help people to troubleshoot it. Not everyone has 3BB/XPMS32 body.
                Notify("Error: effects not inflicted");
                Notify("Suggestion: install 3BB/SMP Body");
			}
        }
	}

	void TrackBreasts(Actor* giant, bool enable) {
		if (giant->formID == 0x14) {
			if (AllowFeetTracking()) {
				auto& sizemanager = SizeManager::GetSingleton();
				sizemanager.SetActionBool(giant, enable, 9.0);
			}
		}
	}

	void GTS_BoobCrush_Smile_On(AnimationEventData& data) {
		auto giant = &data.giant;
		AdjustFacialExpression(giant, 0, 1.0, "modifier"); // blink L
		AdjustFacialExpression(giant, 1, 1.0, "modifier"); // blink R
		AdjustFacialExpression(giant, 0, 0.75, "phenome");
	}

	void GTS_BoobCrush_Smile_Off(AnimationEventData& data) {
		auto giant = &data.giant;
		AdjustFacialExpression(giant, 0, 0.0, "modifier"); // blink L
		AdjustFacialExpression(giant, 1, 0.0, "modifier"); // blink R
		AdjustFacialExpression(giant, 0, 0.0, "phenome");
	}
	void GTS_BoobCrush_TrackBody(AnimationEventData& data) {
		TrackBreasts(&data.giant, true);
	}
	void GTS_BoobCrush_UnTrackBody(AnimationEventData& data) {
		TrackBreasts(&data.giant, false);
	}
	void GTS_BoobCrush_BreastImpact(AnimationEventData& data) {
		InflictDamage(&data.giant);
	}
	void GTS_BoobCrush_DOT_Start(AnimationEventData& data) {
		StartDamageOverTime(&data.giant);
	}
	void GTS_BoobCrush_DOT_End(AnimationEventData& data) {
		StopDamageOverTime(&data.giant);
	}
	void GTS_BoobCrush_Grow_Start(AnimationEventData& data) {
		GrowInSize(&data.giant);
	}
	void GTS_BoobCrush_Grow_Stop(AnimationEventData& data) {
		StopRumble("CleavageRumble", data.giant);
	}
}

namespace Gts
{
	void AnimationBoobCrush::RegisterEvents() {
		AnimationManager::RegisterEvent("GTS_BoobCrush_Smile_On", "BoobCrush", GTS_BoobCrush_Smile_On);
		AnimationManager::RegisterEvent("GTS_BoobCrush_Smile_Off", "BoobCrush", GTS_BoobCrush_Smile_Off);
		AnimationManager::RegisterEvent("GTS_BoobCrush_TrackBody", "BoobCrush", GTS_BoobCrush_TrackBody);
		AnimationManager::RegisterEvent("GTS_BoobCrush_UnTrackBody", "BoobCrush", GTS_BoobCrush_UnTrackBody);
		AnimationManager::RegisterEvent("GTS_BoobCrush_BreastImpact", "BoobCrush", GTS_BoobCrush_BreastImpact);
		AnimationManager::RegisterEvent("GTS_BoobCrush_DOT_Start", "BoobCrush", GTS_BoobCrush_DOT_Start);
		AnimationManager::RegisterEvent("GTS_BoobCrush_DOT_End", "BoobCrush", GTS_BoobCrush_DOT_End);
		AnimationManager::RegisterEvent("GTS_BoobCrush_Grow_Start", "BoobCrush", GTS_BoobCrush_Grow_Start);
		AnimationManager::RegisterEvent("GTS_BoobCrush_Grow_Stop", "BoobCrush", GTS_BoobCrush_Grow_Stop);
	}
}