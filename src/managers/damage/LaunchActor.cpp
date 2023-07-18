#include "magic/effects/smallmassivethreat.hpp"
#include "managers/damage/AccurateDamage.hpp"
#include "managers/damage/SizeHitEffects.hpp"
#include "managers/damage/LaunchActor.hpp"
#include "managers/RipClothManager.hpp"
#include "managers/ai/aifunctions.hpp"
#include "scale/scalespellmanager.hpp"
#include "managers/GtsSizeManager.hpp"
#include "managers/CrushManager.hpp"
#include "managers/InputManager.hpp"
#include "magic/effects/common.hpp"
#include "managers/GtsManager.hpp"
#include "managers/Attributes.hpp"
#include "managers/hitmanager.hpp"
#include "managers/highheel.hpp"
#include "utils/actorUtils.hpp"
#include "data/persistent.hpp"
#include "data/transient.hpp"
#include "data/runtime.hpp"
#include "scale/scale.hpp"
#include "UI/DebugAPI.hpp"
#include "data/time.hpp"
#include "profiler.hpp"
#include "Config.hpp"
#include "timer.hpp"
#include "node.hpp"
#include <vector>
#include <string>

using namespace Gts;
using namespace RE;
using namespace SKSE;
using namespace std;

namespace {

	const std::string_view leftFootLookup = "NPC L Foot [Lft ]";
	const std::string_view rightFootLookup = "NPC R Foot [Rft ]";
	const std::string_view leftCalfLookup = "NPC L Calf [LClf]";
	const std::string_view rightCalfLookup = "NPC R Calf [RClf]";
	const std::string_view leftToeLookup = "NPC L Toe0 [LToe]";
	const std::string_view rightToeLookup = "NPC R Toe0 [RToe]";
	const std::string_view bodyLookup = "NPC Spine1 [Spn1]";

	const float LAUNCH_DAMAGE = 2.4f;
	const float LAUNCH_KNOCKBACK = 0.02f;
	const float UNDERFOOT_POWER = 0.70;

	float GetLaunchPower(float sizeRatio) {
		// https://www.desmos.com/calculator/wh0vwgljfl
		SoftPotential launch {
				.k = 1.42, 
				.n = 0.78, 
				.s = 0.6, 
				.a = 0.8, 
			};
		float power = soft_power(sizeRatio, launch);
		log::info("Launch Power: {}", power);
		return power;
	}

	void RunSTNCheckTask(Actor* giant, Actor* tiny) {
		std::string taskname = std::format("ShrinkOther_{}", tiny->formID);
		const float DURATION = 2.5;

		ActorHandle tinyHandle = tiny->CreateRefHandle();
		ActorHandle giantHandle = giant->CreateRefHandle();
		
		TaskManager::RunFor(taskname, DURATION, [=](auto& progressData){
			auto GTS = giantHandle.get().get();
			auto TINY = tinyHandle.get().get();
			if (!GTS) {
				return false;
			} if (!TINY) {
				return false;
			}
			if (ShrinkToNothing(GTS, TINY)) { //Shrink to nothing if size difference is too big
				return false; // Shrink to nothing casted, cancel Task
			}

			return true; // Everything is fine, continue checking
		});
	}

