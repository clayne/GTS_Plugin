#include "managers/animation/AnimationManager.hpp"
#include "managers/damage/AccurateDamage.hpp"
#include "managers/ai/aifunctions.hpp"
#include "managers/GtsSizeManager.hpp"
#include "magic/effects/common.hpp"
#include "managers/Attributes.hpp"
#include "utils/papyrusUtils.hpp"
#include "managers/explosion.hpp"
#include "utils/DeathReport.hpp"
#include "managers/highheel.hpp"
#include "managers/footstep.hpp"
#include "utils/actorUtils.hpp"
#include "managers/Rumble.hpp"
#include "utils/findActor.hpp"
#include "data/persistent.hpp"
#include "data/transient.hpp"
#include "data/runtime.hpp"
#include "spring.hpp"
#include "scale/scale.hpp"
#include "colliders/RE.hpp"
#include "colliders/actor.hpp"
#include "profiler.hpp"
#include "timer.hpp"
#include "node.hpp"
#include "utils/av.hpp"
#include "colliders/RE.hpp"
#include "UI/DebugAPI.hpp"
#include "raycast.hpp"
#include <vector>
#include <string>

using namespace RE;
using namespace Gts;

namespace {
	float ShakeStrength(Actor* Source) {
		float Size = get_visual_scale(Source);
		float k = 0.065;
		float n = 1.0;
		float s = 1.12;
		float Result = 1.0/(pow(1.0+pow(k*(Size),n*s),1.0/s));
		return Result;
	}

	void StartResetTask(Actor* tiny) {
		std::string name = std::format("ResetActor_{}", tiny->formID);
		float Start = Time::WorldTimeElapsed();
		ActorHandle tinyhandle = tiny->CreateRefHandle();
		TaskManager::Run(name, [=](auto& progressData) {
			if (!tinyhandle) {
				return false;
			}
			auto tiny = tinyhandle.get().get();
			float Finish = Time::WorldTimeElapsed();
			float timepassed = Finish - Start;
			if (timepassed < 1.0) {
				return true; // not enough time has passed yet
			}
			log::info("{} was reset", tiny->GetDisplayFullName());
			EventDispatcher::DoResetActor(tiny);
			return false; // stop task, we reset the actor
		});

	}

	ExtraDataList* CreateExDataList() {
		size_t a_size;
		if (SKYRIM_REL_CONSTEXPR (REL::Module::IsAE()) && (REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_6_629)) {
			a_size = 0x20;
		} else {
			a_size = 0x18;
		}
		auto memory = RE::malloc(a_size);
		std::memset(memory, 0, a_size);
		if (SKYRIM_REL_CONSTEXPR (REL::Module::IsAE()) && (REL::Module::get().version() >= SKSE::RUNTIME_SSE_1_6_629)) {
			// reinterpret_cast<std::uintptr_t*>(memory)[0] = a_vtbl; // Unknown vtable location add once REd
			REL::RelocateMember<BSReadWriteLock>(memory, 0x18) = BSReadWriteLock();
		} else {
			REL::RelocateMember<BSReadWriteLock>(memory, 0x10) = BSReadWriteLock();
		}
		return static_cast<ExtraDataList*>(memory);
	}

	struct SpringGrowData {
		Spring amount = Spring(0.0, 1.0);
		float addedSoFar = 0.0;
		ActorHandle actor;

		SpringGrowData(Actor* actor, float amountToAdd, float halfLife) : actor(actor->CreateRefHandle()) {
			amount.value = 0.0;
			amount.target = amountToAdd;
			amount.halflife = halfLife;
		}
	};

	struct SpringShrinkData {
		Spring amount = Spring(0.0, 1.0);
		float addedSoFar = 0.0;
		ActorHandle actor;

		SpringShrinkData(Actor* actor, float amountToAdd, float halfLife) : actor(actor->CreateRefHandle()) {
			amount.value = 0.0;
			amount.target = amountToAdd;
			amount.halflife = halfLife;
		}
	};
}

RE::ExtraDataList::~ExtraDataList() {
}

namespace Gts {

	RE::NiPoint3 RotateAngleAxis(const RE::NiPoint3& vec, const float angle, const RE::NiPoint3& axis) {
		float S = sin(angle);
		float C = cos(angle);

		const float XX = axis.x * axis.x;
		const float YY = axis.y * axis.y;
		const float ZZ = axis.z * axis.z;

		const float XY = axis.x * axis.y;
		const float YZ = axis.y * axis.z;
		const float ZX = axis.z * axis.x;

		const float XS = axis.x * S;
		const float YS = axis.y * S;
		const float ZS = axis.z * S;

		const float OMC = 1.f - C;

		return RE::NiPoint3(
			(OMC * XX + C) * vec.x + (OMC * XY - ZS) * vec.y + (OMC * ZX + YS) * vec.z,
			(OMC * XY + ZS) * vec.x + (OMC * YY + C) * vec.y + (OMC * YZ - XS) * vec.z,
			(OMC * ZX - YS) * vec.x + (OMC * YZ + XS) * vec.y + (OMC * ZZ + C) * vec.z
		);
	}

	Actor* GetActorPtr(Actor* actor) {
		return actor;
	}

	Actor* GetActorPtr(Actor& actor) {
		return &actor;
	}

	Actor* GetActorPtr(ActorHandle& actor) {
		if (!actor) {
			return nullptr;
		}
		return actor.get().get();
	}
	Actor* GetActorPtr(const ActorHandle& actor) {
		if (!actor) {
			return nullptr;
		}
		return actor.get().get();
	}
	Actor* GetActorPtr(FormID formId) {
		Actor* actor = TESForm::LookupByID<Actor>(formId);
		if (!actor) {
			return nullptr;
		}
		return actor;
	}

