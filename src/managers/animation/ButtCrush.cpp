#include "managers/animation/Controllers/ButtCrushController.hpp"
#include "managers/animation/Utils/AnimationUtils.hpp"
#include "managers/animation/Utils/CrawlUtils.hpp"
#include "managers/animation/AnimationManager.hpp"
#include "managers/damage/AccurateDamage.hpp"
#include "managers/animation/ButtCrush.hpp"
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
#include "node.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;
using namespace Gts;

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
	};

	const std::string_view RNode = "NPC R Foot [Rft ]";
	const std::string_view LNode = "NPC L Foot [Lft ]";

	void StartRumble(std::string_view tag, Actor& actor, float power, float halflife) {
		for (auto& node_name: ALL_RUMBLE_NODES) {
			std::string rumbleName = std::format("ButtCrush_{}{}", tag, node_name);
			GRumble::Start(rumbleName, &actor, power, halflife, node_name);
		}
	}

	void StopRumble(std::string_view tag, Actor& actor) {
		for (auto& node_name: ALL_RUMBLE_NODES) {
			std::string rumbleName = std::format("ButtCrush_{}{}", tag, node_name);
			GRumble::Stop(rumbleName, &actor);
		}
	}

	void CameraFOVTask(Actor* actor, float reduce, float speed) {
		auto camera = PlayerCamera::GetSingleton();
		if (!camera) {
			return;
		}
		if (actor->formID == 0x14) {
			auto tranData = Transient::GetSingleton().GetData(actor);
			bool TP = camera->IsInThirdPerson();
			bool FP = camera->IsInFirstPerson();

			if (tranData) {
				tranData->WorldFov_Default = camera->worldFOV;
				tranData->FpFov_Default = camera->firstPersonFOV;
				float DefaultTP = tranData->WorldFov_Default;
				float DefaultFP = tranData->FpFov_Default;
				if (DefaultTP > 0) {
					std::string name = std::format("Fov_Growth_{}", actor->formID);
					ActorHandle gianthandle = actor->CreateRefHandle();
					camera->worldFOV *= reduce;
					TaskManager::Run(name, [=](auto& progressData) {
						if (!gianthandle) {
							return false;
						}
						auto giantref = gianthandle.get().get();
						float scale = 1.0 + (get_visual_scale(giantref)/10);
						camera->worldFOV += DefaultTP * speed;
						if (camera->worldFOV >= DefaultTP) {
							camera->worldFOV = DefaultTP;
							return false; // stop it
						}
						return true;
					});
				}
			}
		}
	}



	void TrackButt(Actor* giant, bool enable) {
		if (AllowFeetTracking()) {
			auto& sizemanager = SizeManager::GetSingleton();
			sizemanager.SetActionBool(giant, enable, 8.0);
		}
	}

	void DisableButtTrackTask(Actor* giant) {
		std::string name = std::format("DisableCamera_{}", giant->formID);
		auto gianthandle = giant->CreateRefHandle();
		auto FrameA = Time::FramesElapsed();
		TaskManager::Run(name, [=](auto& progressData) {
			if (!gianthandle) {
				return false;
			}
			auto FrameB = Time::FramesElapsed() - FrameA;
			if (FrameB <= 60.0) {
				return true;
			}
			auto giantref = gianthandle.get().get();
			TrackButt(giantref, false);

			return false;
		});
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

	float GetGrowthCount(Actor* giant) {
		auto transient = Transient::GetSingleton().GetData(giant);
		if (transient) {
			return transient->ButtCrushGrowthAmount;
		}
		return 1.0;
	}

	float GetGrowthLimit(Actor* actor) {
		float limit = 0;
		if (Runtime::HasPerkTeam(actor, "ButtCrush_GrowingDisaster")) {
			limit += 2.0;
		}
		if (Runtime::HasPerkTeam(actor, "ButtCrush_UnstableGrowth")) {
			limit += 3.0;
		}
		if (Runtime::HasPerkTeam(actor, "ButtCrush_LoomingDoom")) {
			limit += 4.0;
		}
		return limit;
	}

	float GetButtCrushDamage(Actor* actor) {
		float damage = 1.0;
		if (Runtime::HasPerkTeam(actor, "ButtCrush_KillerBooty")) {
			damage += 0.30;
		}
		if (Runtime::HasPerkTeam(actor, "ButtCrush_UnstableGrowth")) {
			damage += 0.70;
		}
		return damage;
	}

	void GTSButtCrush_MoveBody_MixFrameToLoop(AnimationEventData& data) {
		auto giant = &data.giant;
		TrackButt(giant, true);
	}

	void GTSButtCrush_GrowthStart(AnimationEventData& data) {
		auto giant = &data.giant;

		float scale = get_visual_scale(giant);
		float bonus = 0.24 * (GetGrowthCount(giant) + 1.0);
		float target = std::clamp(bonus/2, 0.02f, 0.80f);
		ModGrowthCount(giant, 1.0, false);
		SetBonusSize(giant, bonus, false);
		SpringGrow_Free(giant, bonus, 0.3 / GetAnimationSlowdown(giant), "ButtCrushGrowth");

		float WasteStamina = 60.0 * GetButtCrushCost(giant);
		DamageAV(giant, ActorValue::kStamina, WasteStamina);

		Runtime::PlaySoundAtNode("growthSound", giant, 1.0, 1.0, "NPC Pelvis [Pelv]");
		Runtime::PlaySoundAtNode("MoanSound", giant, 1.0, 1.0, "NPC Head [Head]");

		StartRumble("BCRumble", data.giant, 0.4, 0.40);
	}

	void GTSBEH_ButtCrush_GrowthFinish(AnimationEventData& data) {
		StopRumble("BCRumble", data.giant);
	}

	void GTSButtCrush_FootstepR(AnimationEventData& data) {
		float shake = 1.0;
		float launch = 1.0;
		float dust = 1.00;
		float perk = GetPerkBonus_Basics(&data.giant);
		if (HasSMT(&data.giant)) {
			shake = 4.0;
			launch = 1.2;
			dust = 1.25;
		}
		GRumble::Once("FS_R", &data.giant, 2.20, 0.0, RNode);
		DoDamageEffect(&data.giant, 1.4, 1.45, 10, 0.25, FootEvent::Right, 1.0, DamageSource::Booty);
		DoFootstepSound(&data.giant, 1.0, FootEvent::Right, RNode);
		DoDustExplosion(&data.giant, dust, FootEvent::Right, RNode);
		DoLaunch(&data.giant, 0.65 * perk, 1.3, FootEvent::Right);
	}

	void GTSButtCrush_FootstepL(AnimationEventData& data) {
		float shake = 1.0;
		float dust = 1.00;
		float perk = GetPerkBonus_Basics(&data.giant);
		if (HasSMT(&data.giant)) {
			shake = 4.0;
			dust = 1.25;
		}
		GRumble::Once("FS_L", &data.giant, 2.20, 0.0, LNode);
		DoDamageEffect(&data.giant, 1.4, 1.45, 10, 0.25, FootEvent::Left, 1.0, DamageSource::Booty);
		DoFootstepSound(&data.giant, 1.0, FootEvent::Left, LNode);
		DoDustExplosion(&data.giant, dust, FootEvent::Left, LNode);
		DoLaunch(&data.giant, 0.65 * perk, 1.3, FootEvent::Left);
	}

	void GTSButtCrush_HandImpactR(AnimationEventData& data) {
		auto giant = &data.giant;
		float scale = get_visual_scale(giant);
		DoCrawlingFunctions(giant, scale, 1.0, 1.8, CrawlEvent::RightHand, "RightHand", 0.5, 14, 1.0, DamageSource::HandCrawlRight);
	}

	void GTSButtCrush_FallDownImpact(AnimationEventData& data) {
		auto giant = &data.giant;

		float perk = GetPerkBonus_Basics(&data.giant);
		float dust = 1.0;

		if (HasSMT(giant)) {
			dust = 1.25;
		}

		SetBonusSize(giant, 0.0, true);

		float damage = GetButtCrushDamage(giant);
		auto ThighL = find_node(giant, "NPC L Thigh [LThg]");
		auto ThighR = find_node(giant, "NPC R Thigh [RThg]");
		auto ButtR = find_node(giant, "NPC R Butt");
		auto ButtL = find_node(giant, "NPC L Butt");
		if (ButtR && ButtL) {
			if (ThighL && ThighR) {
				DoDamageAtPoint(giant, 28, 330.0 * damage, ThighL, 4, 0.70, 0.85, DamageSource::Booty);
				DoDamageAtPoint(giant, 28, 330.0 * damage, ThighR, 4, 0.70, 0.85, DamageSource::Booty);
				DoDustExplosion(giant, 1.45 * dust * damage, FootEvent::Right, "NPC R Butt");
				DoDustExplosion(giant, 1.45 * dust * damage, FootEvent::Left, "NPC L Butt");
				DoFootstepSound(giant, 1.25, FootEvent::Right, RNode);
				DoLaunch(&data.giant, 1.30 * perk, 4.20, FootEvent::Butt);
				GRumble::Once("Butt_L", &data.giant, 3.60 * damage, 0.02, "NPC R Butt");
				GRumble::Once("Butt_R", &data.giant, 3.60 * damage, 0.02, "NPC L Butt");
			}
		} else {
			if (!ButtR) {
				Notify("Error: Missing Butt Nodes"); // Will help people to troubleshoot it. Not everyone has 3BB/XPMS32 body.
				Notify("Error: effects not inflicted");
				Notify("install 3BBB/XP32 Skeleton");
			}
			if (!ThighL) {
				Notify("Error: Missing Thigh Nodes");
				Notify("Error: effects not inflicted");
				Notify("install 3BBB/XP32 Skeleton");
			}
		}
		ModGrowthCount(giant, 0, true); // Reset limit
		DisableButtTrackTask(giant);
	}

	void GTSButtCrush_Exit(AnimationEventData& data) {
		auto giant = &data.giant;
		ModGrowthCount(giant, 0, true); // Reset limit
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	///                     T R I G G E R S
	///
	///////////////////////////////////////////////////////////////////////////////////////////////////////////


	void ButtCrushStartEvent(const InputEventData& data) {
		auto player = PlayerCharacter::GetSingleton();
		if (IsFirstPerson()) {
			return;
		} if (!CanPerformAnimation(player, 2)) {
			return;
		}
		auto grabbedActor = Grab::GetHeldActor(player);
		if (grabbedActor && !IsCrawling(player)) { // IF we have someone in hands, allow only when we crawl
			return;
		}
		if (Runtime::HasPerk(player, "ButtCrush_NoEscape")) {
			auto& ButtCrush = ButtCrushController::GetSingleton();
			std::size_t numberOfPrey = 3;
			if (Runtime::HasPerk(player, "MassVorePerk")) {
				numberOfPrey = 3 + (get_visual_scale(player)/3);
			}
			std::vector<Actor*> preys = ButtCrush.GetButtCrushTargets(player, numberOfPrey);
			for (auto prey: preys) {
				ButtCrush.StartButtCrush(player, prey);
			}
			return;
		} else if (CanDoButtCrush(player) && !Runtime::HasPerk(player, "ButtCrush_NoEscape")) {
			float WasteStamina = 100.0 * GetButtCrushCost(player);
			DamageAV(player, ActorValue::kStamina, WasteStamina);
			AnimationManager::StartAnim("ButtCrush_StartFast", player);
		} else if (!CanDoButtCrush(player) && !Runtime::HasPerk(player, "ButtCrush_NoEscape")) {
			if (!IsCrawling(player)) {
				TiredSound(player, "Butt Crush is on a cooldown");
			} else {
				TiredSound(player, "Breast Crush is on a cooldown");
			}
		}
	}

	void QuickButtCrushStartEvent(const InputEventData& data) {
		auto player = PlayerCharacter::GetSingleton();
		if (IsFirstPerson()) {
			return;
		} if (!CanPerformAnimation(player, 2)) {
			return;
		}
		auto grabbedActor = Grab::GetHeldActor(player);
		if (grabbedActor && !IsCrawling(player)) { // IF we have someone in hands, allow only when we crawl
			return;
		}
		if (CanDoButtCrush(player)) {
			AnimationManager::StartAnim("ButtCrush_StartFast", player);
		} else {
			if (!IsCrawling(player)) {
				TiredSound(player, "Butt Crush is on a cooldown");
			} else {
				TiredSound(player, "Breast Crush is on a cooldown");
			}
		}
	}

	void ButtCrushGrowEvent(const InputEventData& data) {
		auto player = PlayerCharacter::GetSingleton();
		if (IsFirstPerson()) {
			return;
		}
		if (IsButtCrushing(player) && Runtime::HasPerk(player, "ButtCrush_GrowingDisaster")) {
			float GrowthCount = GetGrowthLimit(player);
			bool CanGrow = ButtCrush_IsAbleToGrow(player, GrowthCount);
			if (CanGrow) {
				AnimationManager::StartAnim("ButtCrush_Growth", player);
			} else {
				TiredSound(player, "Your body can't grow any further");
			}
		}
	}

	void ButtCrushAttackEvent(const InputEventData& data) {
		auto player = PlayerCharacter::GetSingleton();
		if (IsButtCrushing(player)) {
			AnimationManager::StartAnim("ButtCrush_Attack", player);
		}
	}
}

namespace Gts
{
	void AnimationButtCrush::RegisterEvents() {
		AnimationManager::RegisterEvent("GTSButtCrush_Exit", "ButtCrush", GTSButtCrush_Exit);
		AnimationManager::RegisterEvent("GTSButtCrush_GrowthStart", "ButtCrush", GTSButtCrush_GrowthStart);
		AnimationManager::RegisterEvent("GTSBEH_ButtCrush_GrowthFinish", "ButtCrush", GTSBEH_ButtCrush_GrowthFinish);
		AnimationManager::RegisterEvent("GTSButtCrush_FallDownImpact", "ButtCrush", GTSButtCrush_FallDownImpact);
		AnimationManager::RegisterEvent("GTSButtCrush_HandImpactR", "ButtCrush", GTSButtCrush_HandImpactR);
		AnimationManager::RegisterEvent("GTSButtCrush_FootstepR", "ButtCrush", GTSButtCrush_FootstepR);
		AnimationManager::RegisterEvent("GTSButtCrush_FootstepL", "ButtCrush", GTSButtCrush_FootstepL);
		AnimationManager::RegisterEvent("GTSButtCrush_MoveBody_MixFrameToLoop", "ButtCrush", GTSButtCrush_MoveBody_MixFrameToLoop);

		InputManager::RegisterInputEvent("ButtCrushStart", ButtCrushStartEvent);
		InputManager::RegisterInputEvent("QuickButtCrushStart", QuickButtCrushStartEvent);
		InputManager::RegisterInputEvent("ButtCrushGrow", ButtCrushGrowEvent);
		InputManager::RegisterInputEvent("ButtCrushAttack", ButtCrushAttackEvent);
	}

	void AnimationButtCrush::RegisterTriggers() {
		AnimationManager::RegisterTrigger("ButtCrush_Start", "ButtCrush", "GTSBEH_ButtCrush_Start");
		AnimationManager::RegisterTrigger("ButtCrush_Attack", "ButtCrush", "GTSBEH_ButtCrush_Attack");
		AnimationManager::RegisterTrigger("ButtCrush_Growth", "ButtCrush", "GTSBEH_ButtCrush_Grow");
		AnimationManager::RegisterTrigger("ButtCrush_StartFast", "ButtCrush", "GTSBEH_ButtCrush_StartFast");
	}
}