	void LaunchDecide(Actor* giant, Actor* tiny, float force, float damagebonus) {
		if (IsBeingHeld(tiny)) {
			return;
		}
		auto& accuratedamage = AccurateDamage::GetSingleton();
		float DamageSetting = Persistent::GetSingleton().size_related_damage_mult;
		float giantSize = get_visual_scale(giant);
		float SMT = 1.0;
		float threshold = 6.0;
		if (HasSMT(giant)) {
			giantSize += 4.0;
			threshold = 0.8;
			force += 0.20;
		}
		float tinySize = get_visual_scale(tiny);
		if (IsDragon(tiny)) {
			tinySize *= 2.6;
		}
		float sizeRatio = giantSize/tinySize;

		float knockBack = LAUNCH_KNOCKBACK * giantSize * force;

		auto& sizemanager = SizeManager::GetSingleton();
		bool IsLaunching = sizemanager.IsLaunching(tiny);
		if (IsLaunching) {
			return;
		}	

		if (force >= 0.10) {
			if (Runtime::HasPerkTeam(giant, "LaunchPerk")) {

				float power = 1.0;
				if (Runtime::HasPerkTeam(giant, "DisastrousTremor")) {
					power = 1.5;
					damagebonus *= 2.0;
				}

				sizemanager.GetSingleton().GetLaunchData(tiny).lastLaunchTime = Time::WorldTimeElapsed();

				if (Runtime::HasPerkTeam(giant, "LaunchDamage")) {
					float damage = LAUNCH_DAMAGE * sizeRatio * force * damagebonus;
					DamageAV(tiny, ActorValue::kHealth, damage * DamageSetting);
					if (power >= 1.5) { // Apply only when we have DisastrousTremor perk
						mod_target_scale(tiny, -(damage * DamageSetting) / 500);

						RunSTNCheckTask(giant, tiny); // Enable Shrink To Nothing check so Actor won't go into negative scale: absorb actor instead.
					}
				}
				PushActorAway(giant, tiny, 1.0);
				
				std::string name = std::format("PushOther_{}", tiny->formID);

				ActorHandle tinyHandle = tiny->CreateRefHandle();

				TaskManager::RunOnce(name, [=](auto& update){ // Possible to-do: Reverse Engineer ApplyHavokImpulse?
					if (tinyHandle) {
						TESObjectREFR* tiny_is_object = skyrim_cast<TESObjectREFR*>(tinyHandle.get().get());
						if (tiny_is_object) {
							ApplyHavokImpulse(tiny_is_object, 0, 0, 40 * GetLaunchPower(sizeRatio) * force * power, 40 * GetLaunchPower(sizeRatio) * force * power);
						}
					}
				});	
			}
		}
	}
}

namespace Gts {

	LaunchActor& LaunchActor::GetSingleton() noexcept {
		static LaunchActor instance;
		return instance;
	}

	std::string LaunchActor::DebugName() {
		return "LaunchActor";
	}

	void LaunchActor::ApplyLaunch(Actor* giant, float radius, float damagebonus, FootEvent kind) {
		if (!Runtime::HasPerkTeam(giant, "LaunchPerk")) {
			return;
		}
		if (kind == FootEvent::Left) {
			LaunchActor::GetSingleton().LaunchLeft(giant, radius, damagebonus);
		} if (kind == FootEvent::Right) {
			LaunchActor::GetSingleton().LaunchRight(giant, radius, damagebonus);
		}
	}