	float GetLaunchPower(float sizeRatio) {
		// https://www.desmos.com/calculator/wh0vwgljfl
		SoftPotential launch {
			.k = 1.42,
			.n = 0.78,
			.s = 0.6,
			.a = 0.8,
		};
		float power = soft_power(sizeRatio, launch);
		return power;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//                                 G T S   ST A T E S  B O O L S                                                                      //
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	bool IsEquipBusy(Actor* actor) {
		auto profiler = Profilers::Profile("ActorUtils: IsEquipBusy");
		int State;
		actor->GetGraphVariableInt("currentDefaultState", State);
		if (State >= 10 && State <= 20) {
			return true;
		}
		return false;
	}

	bool IsProning(Actor* actor) {
		bool prone;
		auto transient = Transient::GetSingleton().GetData(actor);
		actor->GetGraphVariableBool("GTS_IsProne", prone);
		if (actor->formID == 0x14 && actor->IsSneaking() && IsFirstPerson() && transient) {
			return transient->FPProning; // Needed to fix crawling being applied to FP even when Prone is off
		}
		return actor!= nullptr && actor->IsSneaking() && prone;
	}

	bool IsCrawling(Actor* actor) {
		bool crawl;
		auto transient = Transient::GetSingleton().GetData(actor);
		actor->GetGraphVariableBool("GTS_IsCrawling", crawl);
		if (actor->formID == 0x14 && actor->IsSneaking() && IsFirstPerson() && transient) {
			return transient->FPCrawling; // Needed to fix crawling being applied to FP even when Prone is off
		}
		return actor!= nullptr && actor->IsSneaking() && crawl;
	}

	bool IsTransitioning(Actor* actor) { // reports sneak transition to crawl
		bool transition;
		actor->GetGraphVariableBool("GTS_Transitioning", transition);
		return transition;
	}

	bool IsFootGrinding(Actor* actor) {
		bool grind;
		actor->GetGraphVariableBool("GTS_IsFootGrinding", grind);
		return grind;
	}

	bool isTrampling(Actor* actor) {
		bool trample;
		actor->GetGraphVariableBool("GTS_IsTrampling", trample);
		return trample;
	}

	bool IsJumping(Actor* actor) {
		auto profiler = Profilers::Profile("ActorUtils: IsJumping");
		if (!actor) {
			return false;
		}
		if (!actor->Is3DLoaded()) {
			return false;
		}
		bool result = false;
		actor->GetGraphVariableBool("bInJumpState", result);
		return result;
	}

	bool IsBeingHeld(Actor* tiny) {
		auto transient = Transient::GetSingleton().GetData(tiny);
		if (transient) {
			return transient->being_held;
		}
		return false;
	}

	bool IsBetweenBreasts(Actor* actor) {
		auto transient = Transient::GetSingleton().GetData(actor);
		if (transient) {
			return transient->between_breasts;
		}
		return false;
	}

	bool IsTransferingTiny(Actor* actor) { // Reports 'Do we have someone grabed?'
		int grabbed;
		actor->GetGraphVariableInt("GTS_GrabbedTiny", grabbed);
		return grabbed > 0;
	}

	bool IsUsingThighAnimations(Actor* actor) { // Do we currently use Thigh Crush / Thigh Sandwich?
		int sitting;
		actor->GetGraphVariableInt("GTS_Sitting", sitting);
		return sitting > 0;
	}

	bool IsSynced(Actor* actor) {
		bool sync;
		actor->GetGraphVariableBool("bIsSynced", sync);
		return sync;
	}

	bool CanDoPaired(Actor* actor) {
		bool paired;
		actor->GetGraphVariableBool("GTS_CanDoPaired", paired);
		return paired;
	}


	bool IsThighCrushing(Actor* actor) { // Are we currently doing Thigh Crush?
		int crushing;
		actor->GetGraphVariableInt("GTS_IsThighCrushing", crushing);
		return crushing > 0;
	}

	bool IsThighSandwiching(Actor* actor) { // Are we currently Thigh Sandwiching?
		int sandwiching;
		actor->GetGraphVariableInt("GTS_IsThighSandwiching", sandwiching);
		return sandwiching > 0;
	}

	bool IsStomping(Actor* actor) {
		int Stomping;
		actor->GetGraphVariableInt("GTS_IsStomping", Stomping);
		return Stomping > 0;
	}

	bool IsBeingEaten(Actor* tiny) {
		auto transient = Transient::GetSingleton().GetData(tiny);
		if (transient) {
			return transient->about_to_be_eaten;
		}
		return false;
	}

	bool IsGtsBusy(Actor* actor) {
		auto profiler = Profilers::Profile("ActorUtils: IsGtsBusy");
		bool GTSBusy;
		actor->GetGraphVariableBool("GTS_Busy", GTSBusy);
		return GTSBusy;
	}

	bool IsCameraEnabled(Actor* actor) {
		bool Camera;
		actor->GetGraphVariableBool("GTS_VoreCamera", Camera);
		return Camera;
	}

	bool IsCrawlVoring(Actor* actor) {
		bool Voring;
		actor->GetGraphVariableBool("GTS_IsCrawlVoring", Voring);
		return Voring;//Voring;
	}

	bool IsButtCrushing(Actor* actor) {
		bool ButtCrushing;
		actor->GetGraphVariableBool("GTS_IsButtCrushing", ButtCrushing);
		return ButtCrushing;//Voring;
	}

	bool ButtCrush_IsAbleToGrow(Actor* actor, float limit) {
		auto transient = Transient::GetSingleton().GetData(actor);
		if (transient) {
			return transient->ButtCrushGrowthAmount < limit;
		}
		return false;
	}

	bool IsBeingGrinded(Actor* actor) {
		auto transient = Transient::GetSingleton().GetData(actor);
		if (transient) {
			return transient->being_foot_grinded;
		}
		return false;
	}

	bool CanDoButtCrush(Actor* actor) {
		static Timer Default = Timer(30);
		static Timer UnstableGrowth = Timer(25.5);
		static Timer LoomingDoom = Timer(19.1);
		bool lvl70 = Runtime::HasPerk(actor, "ButtCrush_UnstableGrowth");
		bool lvl100 = Runtime::HasPerk(actor, "ButtCrush_LoomingDoom");
		if (lvl100) {
			return LoomingDoom.ShouldRunFrame();
		} else if (lvl70) {
			return UnstableGrowth.ShouldRunFrame();
		} else {
			return Default.ShouldRunFrame();
		}
	}

	bool GetCameraOverride(Actor* actor) {
		if (actor->formID == 0x14) {
			auto transient = Transient::GetSingleton().GetData(actor);
			if (transient) {
				return transient->OverrideCamera;
			}
			return false;
		}
		return false;
	}



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//                                 G T S   ST A T E S  O T H E R                                                                      //
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	bool IsGrowthSpurtActive(Actor* actor) {
		if (!Runtime::HasPerkTeam(actor, "GrowthOfStrength")) {
			return false;
		}
		if (HasGrowthSpurt(actor)) {
			return true;
		}
		return false;
	}

	bool HasGrowthSpurt(Actor* actor) {
		bool Growth1 = Runtime::HasMagicEffect(actor, "explosiveGrowth1");
		bool Growth2 = Runtime::HasMagicEffect(actor, "explosiveGrowth2");
		bool Growth3 = Runtime::HasMagicEffect(actor, "explosiveGrowth3");
		if (Growth1 || Growth2 || Growth3) {
			return true;
		} else {
			return false;
		}
	}

	bool InBleedout(Actor* actor) {
		return actor->AsActorState()->IsBleedingOut();
	}

	bool AllowStagger(Actor* giant, Actor* tiny) {
		if (Persistent::GetSingleton().allow_stagger == true) {
			return true; // Allow it
		} else if (Persistent::GetSingleton().allow_stagger == false && (giant->formID == 0x14 || IsTeammate(giant)) && (tiny->formID == 0x14 || IsTeammate(tiny))) {
			return false; // Protect
		}
		return true;
	}

	bool IsMechanical(Actor* actor) {
		bool dwemer = Runtime::HasKeyword(actor, "DwemerKeyword");
		return dwemer;
	}

	bool IsHuman(Actor* actor) { // Check if Actor is humanoid or not. Currently used for Hugs Animation
		bool vampire = Runtime::HasKeyword(actor, "VampireKeyword");
		bool dragon = Runtime::HasKeyword(actor, "DragonKeyword");
		bool animal = Runtime::HasKeyword(actor, "AnimalKeyword");
		bool dwemer = Runtime::HasKeyword(actor, "DwemerKeyword");
		bool undead = Runtime::HasKeyword(actor, "UndeadKeyword");
		bool creature = Runtime::HasKeyword(actor, "CreatureKeyword");
		if (!dragon && !animal && !dwemer && !undead && !creature) {
			return true; // Detect non-vampire
		}
		if (!dragon && !animal && !dwemer && !creature && undead && vampire) {
			return true; // Detect Vampire
		} else {
			return false;
		}
		return false;
	}

	bool IsBlacklisted(Actor* actor) {
		bool blacklist = Runtime::HasKeyword(actor, "BlackListKeyword");
		return blacklist;
	}

	bool IsInsect(Actor* actor, bool performcheck) {
		bool Check = Persistent::GetSingleton().AllowInsectVore;
		if (performcheck && Check) {
			return false;
		}
		bool Spider = Runtime::IsRace(actor, "FrostbiteSpiderRace");
		bool SpiderGiant = Runtime::IsRace(actor, "FrostbiteSpiderRaceGiant");
		bool SpiderLarge = Runtime::IsRace(actor, "FrostbiteSpiderRaceLarge");
		bool ChaurusReaper = Runtime::IsRace(actor, "ChaurusReaperRace");
		bool Chaurus = Runtime::IsRace(actor, "ChaurusRace");
		bool ChaurusHunterDLC = Runtime::IsRace(actor, "DLC1ChaurusHunterRace");
		bool ChaurusDLC = Runtime::IsRace(actor, "DLC1_BF_ChaurusRace");
		bool ExplSpider = Runtime::IsRace(actor, "DLC2ExpSpiderBaseRace");
		bool ExplSpiderPackMule = Runtime::IsRace(actor, "DLC2ExpSpiderPackmuleRace");
		bool AshHopper = Runtime::IsRace(actor, "DLC2AshHopperRace");
		if (Spider||SpiderGiant||SpiderLarge||ChaurusReaper||Chaurus||ChaurusHunterDLC||ChaurusDLC||ExplSpider||ExplSpiderPackMule||AshHopper) {
			return true;
		} else {
			return false;
		}
		return false;
	}

	bool IsFemale(Actor* actor) {
		/*bool FemaleCheck = false;
		if (!FemaleCheck) {
			return true; // Always return true if we don't check for male/female
		}*/
		auto base = actor->GetActorBase();
		int sex = 0;
		if (base) {
			if (base->GetSex()) {
				sex = base->GetSex();
			}
		}
		return sex == 1; // Else return sex value
	}

	bool IsDragon(Actor* actor) {
		if (Runtime::HasKeyword(actor, "DragonKeyword")) {
			return true;
		}
		if (Runtime::IsRace(actor, "dragonRace")) {
			return true;
		} else {
			return false;
		}
	}

	bool IsGiant(Actor* actor) {
		return Runtime::IsRace(actor, "GiantRace");
	}

	bool IsMammoth(Actor* actor) {
		return Runtime::IsRace(actor, "MammothRace");
	}

	bool IsLiving(Actor* actor) {
		bool IsDraugr = Runtime::HasKeyword(actor, "UndeadKeyword");
		bool IsDwemer = Runtime::HasKeyword(actor, "DwemerKeyword");
		bool IsVampire = Runtime::HasKeyword(actor, "VampireKeyword");
		if (IsVampire) {
			return true;
		}
		if (IsDraugr || IsDwemer) {
			return false;
		} else {
			return true;
		}
		return true;
	}

	bool IsUndead(Actor* actor) {
		bool IsDraugr = Runtime::HasKeyword(actor, "UndeadKeyword");
		bool Check = Persistent::GetSingleton().AllowUndeadVore;
		if (Check) {
			return false;
		}
		return IsDraugr;
	}

	bool WasReanimated(Actor* actor) { // must be called while actor is still alive, else it will return false.
		bool reanimated = false;
		auto transient = Transient::GetSingleton().GetData(actor);
		if (transient) {
			reanimated = transient->WasReanimated;
		}
		return reanimated;
	}

	bool IsEssential(Actor* actor) {
		bool essential = actor->IsEssential() && Runtime::GetBool("ProtectEssentials");
		bool teammate = IsTeammate(actor);
		bool protectfollowers = Persistent::GetSingleton().FollowerProtection;
		if (!teammate && essential) {
			return true;
		} else if (teammate && protectfollowers) {
			if (IsHostile(PlayerCharacter::GetSingleton(), actor)) {
				return false;
			} else {
				return true;
			}
		} else {
			return false;
		}
	}

	bool IsMoving(Actor* giant) {
		return giant->AsActorState()->IsSprinting() || giant->AsActorState()->IsWalking() || giant->IsRunning() || giant->IsSneaking();
	}

	bool IsHeadtracking(Actor* giant) { // Used to report True when we lock onto something, should be Player Exclusive.
	    //Currently used to fix TDM mesh issues when we lock on someone.
		//auto profiler = Profilers::Profile("ActorUtils: HeadTracking");
		bool tracking;
		if (giant->formID == 0x14) {
			giant->GetGraphVariableBool("TDM_TargetLock", tracking); // get HT value, requires newest versions of TDM to work properly
		} else {
			tracking = false;
		}
		return tracking;
	}

	bool IsHostile(Actor* giant, Actor* tiny) {
		return tiny->IsHostileToActor(giant);
	}

	bool AllowActionsWithFollowers(Actor* giant, Actor* tiny) {
		bool allow = Persistent::GetSingleton().FollowerInteractions;
		bool hostile = IsHostile(giant, tiny);
		bool Teammate = IsTeammate(tiny);
		if (hostile) {
			return true;
		} else if (giant->formID == 0x14) { // always disallow for Player
			return false;
		} else if (!Teammate) {
			return true;
		} else if (Teammate && !allow) {
			return false;
		} else {
			return true;
		}
	}

	bool AnimationsInstalled(Actor* giant) {
		bool installed;
		giant->GetGraphVariableBool("GTS_Installed", installed);
		return installed;
	}

	bool IsInGodMode(Actor* giant) {
		if (giant->formID != 0x14) {
			return false;
		}
		REL::Relocation<bool*> singleton{ RELOCATION_ID(517711, 404238) };
		return *singleton;
	}

	bool IsFreeCameraEnabled() {
		bool tfc = false;
		auto camera = PlayerCamera::GetSingleton();
		if (camera) {
			if (camera->IsInFreeCameraMode()) {
				tfc = true;
			}
		}
		return tfc;
	}

	bool SizeRaycastEnabled() {
		return Persistent::GetSingleton().SizeRaycast_Enabled;
	}

	bool IsDebugEnabled() {
		return Runtime::GetBool("EnableDebugOverlay"); // used for debug mode of collisions and such
	}



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//                                 G T S   A C T O R   F U N C T I O N S                                                              //
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	float GetActorWeight(Actor* giant, bool metric) {
		float hh = HighHeelManager::GetBaseHHOffset(giant)[2]/100;
		float scale = get_visual_scale(giant);
		float smt = 1.0;
		float totalscale = scale + (hh*0.10 * scale);
		float actorweight = 1.0 + giant->GetWeight()/300;
		float weight;
		if (metric) { // 70.1 kg as a base
			weight = 70.1 * actorweight * (totalscale * totalscale * totalscale);
		} else {
			weight = (70.1 * actorweight * (totalscale * totalscale * totalscale)) * 2.205;
		} if (HasSMT(giant)) {
			smt = 6.0;
		}
		return weight * smt;
	}

	float GetActorHeight(Actor* giant, bool metric) {
		float hh = HighHeelManager::GetBaseHHOffset(giant)[2]/100;
		float scale = get_visual_scale(giant);
		float smt = 1.0;
		float height;
		if (metric) { // 1.82 m as a base
			height = 1.82 * scale + (hh * scale); // meters
		} else {
			height = (1.82 * scale + (hh * scale)) * 3.28; // ft
		}
		return height;
	}

	float GetScaleAdjustment(Actor* tiny) {
		float sc = 1.0;
		if (tiny->formID != 0x14) { // for non player actors only, always return 1.0 in player case
			if (IsDragon(tiny)) {
				sc = 3.2;
			} else if (IsMammoth(tiny)) {
				sc = 3.0;
			} else if (IsGiant(tiny)) {
				sc = 2.0;
			}
			return sc;
		}
		return sc;
	}

	float GetRaycastStateScale(Actor* giant) {
	    // Goal is to make us effectively smaller during these checks, so RayCast won't adjust our height unless we're truly too big
		if (IsProning(giant)) {
			return 0.20;
		} else if (IsCrawling(giant)) {
			return 0.35;
		} else if (giant->IsSneaking()) {
			return 0.65;
		} else {
			return 1.0;
		}
	}

	float GetProneAdjustment() {
		auto player = PlayerCharacter::GetSingleton();
		float value = 1.0;
		if (IsCrawling(player)) {
			value = std::clamp(Runtime::GetFloat("ProneOffsetFP"), 0.10f, 1.0f);
		} if (IsProning(player)) {
			value *= 0.5;
		}
		return value;
	}


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//                                 G T S   S T A T E S  S E T S                                                                       //
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void SetBeingHeld(Actor* tiny, bool decide) {
		auto transient = Transient::GetSingleton().GetData(tiny);
		if (transient) {
			transient->being_held = decide;
		}
	}
	void SetProneState(Actor* giant, bool enable) {
		if (giant->formID == 0x14) {
			auto transient = Transient::GetSingleton().GetData(giant);
			if (transient) {
				transient->FPProning = enable;
			}
		}
	}
	void SetBetweenBreasts(Actor* actor, bool decide) {
		auto transient = Transient::GetSingleton().GetData(actor);
		if (transient) {
			transient->between_breasts = decide;
		}
	}
	void SetBeingEaten(Actor* tiny, bool decide) {
		auto transient = Transient::GetSingleton().GetData(tiny);
		if (transient) {
			transient->about_to_be_eaten = decide;
		}
	}
	void SetBeingGrinded(Actor* tiny, bool decide) {
		auto transient = Transient::GetSingleton().GetData(tiny);
		if (transient) {
			transient->being_foot_grinded = decide;
		}
	}

	void SetCameraOverride(Actor* actor, bool decide) {
		if (actor->formID == 0x14) {
			auto transient = Transient::GetSingleton().GetData(actor);
			if (transient) {
				transient->OverrideCamera = decide;
			}
		}
	}

	void SetReanimatedState(Actor* actor) {
		auto transient = Transient::GetSingleton().GetData(actor);
		if (!WasReanimated(actor)) { // disallow to override it again if it returned true a frame before
			bool reanimated = actor->AsActorState()->GetLifeState() == ACTOR_LIFE_STATE::kReanimate;
			if (transient) {
				transient->WasReanimated = reanimated;
				//Cprint("Set {} to reanimated: {}", actor->GetDisplayFullName(), reanimated);
			}
		}
	}

	void ShutUp(Actor* actor) { // Disallow them to "So anyway i've been fishing today and my dog died" while we do something to them
		if (!actor) {
			return;
		}
		auto ai = actor->GetActorRuntimeData().currentProcess;
		if (ai) {
			if (ai->high) {
				float Greeting = ai->high->greetingTimer;
				ai->high->greetingTimer = 5;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void PlayAnimation(Actor* actor, std::string_view animName) {
		actor->NotifyAnimationGraph(animName);
	}

	void TransferInventory(Actor* from, Actor* to, const float scale, bool keepOwnership, bool removeQuestItems, DamageSource Cause, bool reset) {
		std::string name = std::format("TransferItems_{}_{}", from->formID, to->formID);

		bool reanimated = false; // shall we avoid transfering items or not.
		if (Cause != DamageSource::Vored) {
			reanimated = WasReanimated(from);
		}
		// ^ we generally do not want to transfer loot in that case: 2 loot piles will spawn if actor was resurrected

		float Start = Time::WorldTimeElapsed();
		ActorHandle gianthandle = to->CreateRefHandle();
		ActorHandle tinyhandle = from->CreateRefHandle();
		bool PCLoot = Runtime::GetBool("GtsEnableLooting");
		bool NPCLoot = Runtime::GetBool("GtsNPCEnableLooting");

		if (reset) {
			StartResetTask(from); // reset actor data.
			// Used to be inside CrushManager/ShrinkToNothingManager
		}

		TaskManager::Run(name, [=](auto& progressData) {
			if (!tinyhandle) {
				return false;
			}
			if (!gianthandle) {
				return false;
			}
			auto tiny = tinyhandle.get().get();
			auto giant = gianthandle.get().get();


			if (!tiny->IsDead()) {
				KillActor(giant, tiny); // just to make sure
				return true; // try again
			} if (tiny->IsDead()) {
				float Finish = Time::WorldTimeElapsed();
				float timepassed = Finish - Start;
				if (timepassed < 0.10) {
					return true; // retry, not enough time has passed yet
				}
				TESObjectREFR* ref = skyrim_cast<TESObjectREFR*>(tiny);
				if (ref) {
					//ref->GetInventoryChanges()->InitLeveledItems();
				}
				if (giant->formID == 0x14 && !PCLoot) {
					TransferInventory_Normal(giant, tiny, removeQuestItems);
					return false;
				} if (giant->formID != 0x14 && !NPCLoot) {
					TransferInventory_Normal(giant, tiny, removeQuestItems);
					return false;
				}
				TransferInventoryToDropbox(giant, tiny, scale, removeQuestItems, Cause, reanimated);
				return false; // stop it, we started the looting of the Target.
			}
			return true;
		});
	}

	void TransferInventory_Normal(Actor* giant, Actor* tiny, bool removeQuestItems) {
		int32_t quantity = 1.0;

		for (auto &[a_object, invData]: tiny->GetInventory()) { // transfer loot
			if (a_object->GetPlayable() && a_object->GetFormType() != FormType::LeveledItem) {
				if ((!invData.second->IsQuestObject() || removeQuestItems)) {

					TESObjectREFR* ref = skyrim_cast<TESObjectREFR*>(tiny);
					if (ref) {
						quantity = ref->GetInventoryChanges()->GetItemCount(a_object); // obtain item count
					}

					tiny->RemoveItem(a_object, quantity, ITEM_REMOVE_REASON::kRemove, nullptr, giant, nullptr, nullptr);
				}
			}
		}
	}

	void Disintegrate(Actor* actor, bool script) {
		// Optional TO-DO: 11.12.2023:
		// RE SetCriticalStage function since it isn't working in dll and we have to use script hacks.
		// ^ Done!
		std::string taskname = std::format("Disintegrate_{}", actor->formID);
		auto tinyref = actor->CreateRefHandle();
		TaskManager::RunOnce(taskname, [=](auto& update) {
			if (!tinyref) {
				return;
			}
			auto tiny = tinyref.get().get();
			SetCriticalStage(tiny, 4);
		});
	}


	void UnDisintegrate(Actor* actor) {
		//actor->GetActorRuntimeData().criticalStage.reset(ACTOR_CRITICAL_STAGE::kDisintegrateEnd);
	}

	void SetRestrained(Actor* actor) {
		CallFunctionOn(actor, "Actor", "SetRestrained", true);
	}

	void SetUnRestrained(Actor* actor) {
		CallFunctionOn(actor, "Actor", "SetRestrained", false);
	}

	void SetDontMove(Actor* actor) {
		CallFunctionOn(actor, "Actor", "SetDontMove", true);
	}

	void SetMove(Actor* actor) {
		CallFunctionOn(actor, "Actor", "SetDontMove", true);
	}

	void ForceRagdoll(Actor* actor, bool forceOn) {
		if (!actor) {
			return;
		}
		auto charCont = actor->GetCharController();
		if (!charCont) {
			return;
		}
		BSAnimationGraphManagerPtr animGraphManager;
		if (actor->GetAnimationGraphManager(animGraphManager)) {
			for (auto& graph : animGraphManager->graphs) {
				if (graph) {
					if (graph->HasRagdoll()) {
						if (forceOn) {
							graph->AddRagdollToWorld();
							charCont->flags.set(CHARACTER_FLAGS::kFollowRagdoll);
						} else {
							graph->RemoveRagdollFromWorld();
							charCont->flags.reset(CHARACTER_FLAGS::kFollowRagdoll);
						}
					}
				}
			}
		}
	}


	std::vector<hkpRigidBody*> GetActorRB(Actor* actor) {
		std::vector<hkpRigidBody*> results = {};
		auto charCont = actor->GetCharController();
		if (!charCont) {
			return results;
		}

		bhkCharProxyController* charProxyController = skyrim_cast<bhkCharProxyController*>(charCont);
		bhkCharRigidBodyController* charRigidBodyController = skyrim_cast<bhkCharRigidBodyController*>(charCont);
		if (charProxyController) {
			// Player controller is a proxy one
			auto& proxy = charProxyController->proxy;
			hkReferencedObject* refObject = proxy.referencedObject.get();
			if (refObject) {
				hkpCharacterProxy* hkpObject = skyrim_cast<hkpCharacterProxy*>(refObject);

				if (hkpObject) {
					// Not sure what bodies is doing
					for (auto body: hkpObject->bodies) {
						results.push_back(body);
					}
					// // This one appears to be active during combat.
					// // Maybe used for sword swing collision detection
					// for (auto phantom: hkpObject->phantoms) {
					// 	results.push_back(phantom);
					// }
					//
					// // This is the actual shape
					// if (hkpObject->shapePhantom) {
					// 	results.push_back(hkpObject->shapePhantom);
					// }
				}
			}
		} else if (charRigidBodyController) {
			// NPCs seem to use rigid body ones
			auto& characterRigidBody = charRigidBodyController->characterRigidBody;
			hkReferencedObject* refObject = characterRigidBody.referencedObject.get();
			if (refObject) {
				hkpCharacterRigidBody* hkpObject = skyrim_cast<hkpCharacterRigidBody*>(refObject);
				if (hkpObject) {
					if (hkpObject->m_character) {
						results.push_back(hkpObject->m_character);
					}
				}
			}
		}

		return results;
	}

	void PushActorAway(TESObjectREFR* source, Actor* receiver, float afKnockBackForce) {
		if (receiver->IsDead()) {
			return;
		}
		// CallFunctionOn(source, "ObjectReference", "PushActorAway", receiver, afKnockBackForce);

		if (source) {
			auto ai = receiver->GetActorRuntimeData().currentProcess;
			if (ai) {
				if (ai->InHighProcess()) {
					if (receiver->Is3DLoaded()) {
						if (source->Is3DLoaded()) {
							NiPoint3 direction = receiver->GetPosition() - source->GetPosition();
							direction = direction / direction.Length();

							typedef void (*DefPushActorAway)(AIProcess *ai, Actor* actor, NiPoint3& direction, float force);
							REL::Relocation<DefPushActorAway> RealPushActorAway{ RELOCATION_ID(38858, 39895) };
							RealPushActorAway(ai, receiver, direction, afKnockBackForce);
						}
					}
				}
			}
		}
	}

	void PushActorAway(TESObjectREFR* source, Actor* receiver, NiPoint3 direction, float force) {
		if (receiver->IsDead()) {
			return;
		}

		if (source) {
			auto ai = receiver->GetActorRuntimeData().currentProcess;
			if (ai) {
				if (ai->InHighProcess()) {
					if (receiver->Is3DLoaded()) {
						if (source->Is3DLoaded()) {
							typedef void (*DefPushActorAway)(AIProcess *ai, Actor* actor, NiPoint3& direction, float force);
							REL::Relocation<DefPushActorAway> RealPushActorAway{ RELOCATION_ID(38858, 39895) };
							RealPushActorAway(ai, receiver, direction, force);
						}
					}
				}
			}
		}
	}

	void KnockAreaEffect(TESObjectREFR* source, float afMagnitude, float afRadius) {
		CallFunctionOn(source, "ObjectReference", "KnockAreaEffect", afMagnitude, afRadius);
	}
	void ApplyHavokImpulse_Manual(Actor* target, float afX, float afY, float afZ, float afMagnitude) {
		NiPoint3 direction = NiPoint3(afX, afY, afZ);
		//NiPoint3 niImpulse = direction * afMagnitude/direction.Length();
		//hkVector4 impulse = hkVector4(niImpulse.x, niImpulse.y, niImpulse.z, 0.0);
		hkVector4 impulse = hkVector4(afX, afY, afZ, afMagnitude);
		auto rbs = GetActorRB(target);
		for (auto rb: rbs) {
			if (rb) {
				auto& motion = rb->motion;
				motion.ApplyLinearImpulse(impulse);
			}
		}
	}
	void ApplyHavokImpulse(TESObjectREFR* target, float afX, float afY, float afZ, float afMagnitude) {
		CallFunctionOn(target, "ObjectReference", "ApplyHavokImpulse", afX, afY, afZ, afMagnitude);
	}

	void CompleteDragonQuest(Actor* tiny, bool vore) {
		auto pc = PlayerCharacter::GetSingleton();
		auto progressionQuest = Runtime::GetQuest("MainQuest");
		if (progressionQuest) {
			auto stage = progressionQuest->GetCurrentStageID();
			if (stage == 80) {
				auto transient = Transient::GetSingleton().GetData(pc);
				if (transient) {
					Cprint("Quest is Completed");
					transient->dragon_was_eaten = true;
					SpawnProgressionParticle(tiny, vore);
				}
			}
		}
	}

	float GetHighHeelsBonusDamage(Actor* actor) {
		if (!actor) {
			return false;
		}
		auto profiler = Profilers::Profile("ActorUtils: GetHHBonusDamage");
		if (Runtime::HasPerkTeam(actor, "hhBonus")) {
			return HighHeelManager::GetBaseHHOffset(actor).Length()/100;
		}
		return 0.0;
	}

	float get_distance_to_actor(Actor* receiver, Actor* target) {
		auto profiler = Profilers::Profile("ActorUtils: GetDistanceToActor");
		if (target) {
			auto point_a = receiver->GetPosition();
			auto point_b = target->GetPosition();
			auto delta = point_a - point_b;
			return delta.Length();
		}
		return 3.4028237E38; // Max float
	}

	void ApplyShake(Actor* caster, float modifier, float radius) {
		if (caster) {
			auto position = caster->GetPosition();
			ApplyShakeAtPoint(caster, modifier, position, radius);
		}
	}

	void ApplyShakeAtNode(Actor* caster, float modifier, std::string_view nodesv) {
		auto node = find_node(caster, nodesv);
		if (node) {
			ApplyShakeAtPoint(caster, modifier, node->world.translate, 1.0);
		}
	}

	void ApplyShakeAtNode(Actor* caster, float modifier, std::string_view nodesv, float radius) {
		auto node = find_node(caster, nodesv);
		if (node) {
			ApplyShakeAtPoint(caster, modifier, node->world.translate, radius);
		}
	}

	void ApplyShakeAtPoint(Actor* caster, float modifier, const NiPoint3& coords, float radius) {
		if (!caster) {
			return;
		}
		// Reciever is always PC if it is not PC we do nothing anyways
		Actor* receiver = PlayerCharacter::GetSingleton();
		if (!receiver) {
			return;
		}

		float distance = get_distance_to_camera(coords);
		float sourcesize = get_visual_scale(caster);
		float receiversize = get_visual_scale(receiver);
		float sizedifference = (sourcesize/receiversize);
		if (caster->formID == 0x14) {
			sizedifference = sourcesize;
		}


		// To Sermit: You wrote a cutoff not a falloff
		//            was this intentional?
		//
		// FYI: This is the difference
		// Falloff:
		//   |
		// I |----\
		//   |     \
		//   |______\___
		//        distance
		// Cuttoff:
		//   |
		// I |----|
		//   |    |
		//   |____|_____
		//        distance
		float cuttoff = 450 * sizedifference * radius;
		if (distance < cuttoff) {
			// To Sermit: Same value as before just with the math reduced to minimal steps
			float intensity = (sizedifference * 18.8) / distance;
			float duration = 0.25 * intensity * (1 + (sizedifference * 0.25));
			intensity = std::clamp(intensity, 0.0f, 1e8f);
			duration = std::clamp(duration, 0.0f, 1.2f);


			shake_controller(intensity*modifier, intensity*modifier, duration);
			shake_camera_at_node(coords, intensity*modifier, duration);
		}
	}

	void EnableFreeCamera() {
		auto playerCamera = PlayerCamera::GetSingleton();
		playerCamera->ToggleFreeCameraMode(false);
	}

	bool AllowDevourment() {
		return Persistent::GetSingleton().devourment_compatibility;
	}

	bool AllowFeetTracking() {
		return Persistent::GetSingleton().allow_feetracking;
	}
	bool LessGore() {
		return Persistent::GetSingleton().less_gore;
	}

	bool IsTeammate(Actor* actor) {
		if (Runtime::InFaction(actor, "FollowerFaction") || actor->IsPlayerTeammate()) {
			return true;
		}
		return false;
	}

	bool EffectsForEveryone(Actor* giant) { // determines if we want to apply size effects for literally every single actor
		if (giant->formID == 0x14) { // don't enable for Player
			return false;
		}
		bool dead = giant->IsDead();
		bool everyone = Runtime::GetBool("PreciseDamageOthers");
		if (!dead && everyone) {
			return true;
		} else {
			return false;
		}
	}

	void TrackFeet(Actor* giant, float number, bool enable) {
		if (giant->formID == 0x14) {
			if (AllowFeetTracking()) {
				auto& sizemanager = SizeManager::GetSingleton();
				sizemanager.SetActionBool(giant, enable, number);
			}
		}
	}

	void CallDevourment(Actor* giant, Actor* tiny) {
		auto progressionQuest = Runtime::GetQuest("MainQuest");
		if (progressionQuest) {
			CallFunctionOn(progressionQuest, "gtsProgressionQuest", "Devourment", giant, tiny);
		}
	}

	void CallGainWeight(Actor* giant, float value) {
		auto progressionQuest = Runtime::GetQuest("MainQuest");
		if (progressionQuest) {
			CallFunctionOn(progressionQuest, "gtsProgressionQuest", "GainWeight", giant, value);
		}
	}

	void CallVampire() {
		auto progressionQuest = Runtime::GetQuest("MainQuest");
		if (progressionQuest) {
			CallFunctionOn(progressionQuest, "gtsProgressionQuest", "SatisfyVampire");
		}
	}

	void CallHelpMessage() {
		auto progressionQuest = Runtime::GetQuest("MainQuest");
		if (progressionQuest) {
			CallFunctionOn(progressionQuest, "gtsProgressionQuest", "TrueGiantessMessage");
		}
	}

	void AddCalamityPerk() {
		auto progressionQuest = Runtime::GetQuest("MainQuest");
		if (progressionQuest) {
			CallFunctionOn(progressionQuest, "gtsProgressionQuest", "AddCalamityPerk");
		}
	}

	void AddPerkPoints(float level) {
		auto GtsSkillPerkPoints = Runtime::GetGlobal("GtsSkillPerkPoints");
		if (!GtsSkillPerkPoints) {
			return;
		}
		if (int(level) % 5 == 0) {
			Notify("You've learned a bonus perk point");
			GtsSkillPerkPoints->value += 1.0;
		}
		if (level == 20 || level == 40) {
			GtsSkillPerkPoints->value += 2.0;
		} else if (level == 60 || level == 80) {
			GtsSkillPerkPoints->value += 3.0;
		} else if (level == 100) {
			GtsSkillPerkPoints->value += 4.0;
		}
	}

	void AddStolenAttributes(Actor* giant, float value) {
		if (giant->formID == 0x14 && Runtime::HasPerk(giant, "SizeAbsorption")) {
			auto attributes = Persistent::GetSingleton().GetData(giant);
			if (attributes) {
				log::info("Adding {} to stolen attributes", value);
				attributes->stolen_attributes += value;

				if (attributes->stolen_attributes <= 0.0) {
					attributes->stolen_attributes = 0.0; // Cap it just in case
				}
			log::info("Stolen AV value: {}", attributes->stolen_attributes);
			}
		}
	}

	void AddStolenAttributesTowards(Actor* giant, ActorValue type, float value) {
		if (giant->formID == 0x14) {
			auto Persistent = Persistent::GetSingleton().GetData(giant);
			if (Persistent) {
				float& health = Persistent->stolen_health;
				float& magick = Persistent->stolen_magick;
				float& stamin = Persistent->stolen_stamin;
				float limit = 2.0 * giant->GetLevel();
				if (type == ActorValue::kHealth) {
					health += value;
					if (health >= limit) {
						health = limit;
					}
					//log::info("Adding {} to health, health: {}", value, health);
				} else if (type == ActorValue::kMagicka) {
					magick += value;
					if (magick >= limit) {
						magick = limit;
					}
					//log::info("Adding {} to magick, magicka: {}", value, magick);
				} else if (type == ActorValue::kStamina) {
					stamin += value;
					if (stamin >= limit) {
						stamin = limit;
					}
					//log::info("Adding {} to stamina, stamina: {}", value, stamin);
				}
			}
		}
	}

	float GetStolenAttributes_Values(Actor* giant, ActorValue type) {
		if (giant->formID == 0x14) {
			auto Persistent = Persistent::GetSingleton().GetData(giant);
			if (Persistent) {
				if (type == ActorValue::kHealth) {
					return Persistent->stolen_health;
				} else if (type == ActorValue::kMagicka) {
					return Persistent->stolen_magick;
				} else if (type == ActorValue::kStamina) {
					return Persistent->stolen_stamin;
				} else {
					return 0.0;
				}
			}
			return 0.0;
		}
		return 0.0;
	}

	float GetStolenAttributes(Actor* giant) {
		auto persist = Persistent::GetSingleton().GetData(giant);
		if (persist) {
			return persist->stolen_attributes;
		}
		return 0.0;
	}

	void DistributeStolenAttributes(Actor* giant, float value) {
		if (value > 0 && giant->formID == 0x14 && Runtime::HasPerk(giant, "SizeAbsorption")) { // Permamently increases random AV after shrinking and stuff
			float scale = std::clamp(get_visual_scale(giant), 0.01f, 999999.0f);
			float Storage = GetStolenAttributes(giant);
			float limit = 2.0 * giant->GetLevel();


			auto Persistent = Persistent::GetSingleton().GetData(giant);
			if (!Persistent) {
				return;
			}
			//log::info("Adding {} to attributes", value);
			float& health = Persistent->stolen_health;
			float& magick = Persistent->stolen_magick;
			float& stamin = Persistent->stolen_stamin;

			if (Storage > 0.0) {
				int Boost = rand() % 3;
				if (Boost == 0) {
					health += (value * 4);
					if (health >= limit) {
						health = limit;
					}
					//log::info("Adding {} to HP, HP {}", value * 4, health);
				} else if (Boost == 1) {
					magick += (value * 4);
					if (magick >= limit) {
						magick = limit;
					}
					//log::info("Adding {} to MP, MP {}", value * 4, magick);
				} else if (Boost >= 2) {
					stamin += (value * 4);
					if (stamin >= limit) {
						stamin = limit;
					}
					//log::info("Adding {} to SP, SP {}", value * 4, stamin);
				}
				AddStolenAttributes(giant, -value); // reduce it
			}
		}
	}

	float GetRandomBoost() {
		float rng = (rand()% 150 + 1);
		float random = rng/100;
		return random;
	}

	float GetButtCrushCost(Actor* actor) {
		float cost = 1.0;
		if (Runtime::HasPerkTeam(actor, "ButtCrush_KillerBooty")) {
			cost -= 0.15;
		}
		if (Runtime::HasPerkTeam(actor, "ButtCrush_LoomingDoom")) {
			cost -= 0.25;
		}
		if (Runtime::HasPerkTeam(actor, "SkilledGTS")) {
			float level = std::clamp(GetGtsSkillLevel() * 0.0035f, 0.0f, 0.35f);
			cost -= level;
		}
		if (IsCrawling(actor)) {
			cost *= 1.35;
		}
		return cost;
	}

	float GetAnimationSlowdown(Actor* giant) {
		if (!giant) {
			return 1.0;
		}
		float scale = get_visual_scale(giant);
		SoftPotential getspeed {
			.k = 0.142, // 0.125
			.n = 0.82, // 0.86
			.s = 1.90, // 1.12
			.o = 1.0,
			.a = 0.0,  //Default is 0
		};
		float speedmultcalc = soft_core(scale, getspeed);
		return speedmultcalc;
	}

	void DoFootstepSound(Actor* giant, float modifier, FootEvent kind, std::string_view node) {
		auto& footstep = FootStepManager::GetSingleton();
		Impact impact_data = Impact {
			.actor = giant,
			.kind = kind,
			.scale = get_visual_scale(giant) * modifier,
			.nodes = find_node(giant, node),
		};
		footstep.OnImpact(impact_data); // Play sound
	}

	void DoDustExplosion(Actor* giant, float modifier, FootEvent kind, std::string_view node) {
		auto& explosion = ExplosionManager::GetSingleton();
		Impact impact_data = Impact {
			.actor = giant,
			.kind = kind,
			.scale = get_visual_scale(giant) * modifier,
			.nodes = find_node(giant, node),
		};
		explosion.OnImpact(impact_data); // Play explosion
	}

	void SpawnParticle(Actor* actor, float lifetime, const char* modelName, const NiMatrix3& rotation, const NiPoint3& position, float scale, std::uint32_t flags, NiAVObject* target) {
		auto cell = actor->GetParentCell();
		if (cell) {
			BSTempEffectParticle::Spawn(cell, lifetime, modelName, rotation, position, scale, flags, target);
		}
	}

	void SpawnDustParticle(Actor* giant, Actor* tiny, std::string_view node, float size) {
		auto result = find_node(giant, node);
		if (result) {
			BGSExplosion* base_explosion = Runtime::GetExplosion("draugrexplosion");
			if (base_explosion) {
				NiPointer<TESObjectREFR> instance_ptr = giant->PlaceObjectAtMe(base_explosion, false);
				if (!instance_ptr) {
					return;
				}
				TESObjectREFR* instance = instance_ptr.get();
				if (!instance) {
					return;
				}
				Explosion* explosion = instance->AsExplosion();
				if (!explosion) {
					return;
				}
				explosion->SetPosition(result->world.translate);
				explosion->GetExplosionRuntimeData().radius *= 3 * get_visual_scale(tiny) * size;
				explosion->GetExplosionRuntimeData().imodRadius *= 3 * get_visual_scale(tiny) * size;
				explosion->GetExplosionRuntimeData().unkB8 = nullptr;
				explosion->GetExplosionRuntimeData().negativeVelocity *= 0.0;
				explosion->GetExplosionRuntimeData().unk11C *= 0.0;
			}
		}
	}

	void StaggerOr(Actor* giant, Actor* tiny, float power, float afX, float afY, float afZ, float afMagnitude) {
		if (tiny->IsDead()) {
			return;
		}
		if (InBleedout(tiny)) {
			return;
		}
		if (IsBeingHeld(tiny)) {
			return;
		}
		if (!AllowStagger(giant, tiny)) {
			return;
		}
		float giantSize = get_visual_scale(giant);
		float tinySize = get_visual_scale(tiny) * GetScaleAdjustment(tiny);
		if (HasSMT(giant)) {
			giantSize *= 4.0;
		}
		float sizedifference = giantSize/tinySize;
		int ragdollchance = rand() % 30 + 1.0;
		if (sizedifference > 2.8 && ragdollchance < 4.0 * sizedifference) { // Chance for ragdoll. Becomes 100% at high scales
			PushActorAway(giant, tiny, 1.0); // Ragdoll
			return;
		} else if (sizedifference > 1.25) { // Always Stagger
			tiny->SetGraphVariableFloat("staggerMagnitude", 100.00f); // Stagger actor
			tiny->NotifyAnimationGraph("staggerStart");
			return;
		}
	}

	void DoDamageEffect(Actor* giant, float damage, float radius, int random, float bonedamage, FootEvent kind, float crushmult, DamageSource Cause) {
		radius *= 1.0 + (GetHighHeelsBonusDamage(giant) * 2.5);
		if (kind == FootEvent::Left) {
			AccurateDamage::GetSingleton().DoAccurateCollisionLeft(giant, (45.0 * damage), radius, random, bonedamage, crushmult, Cause);
		}
		if (kind == FootEvent::Right) {
			AccurateDamage::GetSingleton().DoAccurateCollisionRight(giant, (45.0 * damage), radius, random, bonedamage, crushmult, Cause);
			//                                                                                         ^        ^           ^ - - - - Normal Crush
			//                                                               Chance to trigger bone crush   Damage of            Threshold multiplication
			//                                                                                             Bone Crush
		}
	}

	void PushTowards(Actor* giantref, Actor* tinyref, NiAVObject* bone, float power, bool sizecheck) {
		NiPoint3 startCoords = bone->world.translate;
		double startTime = Time::WorldTimeElapsed();
		ActorHandle tinyHandle = tinyref->CreateRefHandle();
		ActorHandle gianthandle = giantref->CreateRefHandle();
		PushActorAway(giantref, tinyref, 1);
		// Do this next frame (or rather until some world time has elapsed)
		TaskManager::Run([=](auto& update){
			Actor* giant = gianthandle.get().get();
			Actor* tiny = tinyHandle.get().get();
			if (!giant) {
				return false;
			}
			if (!tiny) {
				return false;
			}

			NiPoint3 endCoords = bone->world.translate;
			double endTime = Time::WorldTimeElapsed();


			if ((endTime - startTime) > 1e-4) {
				// Time has elapsed

				NiPoint3 vector = endCoords- startCoords;
				float distanceTravelled = vector.Length();
				float timeTaken = endTime - startTime;
				float speed = distanceTravelled / timeTaken;
				NiPoint3 direction = vector / vector.Length();
				if (sizecheck) {
					float giantscale = get_visual_scale(giant);
					float tinyscale = get_visual_scale(tiny);
					if (HasSMT(giant)) {
						giantscale *= 6.0;
					}
					float sizedifference = giantscale/tinyscale;

					if (sizedifference < 1.2) {
						return false; // terminate task
					} else if (sizedifference > 1.2 && sizedifference < 3.0) {
						tiny->SetGraphVariableFloat("staggerMagnitude", 100.00f); // Stagger actor
						tiny->NotifyAnimationGraph("staggerStart");
						return false; //Only Stagger
					}
				}
				// If we pass checks, launch actor instead
				TESObjectREFR* tiny_is_object = skyrim_cast<TESObjectREFR*>(tiny);
				if (tiny_is_object) {
					ApplyHavokImpulse(tiny_is_object, direction.x, direction.y, direction.z, speed * 2.0 * power);
				}
				return false;
			} else {
				return true;
			}
		});
	}

	void PushForward(Actor* giantref, Actor* tinyref, float power) {
		double startTime = Time::WorldTimeElapsed();
		ActorHandle tinyHandle = tinyref->CreateRefHandle();
		ActorHandle gianthandle = giantref->CreateRefHandle();
		std::string taskname = std::format("PushOther {}", tinyref->formID);
		PushActorAway(giantref, tinyref, 1);
		TaskManager::Run(taskname, [=](auto& update) {
			Actor* giant = gianthandle.get().get();
			Actor* tiny = tinyHandle.get().get();
			if (!giant) {
				return false;
			}
			if (!tiny) {
				return false;
			}

      		auto playerRotation = giant->GetCurrent3D()->world.rotate;
			RE::NiPoint3 localForwardVector{ 0.f, 1.f, 0.f };
      		RE::NiPoint3 globalForwardVector = playerRotation * localForwardVector;

			RE::NiPoint3 direction = globalForwardVector;
			log::info("{} direction: {}", giant->GetDisplayFullName(), Vector2Str(direction));
			double endTime = Time::WorldTimeElapsed();

			if ((endTime - startTime) > 0.08) {
				// Time has elapsed
				TESObjectREFR* tiny_as_object = skyrim_cast<TESObjectREFR*>(tiny);
				if (tiny_as_object) {
					ApplyHavokImpulse(tiny_as_object, direction.x, direction.y, direction.z, power);
				}
				return false;
			} else {
				return true;
			}
		});
	}

	void TinyCalamityExplosion(Actor* giant, float radius) { // Meant to just stagger actors
		if (!giant) {
			return;
		}
		auto node = find_node(giant, "NPC Root [Root]");
		if (!node) {
			return;
		}
		float giantScale = get_visual_scale(giant);
		NiPoint3 NodePosition = node->world.translate;
		const float maxDistance = radius;
		float totaldistance = maxDistance * giantScale;
		// Make a list of points to check
		if (IsDebugEnabled() && (giant->formID == 0x14 || IsTeammate(giant))) {
			DebugAPI::DrawSphere(glm::vec3(NodePosition.x, NodePosition.y, NodePosition.z), totaldistance, 600, {0.0, 1.0, 0.0, 1.0});
		}

		NiPoint3 giantLocation = giant->GetPosition();

		for (auto otherActor: find_actors()) {
			if (otherActor != giant) {
				NiPoint3 actorLocation = otherActor->GetPosition();
				if ((actorLocation-giantLocation).Length() < (maxDistance*giantScale * 3.0)) {
					int nodeCollisions = 0;
					float force = 0.0;
					auto model = otherActor->GetCurrent3D();
					if (model) {
						VisitNodes(model, [&nodeCollisions, &force, NodePosition, totaldistance](NiAVObject& a_obj) {
							float distance = (NodePosition - a_obj.world.translate).Length();
							if (distance < totaldistance) {
								nodeCollisions += 1;
								force = 1.0 - distance / totaldistance;
								return false;
							}
							return true;
						});
					}
					if (nodeCollisions > 0) {
						float sizedifference = giantScale/get_visual_scale(otherActor);
						if (sizedifference <= 1.6) {
							StaggerActor(otherActor);
						} else {
							PushActorAway(giant, otherActor, 1.0 * GetLaunchPower(sizedifference));
						}
					}
				}
			}
		}
	}

	void ShrinkOutburst_Shrink(Actor* giant, Actor* tiny, float shrink, float gigantism) {
		if (IsEssential(tiny)) { // Protect followers/essentials
			return;
		}
		bool DarkArts1 = Runtime::HasPerk(giant, "DarkArts_Aug");
		bool DarkArts2 = Runtime::HasPerk(giant, "DarkArts_Aug2");

		float shrinkpower = (shrink * 0.70) * (1.0 + (GetGtsSkillLevel() * 0.005)) * CalcEffeciency(giant, tiny, true);

		float Adjustment = GetScaleAdjustment(tiny);
		float giantScale = get_visual_scale(giant);
		float tinyScale = get_visual_scale(tiny);

		float sizedifference = giantScale/tinyScale;
		if (DarkArts1) {
			giant->AsActorValueOwner()->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, 8.0);
		}
		if (DarkArts2 && (IsGrowthSpurtActive(giant) || HasSMT(giant))) {
			shrinkpower *= 1.40;
		}

		mod_target_scale(tiny, -(shrinkpower * gigantism));
		Attacker(giant, tiny);
		//StartCombat(giant, tiny, true);

		AdjustGtsSkill((shrinkpower * gigantism) * 0.80, giant);

		float MinScale = 0.11/Adjustment;

		if (get_target_scale(tiny) <= MinScale) {
			set_target_scale(tiny, MinScale);
		}
		if (sizedifference <= 4.0) { // Stagger or Push
			StaggerActor(tiny);
		} else {
			PushActorAway(giant, tiny, 1.0/Adjustment * GetLaunchPower(sizedifference));
		}
	}

	void ShrinkOutburstExplosion(Actor* giant, bool WasHit) {
		if (!giant) {
			return;
		}
		auto node = find_node(giant, "NPC Pelvis [Pelv]");
		if (!node) {
			return;
		}
		NiPoint3 NodePosition = node->world.translate;

		float giantScale = get_visual_scale(giant);
		float gigantism = 1.0 + SizeManager::GetSingleton().GetEnchantmentBonus(giant)*0.01;
		float shrink = 0.38;
		float radius = 1.0;

		float explosion = 0.75;
		bool DarkArts1 = Runtime::HasPerk(giant, "DarkArts_Aug");
		if (WasHit) {
			radius *= 1.4;
			shrink += 0.20;
			explosion += 0.95;
		}
		if (DarkArts1) {
			radius *= 1.33;
			shrink *= 1.33;
			explosion += 0.30;
		}

		const float BASE_DISTANCE = 84.0;
		float CheckDistance = BASE_DISTANCE*giantScale*gigantism*radius;

		Runtime::PlaySoundAtNode("ShrinkOutburstSound", giant, explosion, 1.0, "NPC Pelvis [Pelv]");
		GRumble::For("ShrinkOutburst", giant, 20.0, 0.15, "NPC COM [COM ]", 0.60);

		SpawnParticle(giant, 6.00, "GTS/Shouts/ShrinkOutburst.nif", NiMatrix3(), NodePosition, giantScale*explosion*3.0, 7, nullptr); // Spawn effect

		if (IsDebugEnabled() && (giant->formID == 0x14 || IsTeammate(giant))) {
			DebugAPI::DrawSphere(glm::vec3(NodePosition.x, NodePosition.y, NodePosition.z), CheckDistance, 600, {0.0, 1.0, 0.0, 1.0});
		}

		NiPoint3 giantLocation = giant->GetPosition();
		for (auto otherActor: find_actors()) {
			if (otherActor != giant) {
				float tinyScale = get_visual_scale(otherActor);
				NiPoint3 actorLocation = otherActor->GetPosition();
				if ((actorLocation - giantLocation).Length() < BASE_DISTANCE*giantScale*radius*3) {
					int nodeCollisions = 0;
					float force = 0.0;

					auto model = otherActor->GetCurrent3D();

					if (model) {
						VisitNodes(model, [&nodeCollisions, &force, NodePosition, CheckDistance](NiAVObject& a_obj) {
							float distance = (NodePosition - a_obj.world.translate).Length();
							if (distance < CheckDistance) {
								nodeCollisions += 1;
								force = 1.0 - distance / CheckDistance;
								return false;
							}
							return true;
						});
					}
					if (nodeCollisions > 0) {
						ShrinkOutburst_Shrink(giant, otherActor, shrink, gigantism);
					}
				}
			}
		}
	}


	bool HasSMT(Actor* giant) {
		if (Runtime::HasMagicEffect(giant, "SmallMassiveThreat")) {
			return true;
		}
		return false;
	}

	void TiredSound(Actor* player, std::string_view message) {
		if (player->formID != 0x14) {
			return;
		}
		if (IsFirstPerson()) {
			return;
		}
		static Timer Cooldown = Timer(1.2);
		if (Cooldown.ShouldRun()) {
			Runtime::PlaySound("VoreSound_Fail", player, 0.7, 0.0);
			Notify(message);
		}
	}

	hkaRagdollInstance* GetRagdoll(Actor* actor) {
		BSAnimationGraphManagerPtr animGraphManager;
		if (actor->GetAnimationGraphManager(animGraphManager)) {
			for (auto& graph : animGraphManager->graphs) {
				if (graph) {
					auto& character = graph->characterInstance;
					auto ragdollDriver = character.ragdollDriver.get();
					if (ragdollDriver) {
						auto ragdoll = ragdollDriver->ragdoll;
						if (ragdoll) {
							return ragdoll;
						}
					}
				}
			}
		}
		return nullptr;
	}

	void ManageRagdoll(Actor* tiny, float deltaLength, NiPoint3 deltaLocation, NiPoint3 targetLocation) {
		if (deltaLength >= 70.0) {
			// WARP if > 1m
			auto ragDoll = GetRagdoll(tiny);
			hkVector4 delta = hkVector4(deltaLocation.x/70.0, deltaLocation.y/70.0, deltaLocation.z/70, 1.0);
			for (auto rb: ragDoll->rigidBodies) {
				if (rb) {
					auto ms = rb->GetMotionState();
					if (ms) {
						hkVector4 currentPos = ms->transform.translation;
						hkVector4 newPos = currentPos + delta;
						rb->motion.SetPosition(newPos);
						rb->motion.SetLinearVelocity(hkVector4(0.0, 0.0, -10.0, 0.0));
					}
				}
			}
		} else {
			// Just move the hand if <1m
			std::string_view handNodeName = "NPC HAND L [L Hand]";
			auto handBone = find_node(tiny, handNodeName);
			if (handBone) {
				auto collisionHand = handBone->GetCollisionObject();
				if (collisionHand) {
					auto handRbBhk = collisionHand->GetRigidBody();
					if (handRbBhk) {
						auto handRb = static_cast<hkpRigidBody*>(handRbBhk->referencedObject.get());
						if (handRb) {
							auto ms = handRb->GetMotionState();
							if (ms) {
								hkVector4 targetLocationHavok = hkVector4(targetLocation.x/70.0, targetLocation.y/70.0, targetLocation.z/70, 1.0);
								handRb->motion.SetPosition(targetLocationHavok);
								handRb->motion.SetLinearVelocity(hkVector4(0.0, 0.0, -10.0, 0.0));
							}
						}
					}
				}
			}
		}
	}

	void StaggerActor(Actor* receiver) {
		receiver->SetGraphVariableFloat("staggerMagnitude", 100.00f);
		receiver->NotifyAnimationGraph("staggerStart");
	}

	float GetMovementModifier(Actor* giant) {
		float modifier = 1.0;
		if (giant->AsActorState()->IsSprinting()) {
			modifier *= 1.33;
		} if (giant->AsActorState()->IsWalking()) {
			modifier *= 0.75;
		} if (giant->AsActorState()->IsSneaking()) {
			modifier *= 0.75;
		}
		return modifier;
	}

	float GetGtsSkillLevel() {
		auto GtsSkillLevel = Runtime::GetGlobal("GtsSkillLevel");
		return GtsSkillLevel->value;
	}

	float GetXpBonus() {
		float xp = Persistent::GetSingleton().experience_mult;
		return xp;
	}

	void AddSMTDuration(Actor* actor, float duration) {
		if (!HasSMT(actor)) {
			return;
		}
		if (Runtime::HasPerk(actor, "EternalCalamity")) {
			auto transient = Transient::GetSingleton().GetData(actor);
			if (transient) {
				transient->SMT_Bonus_Duration += duration;
			}
		}
	}

	void AddSMTPenalty(Actor* actor, float penalty) {
		auto transient = Transient::GetSingleton().GetData(actor);
		if (transient) {
			transient->SMT_Penalty_Duration += penalty;
		}
	}

	void PrintDeathSource(Actor* giant, Actor* tiny, DamageSource cause) {
		ReportDeath(giant, tiny, cause);
	}

	void PrintSuffocate(Actor* pred, Actor* prey) {
		int random = rand() % 6;
		if (random <= 1) {
			Cprint("{} was slowly smothered between {} thighs", prey->GetDisplayFullName(), pred->GetDisplayFullName());
		} else if (random == 2) {
			Cprint("{} was suffocated by the thighs of {}", prey->GetDisplayFullName(), pred->GetDisplayFullName());
		} else if (random == 3) {
			Cprint("Thighs of {} suffocated {} to death", pred->GetDisplayFullName(), prey->GetDisplayFullName());
		} else if (random == 4) {
			Cprint("{} got smothered between the thighs of {}", prey->GetDisplayFullName(), pred->GetDisplayFullName());
		} else if (random >= 5) {
			Cprint("{} lost life to the thighs of {}", prey->GetDisplayFullName(), pred->GetDisplayFullName());
		}
	}

	void ShrinkUntil(Actor* giant, Actor* tiny, float expected) {
		if (HasSMT(giant)) {
			float Adjustment = GetScaleAdjustment(tiny);
			float predscale = get_target_scale(giant);
			float preyscale = get_target_scale(tiny) * Adjustment;
			expected *= Adjustment;
			float targetScale = predscale/expected;
			if (preyscale >= targetScale) { // Apply ONLY if target is bigger than requirement
				set_target_scale(tiny, targetScale);
				AddSMTPenalty(giant, 5.0 * GetScaleAdjustment(tiny));
			}
		}
	}

	void DisableCollisions(Actor* actor, TESObjectREFR* otherActor) {
		if (actor) {
			auto trans = Transient::GetSingleton().GetData(actor);
			if (trans) {
				trans->disable_collision_with = otherActor;
				auto colliders = ActorCollisionData(actor);
				colliders.UpdateCollisionFilter();
				if (otherActor) {
					Actor* asOtherActor = skyrim_cast<Actor*>(otherActor);
					auto otherColliders = ActorCollisionData(asOtherActor);
					otherColliders.UpdateCollisionFilter();
				}
			}
		}
	}
	void EnableCollisions(Actor* actor) {
		if (actor) {
			auto trans = Transient::GetSingleton().GetData(actor);
			if (trans) {
				auto otherActor = trans->disable_collision_with;
				trans->disable_collision_with = nullptr;
				auto colliders = ActorCollisionData(actor);
				colliders.UpdateCollisionFilter();
				if (otherActor) {
					Actor* asOtherActor = skyrim_cast<Actor*>(otherActor);
					auto otherColliders = ActorCollisionData(asOtherActor);
					otherColliders.UpdateCollisionFilter();
				}
			}
		}
	}

	void SpringGrow(Actor* actor, float amt, float halfLife, std::string_view naming) {
		if (!actor) {
			return;
		}

		auto growData = std::make_shared<SpringGrowData>(actor, amt, halfLife);
		std::string name = std::format("SpringGrow {}: {}", naming, actor->formID);
		const float DURATION = halfLife * 3.2;

		TaskManager::RunFor(DURATION,
		                    [ growData ](const auto& progressData) {
			float totalScaleToAdd = growData->amount.value;
			float prevScaleAdded = growData->addedSoFar;
			float deltaScale = totalScaleToAdd - prevScaleAdded;
			Actor* actor = growData->actor.get().get();

			if (actor) {
				float stamina = clamp(0.05, 1.0, GetStaminaPercentage(actor));
				DamageAV(actor, ActorValue::kStamina, 0.55 * (get_visual_scale(actor) * 0.5 + 0.5) * stamina * TimeScale());
				auto actorData = Persistent::GetSingleton().GetData(actor);
				if (actorData) {
					actorData->target_scale += deltaScale;
					actorData->visual_scale += deltaScale;
					growData->addedSoFar = totalScaleToAdd;
				}
			}
			return fabs(growData->amount.value - growData->amount.target) > 1e-4;
		}
		                    );
	}

	void SpringGrow_Free(Actor* actor, float amt, float halfLife, std::string_view naming) {
		if (!actor) {
			return;
		}

		auto growData = std::make_shared<SpringGrowData>(actor, amt, halfLife);
		std::string name = std::format("SpringGrow_Free {}: {}", naming, actor->formID);
		const float DURATION = halfLife * 3.2;

		TaskManager::RunFor(name, DURATION,
		                    [ growData ](const auto& progressData) {
			float totalScaleToAdd = growData->amount.value;
			float prevScaleAdded = growData->addedSoFar;
			float deltaScale = totalScaleToAdd - prevScaleAdded;
			Actor* actor = growData->actor.get().get();

			if (actor) {
				auto actorData = Persistent::GetSingleton().GetData(actor);
				if (actorData) {
					actorData->target_scale += deltaScale;
					actorData->visual_scale += deltaScale;
					growData->addedSoFar = totalScaleToAdd;
				}
			}
			return fabs(growData->amount.value - growData->amount.target) > 1e-4;
		}
		                    );
	}

	void SpringShrink(Actor* actor, float amt, float halfLife, std::string_view naming) {
		if (!actor) {
			return;
		}

		auto growData = std::make_shared<SpringShrinkData>(actor, amt, halfLife);
		std::string name = std::format("SpringShrink {}: {}", naming, actor->formID);
		const float DURATION = halfLife * 3.2;
		TaskManager::RunFor(DURATION,
		                    [ growData ](const auto& progressData) {
			float totalScaleToAdd = growData->amount.value;
			float prevScaleAdded = growData->addedSoFar;
			float deltaScale = totalScaleToAdd - prevScaleAdded;
			Actor* actor = growData->actor.get().get();

			if (actor) {
				float stamina = clamp(0.05, 1.0, GetStaminaPercentage(actor));
				DamageAV(actor, ActorValue::kStamina, 0.35 * (get_visual_scale(actor) * 0.5 + 0.5) * stamina * TimeScale());
				auto actorData = Persistent::GetSingleton().GetData(actor);
				if (actorData) {
					actorData->target_scale += deltaScale;
					actorData->visual_scale += deltaScale;
					growData->addedSoFar = totalScaleToAdd;
				}
			}

			return fabs(growData->amount.value - growData->amount.target) > 1e-4;
		}
		                    );
	}

	void ResetGrab(Actor* giant) {
		if (giant->formID == 0x14 || IsTeammate(giant)) {
			AnimationManager::StartAnim("GrabAbort", giant); // Abort Grab animation
			AnimationManager::StartAnim("TinyDied", giant);

			giant->SetGraphVariableInt("GTS_GrabbedTiny", 0); // Tell behaviors 'we have nothing in our hands'. A must.
			giant->SetGraphVariableInt("GTS_Grab_State", 0);
			giant->SetGraphVariableInt("GTS_Storing_Tiny", 0);
			SetBetweenBreasts(giant, false);
		}
	}

	void FixAnimations() { // Fixes Animations for GTS Grab Actions
		auto profiler = Profilers::Profile("Utils: Actor State Fix");
		for (auto giant: find_actors()) {
			if (!giant) {
				continue;
			}
			int StateID;
			int GTSStateID;

			giant->GetGraphVariableInt("currentDefaultState", StateID);
			giant->GetGraphVariableInt("GTS_Def_State", GTSStateID);

			ResetGrab(giant);
			if (GTSStateID != StateID) {
				giant->SetGraphVariableInt("GTS_Def_State", StateID);
			}
		}
	}

	NiPoint3 GetContainerSpawnLocation(Actor* giant, Actor* tiny) {
		bool success_first = false;
		bool success_second = false;
		NiPoint3 ray_start = tiny->GetPosition();
		ray_start.z += 40.0; // overrize .z with giant .z + 40, so ray starts from above
		NiPoint3 ray_direction(0.0, 0.0, -1.0);

		float ray_length = 40000;

		NiPoint3 pos = NiPoint3(0, 0, 0); // default pos
		NiPoint3 endpos = CastRayStatics(tiny, ray_start, ray_direction, ray_length, success_first);
		if (success_first) {
			// attempt 1, spawn at actor ray cast.
			// Obtain actor coords, shift Z by 40 and cast ray down. That way we always do ray at around ground level.
			log::info("Cast 1 success");
			return endpos;
		} else if (!success_first) {
			NiPoint3 ray_start_second = giant->GetPosition();
			ray_start_second.z += 40.0;
			pos = CastRayStatics(giant, ray_start_second, ray_direction, ray_length, success_second);
			log::info("Cast 1 fail, trying again");
			if (!success_second) {
				// failed, spawn at giant location directly.
				log::info("Cast 2 failed, spawning on default GTS location");
				pos = giant->GetPosition();
				return pos;
			}
			return pos;
		}
		log::info("Everything failed, spawning nowhere");
		return pos;
	}

	// From an actor place a new container at them and transfer
	// all of their inventory into it
	void TransferInventoryToDropbox(Actor* giant, Actor* actor, const float scale, bool removeQuestItems, DamageSource Cause, bool Resurrected) {
		bool soul = false;
		float Scale = std::clamp(scale, 0.40f, 4.4f);

		if (Resurrected) {
			Cprint("Task Aborted, target was resurrected");
			return;
		}

		std::string_view container;
		std::string name = std::format("{} remains", actor->GetDisplayFullName());

		if (IsMechanical(actor)) {
			container = "Dropbox_Mechanical";
		} else if (Cause == DamageSource::Vored) { // Always spawn soul on vore
			container = "Dropbox_Soul";
			name = std::format("{} Soul Remains", actor->GetDisplayFullName());
			soul = true;
		} else if (LessGore()) { // Spawn soul if Less Gore is on
			container = "Dropbox_Soul";
			name = std::format("Crushed Soul of {} ", actor->GetDisplayFullName());
			soul = true;
		} else if (IsInsect(actor, false)) {
			container = "Dropbox_Bug";
			name = std::format("Remains of {}", actor->GetDisplayFullName());
		} else if (IsLiving(actor)) {
			container = "Dropbox_Physics";
		} else {
			container = "Dropbox_Undead_Physics";
		}

		if (IsDragon(actor)) { // These affect the scale of dropbox visuals
			Scale *= 3.2;
		} else if (IsGiant(actor)) {
			Scale *= 2.0;
		} else if (IsMammoth(actor)) {
			Scale *= 5.0;
		}
		/*NiPoint3 TinyPos = actor->GetPosition();
		NiPoint3 GiantPos = giant->GetPosition();
		NiPoint3 TotalPos = NiPoint3(TinyPos.x, TinyPos.y, GiantPos.z + 170);
		*/

		NiPoint3 TotalPos = GetContainerSpawnLocation(giant, actor); // obtain goal of container position by doing ray-cast
		if (IsDebugEnabled()) {
			DebugAPI::DrawSphere(glm::vec3(TotalPos.x, TotalPos.y, TotalPos.z), 8.0, 6000, {1.0, 1.0, 0.0, 1.0});
		}

		auto dropbox = Runtime::PlaceContainerAtPos(actor, TotalPos, container); // Place chosen container

		std::string taskname = std::format("Dropbox {}", actor->formID); // create task name for main task
		std::string taskname_sound = std::format("DropboxAudio {}", actor->formID); // create task name for Audio play
		if (!dropbox) {
			return;
		}
		float Start = Time::WorldTimeElapsed();
		dropbox->SetDisplayName(name, false); // Rename container to match chosen name

		ObjectRefHandle dropboxHandle = dropbox->CreateRefHandle();
			TaskManager::RunFor(taskname, 16, [=](auto& progressData) { // Spawn loot piles
				float Finish = Time::WorldTimeElapsed();
				auto dropboxPtr = dropboxHandle.get().get();
				if (!dropboxPtr) {
					return false;
				}
				if (!dropboxPtr->Is3DLoaded()) {
					return true;
				}
				auto dropbox3D = dropboxPtr->GetCurrent3D();
				if (!dropbox3D) {
					return true; // Retry next frame
				} else {
					float timepassed = Finish - Start;
					if (soul) {
						timepassed *= 1.33; // faster soul scale
					}
					auto node = find_object_node(dropboxPtr, "GorePile_Obj");
					auto trigger = find_object_node(dropboxPtr, "Trigger_Obj");
					if (node) {
						node->local.scale = (Scale * 0.33) + (timepassed*0.18);
						if (!soul) {
							node->world.translate.z = TotalPos.z;
						}
						update_node(node);
					}
					if (trigger) {
						trigger->local.scale = (Scale * 0.33) + (timepassed*0.18);
						if (!soul) {
							trigger->world.translate.z = TotalPos.z;
						}
						update_node(trigger);
					}
					if (node && node->local.scale >= Scale) { // disable collision once it is scaled enough
						//dropbox3D->SetMotionType(4, true, true, true);
						//dropbox3D->SetCollisionLayer(COL_LAYER::kNonCollidable);
						return false; // End task
					}
					return true;
				}
    		});
			if (Cause == DamageSource::Overkill) { // Play audio that won't disappear if source of loot transfer is Overkill
				TaskManager::RunFor(taskname_sound, 6, [=](auto& progressData) {
					auto dropboxPtr = dropboxHandle.get().get();
					if (!dropboxPtr) {
						return false; // cancel
					}
					if (!dropboxPtr->Is3DLoaded()) {
						return true; // retry
					}
					auto dropbox3D = dropboxPtr->GetCurrent3D();
					if (!dropbox3D) {
						return true; // Retry next frame
					} else {
						Runtime::PlaySound("GtsCrushSound", dropboxPtr, 1.0, 1.0);
						return false;
					}
				});
			}
		for (auto &[a_object, invData]: actor->GetInventory()) { // transfer loot
			int32_t quantity = 1.0;
			if (a_object->GetPlayable() && a_object->GetFormType() != FormType::LeveledItem) { // We don't want to move Leveled Items
				if ((!invData.second->IsQuestObject() || removeQuestItems)) {

					TESObjectREFR* ref = skyrim_cast<TESObjectREFR*>(actor);
					if (ref) {
						quantity = ref->GetInventoryChanges()->GetItemCount(a_object); // obtain item count
					}

					//log::info("Transfering item: {}, quantity: {}", a_object->GetName(), quantity);

					actor->RemoveItem(a_object, quantity, ITEM_REMOVE_REASON::kRemove, nullptr, dropbox, nullptr, nullptr);
				}
			}
		}
	}

	bool CanPerformAnimation(Actor* giant, float type) { // Needed for smooth animation unlocks during quest progression
		// 0 = Hugs
		// 1 = stomps and kicks
		// 2 = Grab and Sandwich
		// 3 = Vore
		// 5 = Others
		if (giant->formID != 0x14) {
			return true;
		} else {
			auto progressionQuest = Runtime::GetQuest("MainQuest");
			if (progressionQuest) {
				auto queststage = progressionQuest->GetCurrentStageID();
				if (queststage >= 10 && type == 0) {
					return true; // allow hugs
				} else if (queststage >= 30 && type == 1) {
					return true; // allow stomps and kicks
				} else if (queststage >= 50 && type == 2) {
					return true; // Allow grabbing and sandwiching
				} else if (queststage >= 60 && type >= 3) {
					return true; // Allow Vore
				} else {
					Notify("You're not experienced to perform this action");
					return false;
				}
			}
			Notify("You're not experienced to perform this action");
			return false;
		}
	}

	void AdvanceQuestProgression(Actor* giant, float stage, float value) {
		if (giant->formID == 0x14) { // Player Only
			auto progressionQuest = Runtime::GetQuest("MainQuest");
			if (progressionQuest) {
				auto queststage = progressionQuest->GetCurrentStageID();
				if (queststage < 10 || queststage >= 100) {
					return;
				}
				if (stage == 1) {
					Persistent::GetSingleton().HugStealCount += value;
				} else if (stage == 2) {
					Persistent::GetSingleton().StolenSize += value;
				} else if (stage == 3 && queststage >= 30) {
					Persistent::GetSingleton().CrushCount += value;
				} else if (stage == 4 && queststage >= 40) {
					Persistent::GetSingleton().STNCount += value;
				} else if (stage == 5) {
					Persistent::GetSingleton().HandCrushed += value;
				} else if (stage == 6) {
					Persistent::GetSingleton().VoreCount += value;
				} else if (stage == 7) {
					Persistent::GetSingleton().GiantCount += value;
				}
			}
		}
	}

	void AdvanceQuestProgression(Actor* giant, Actor* tiny, float stage, float value, bool vore) {
		if (giant->formID == 0x14) { // Player Only
			auto progressionQuest = Runtime::GetQuest("MainQuest");
			if (progressionQuest) {
				auto queststage = progressionQuest->GetCurrentStageID();
				if (queststage < 10 || queststage >= 100) {
					return;
				}
				float bonus = GetScaleAdjustment(tiny);
				if (stage == 1) {
					Persistent::GetSingleton().HugStealCount += value;
				} else if (stage == 2) {
					Persistent::GetSingleton().StolenSize += value;
				} else if (stage == 3 && queststage >= 30) {
					Persistent::GetSingleton().CrushCount += value * bonus;
					SpawnProgressionParticle(tiny, false);
				} else if (stage == 4 && queststage >= 40) {
					Persistent::GetSingleton().STNCount += value * bonus;
					SpawnProgressionParticle(tiny, false);
				} else if (stage == 5) {
					Persistent::GetSingleton().HandCrushed += value * bonus;
					SpawnProgressionParticle(tiny, false);
				} else if (stage == 6) {
					Persistent::GetSingleton().VoreCount += value * bonus;
					SpawnProgressionParticle(tiny, true);
				} else if (stage == 7) {
					Persistent::GetSingleton().GiantCount += value;
					SpawnProgressionParticle(tiny, vore);
				}
			}
		}
	}

	void ResetQuest() {
		Persistent::GetSingleton().HugStealCount = 0.0;
		Persistent::GetSingleton().StolenSize = 0.0;
		Persistent::GetSingleton().CrushCount = 0.0;
		Persistent::GetSingleton().STNCount = 0.0;
		Persistent::GetSingleton().HandCrushed = 0.0;
		Persistent::GetSingleton().VoreCount = 0.0;
		Persistent::GetSingleton().GiantCount = 0.0;
	}


	void SpawnProgressionParticle(Actor* tiny, bool vore) {
		float scale = 1.0 * GetScaleAdjustment(tiny);
		auto node = find_node(tiny, "NPC Root [Root]");
		log::info("Spawning particle");
		if (node) {
			NiPoint3 pos = node->world.translate;
			if (!vore) {
				SpawnParticle(tiny, 4.60, "GTS/Magic/Life_Drain.nif", NiMatrix3(), pos, scale, 7, nullptr);
				log::info("Soul false, spawning particle");
			} else {
				SpawnParticle(tiny, 4.60, "GTS/Magic/Soul_Drain.nif", NiMatrix3(), pos, scale, 7, nullptr);
				log::info("Soul true, spawning particle");
			}
		}
	}

	float GetQuestProgression(float stage) {
		if (stage == 1) {
			return Persistent::GetSingleton().HugStealCount;
		} else if (stage == 2) {
			return Persistent::GetSingleton().StolenSize;
		} else if (stage == 3) {
			return Persistent::GetSingleton().CrushCount;
		} else if (stage == 4) {
			return (Persistent::GetSingleton().CrushCount - 3.0) + Persistent::GetSingleton().STNCount;
		} else if (stage == 5) {
			return Persistent::GetSingleton().HandCrushed;
		} else if (stage == 6) {
			return Persistent::GetSingleton().VoreCount;
		} else if (stage == 7) {
			return Persistent::GetSingleton().GiantCount;
		} else {
			return 0.0;
		}
	}

	void InflictSizeDamage(Actor* attacker, Actor* receiver, float value) {
		//float resistance = AttributeManager::GetSingleton().GetAttributeBonus(receiver, ActorValue::kHealth);
		DamageAV(receiver, ActorValue::kHealth, value);
    Attacked(receiver, attacker);
	}

	void EditDetectionLevel(Actor* actor, Actor* giant) { // Unused and does nothing.
		giant = PlayerCharacter::GetSingleton();
		float scale = get_visual_scale(actor);
		auto ai = actor->GetActorRuntimeData().currentProcess;
		if (ai) {
			if (ai->high) {
				auto Array = ai->high->knowledgeArray;
				for (auto references: Array) { // Do array stuff
					auto Find = references.second; // Obtain BSTTuple.second member (first/second)
					auto DetectionStage = Find->detectionState.get(); // get detection stage of actor
					if (DetectionStage) { // Do nothing since it does nothing. Sigh.
						std::int32_t level = DetectionStage->level = 0;
						std::int32_t unk14 = DetectionStage->unk14 = 0;
						std::int32_t unk15 = DetectionStage->unk15 = 0;
						std::int32_t unk16 = DetectionStage->unk16 = 0;
						std::int32_t pad17 = DetectionStage->pad17 = 0;
						std::int32_t unk18 = DetectionStage->unk18 = 0;
						std::int32_t unk28 = DetectionStage->unk28 = 0;
						std::int32_t unk38 = DetectionStage->unk38 = 0;
					}
				}
			}
		}
	}

  // RE Fun
  void SetCriticalStage(Actor* actor, int stage) {
    if (stage < 5 && stage >= 0) {
      typedef void (*DefSetCriticalStage)(Actor* actor, int stage);
      REL::Relocation<DefSetCriticalStage> SkyrimSetCriticalStage{ RELOCATION_ID(36607, 37615) };
      SkyrimSetCriticalStage(actor, stage);
    }
  }
  void Attacked(Actor* victim, Actor* agressor) {
    typedef void (*DefAttacked)(Actor* victim, Actor* agressor);
    REL::Relocation<DefAttacked> SkyrimAttacked{ RELOCATION_ID(37672, 38626) };
    SkyrimAttacked(victim, agressor);
  }
}