	void LaunchActor::LaunchLeft(Actor* giant, float radius, float damagebonus) {
		if (!giant) {
			return;
		}
		float giantScale = get_visual_scale(giant);
		const float BASE_CHECK_DISTANCE = 34.0;
		float SCALE_RATIO = 6.0 / GetMovementModifier(giant);
		if (HasSMT(giant)) {
			SCALE_RATIO = 1.2 / GetMovementModifier(giant);
			giantScale *= 2.0;
		}

		radius *= 1.0 + GetHighHeelsBonusDamage(giant) * 2.5;

		// Get world HH offset
		NiPoint3 hhOffset = HighHeelManager::GetHHOffset(giant);
		NiPoint3 hhOffsetbase = HighHeelManager::GetBaseHHOffset(giant);

		auto leftFoot = find_node(giant, leftFootLookup);
		auto leftCalf = find_node(giant, leftCalfLookup);
		auto leftToe = find_node(giant, leftToeLookup);
		auto BodyBone = find_node(giant, bodyLookup);
		if (!leftFoot) {
			return;
		}if (!leftCalf) {
			return;
		}if (!leftToe) {
			return;
		}if (!BodyBone) {
			return; // CTD protection attempts
		}
		NiMatrix3 leftRotMat;
		{
			NiAVObject* foot = leftFoot;
			NiAVObject* calf = leftCalf;
			NiAVObject* toe = leftToe;
			NiTransform inverseFoot = foot->world.Invert();
			NiPoint3 forward = inverseFoot*toe->world.translate;
			forward = forward / forward.Length();

			NiPoint3 up = inverseFoot*calf->world.translate;
			up = up / up.Length();

			NiPoint3 right = forward.UnitCross(up);
			forward = up.UnitCross(right); // Reorthonalize

			leftRotMat = NiMatrix3(right, forward, up);
		}

		float maxFootDistance = BASE_CHECK_DISTANCE * radius * giantScale;
		float hh = hhOffsetbase[2];
		// Make a list of points to check
		std::vector<NiPoint3> points = {
			NiPoint3(0.0, hh*0.08, -0.25 +(-hh * 0.25)), // The standard at the foot position
			NiPoint3(-1.6, 7.7 + (hh/70), -0.75 + (-hh * 1.15)), // Offset it forward
			NiPoint3(0.0, (hh/50), -0.25 + (-hh * 1.15)), // Offset for HH
		};
		std::tuple<NiAVObject*, NiMatrix3> left(leftFoot, leftRotMat);

		for (const auto& [foot, rotMat]: {left}) {
			std::vector<NiPoint3> footPoints = {};
			for (NiPoint3 point: points) {
				footPoints.push_back(foot->world*(rotMat*point));
			}
			if (Runtime::GetBool("EnableDebugOverlay") && (giant->formID == 0x14 || giant->IsPlayerTeammate() || Runtime::InFaction(giant, "FollowerFaction"))) {
				for (auto point: footPoints) {
					DebugAPI::DrawSphere(glm::vec3(point.x, point.y, point.z), maxFootDistance);
				}
			}

			NiPoint3 giantLocation = giant->GetPosition();

			auto cell = giant->GetParentCell();
			if (cell) {
				auto data = cell->GetRuntimeData();
				for (auto object: data.objectList) {
					if (object) {
						log::info("Checking objects");
						Actor* NonRef = skyrim_cast<Actor*>(object);
						if (!NonRef) {
							log::info("Non ref found: {}", object->GetDisplayFullName());
							NiPoint3 objectlocation = object->GetPosition();
							for (auto point: footPoints) {
								float distance = (point - objectlocation).Length();
								if (distance <= maxFootDistance) {
									float force = 1.0 - distance / maxFootDistance;
									object->ApplyCurrent(0.5, hkVector4(0,0, 65 * get_visual_scale(giant), 1.0));
									log::info("Applying Current for {}", object->GetDisplayFullName());
								}
							}
						}
					}
				}
			}
			

			for (auto otherActor: find_actors()) {
				if (otherActor != giant) {
					if (!AllowStagger(giant, otherActor)) {
						return;
					}
					float tinyScale = get_visual_scale(otherActor);
					if (giantScale / tinyScale > SCALE_RATIO/GetMovementModifier(giant)) {
						NiPoint3 actorLocation = otherActor->GetPosition();
						for (auto point: footPoints) {
							float distance = (point - actorLocation).Length();
							if (distance <= maxFootDistance) {
								float force = 1.0 - distance / maxFootDistance;//force += 1.0 - distance / maxFootDistance;
								LaunchDecide(giant, otherActor, force, damagebonus/6);
							}
						}
					}
				}
			}
		}
	}
	


	void LaunchActor::LaunchRight(Actor* giant, float radius, float damagebonus) {
		if (!giant) {
			return;
		}
		float giantScale = get_visual_scale(giant);
		const float BASE_CHECK_DISTANCE = 34.0;
		float SCALE_RATIO = 6.0 / GetMovementModifier(giant);
		if (HasSMT(giant)) {
			SCALE_RATIO = 1.2 / GetMovementModifier(giant);
			giantScale *= 2.0;
		}
		radius *= 1.0 + GetHighHeelsBonusDamage(giant) * 2.5;

		// Get world HH offset
		NiPoint3 hhOffset = HighHeelManager::GetHHOffset(giant);
		NiPoint3 hhOffsetbase = HighHeelManager::GetBaseHHOffset(giant);

		auto rightFoot = find_node(giant, rightFootLookup);
		auto rightCalf = find_node(giant, rightCalfLookup);
		auto rightToe = find_node(giant, rightToeLookup);
		auto BodyBone = find_node(giant, bodyLookup);


		if (!rightFoot) {
			return;
		}
		if (!rightCalf) {
			return;
		}
		if (!rightToe) {
			return;
		}
		if (!BodyBone) {
			return; // CTD protection attempts
		}
		NiMatrix3 rightRotMat;
		{
			NiAVObject* foot = rightFoot;
			NiAVObject* calf = rightCalf;
			NiAVObject* toe = rightToe;

			NiTransform inverseFoot = foot->world.Invert();
			NiPoint3 forward = inverseFoot*toe->world.translate;
			forward = forward / forward.Length();

			NiPoint3 up = inverseFoot*calf->world.translate;
			up = up / up.Length();

			NiPoint3 right = up.UnitCross(forward);
			forward = right.UnitCross(up); // Reorthonalize

			rightRotMat = NiMatrix3(right, forward, up);
		}

		float maxFootDistance = BASE_CHECK_DISTANCE * radius * giantScale;
		float hh = hhOffsetbase[2];
		// Make a list of points to check
		std::vector<NiPoint3> points = {
			NiPoint3(0.0, hh*0.08, -0.25 +(-hh * 0.25)), // The standard at the foot position
			NiPoint3(-1.6, 7.7 + (hh/70), -0.75 + (-hh * 1.15)), // Offset it forward
			NiPoint3(0.0, (hh/50), -0.25 + (-hh * 1.15)), // Offset for HH
		};
		std::tuple<NiAVObject*, NiMatrix3> right(rightFoot, rightRotMat);

		for (const auto& [foot, rotMat]: {right}) {
			std::vector<NiPoint3> footPoints = {};
			for (NiPoint3 point: points) {
				footPoints.push_back(foot->world*(rotMat*point));
			}
			if (Runtime::GetBool("EnableDebugOverlay") && (giant->formID == 0x14 || giant->IsPlayerTeammate() || Runtime::InFaction(giant, "FollowerFaction"))) {
				for (auto point: footPoints) {
					DebugAPI::DrawSphere(glm::vec3(point.x, point.y, point.z), maxFootDistance);
				}
			}

			NiPoint3 giantLocation = giant->GetPosition();

			auto cell = giant->GetParentCell();
			if (cell) {
				auto data = cell->GetRuntimeData();
				for (auto object: data.objectList) {
					if (object) {
						log::info("Checking objects");
						Actor* NonRef = skyrim_cast<Actor*>(object);
						if (!NonRef) {
							log::info("Non ref found: {}", object->GetDisplayFullName());
							NiPoint3 objectlocation = object->GetPosition();
							for (auto point: footPoints) {
								float distance = (point - objectlocation).Length();
								if (distance <= maxFootDistance) {
									float force = 1.0 - distance / maxFootDistance;
									object->ApplyCurrent(0.5, hkVector4(0,0, 65 * get_visual_scale(giant), 1.0));
									log::info("Applying Current for {}", object->GetDisplayFullName());
								}
							}
						}
					}
				}
			}

			for (auto otherActor: find_actors()) {
				if (otherActor != giant) {
					if (!AllowStagger(giant, otherActor)) {
						return;
					}
					float tinyScale = get_visual_scale(otherActor);
					if (giantScale / tinyScale > SCALE_RATIO/GetMovementModifier(giant)) {
						NiPoint3 actorLocation = otherActor->GetPosition();
						for (auto point: footPoints) {
							float distance = (point - actorLocation).Length();
							if (distance <= maxFootDistance) {
								float force = 1.0 - distance / maxFootDistance;//force += 1.0 - distance / maxFootDistance;
								LaunchDecide(giant, otherActor, force, damagebonus/6);
							}
						}
					}
				}
			}
		}
	}
}
