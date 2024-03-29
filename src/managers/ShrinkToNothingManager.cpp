#include "managers/ShrinkToNothingManager.hpp"
#include "managers/ai/aifunctions.hpp"
#include "utils/actorUtils.hpp"
#include "managers/Rumble.hpp"
#include "data/runtime.hpp"
#include "scale/scale.hpp"
#include "data/time.hpp"
#include "profiler.hpp"
#include "node.hpp"

#include <random>

using namespace SKSE;
using namespace RE;
using namespace REL;
using namespace Gts;

namespace Gts {
	ShrinkToNothingManager& ShrinkToNothingManager::GetSingleton() noexcept {
		static ShrinkToNothingManager instance;
		return instance;
	}

	std::string ShrinkToNothingManager::DebugName() {
		return "ShrinkToNothingManager";
	}

	void ShrinkToNothingManager::Update() {
		auto profiler = Profilers::Profile("ShrinkToNothing: Update");
		for (auto &[tinyId, data]: this->data) {
			auto tiny = TESForm::LookupByID<Actor>(tinyId);
			auto giantHandle = data.giant;
			if (!tiny) {
				continue;
			}
			if (!giantHandle) {
				continue;
			}
			auto giant = giantHandle.get().get();
			if (!giant) {
				continue;
			}

			if (data.state == ShrinkState::Healthy) {
				SetReanimatedState(tiny);
				data.state = ShrinkState::Shrinking;
			} else if (data.state == ShrinkState::Shrinking) {
				if (data.delay.ShouldRun()) {
					// Do shrink
					float currentSize = get_visual_scale(tiny);

					// Fully shrunk
					ShrinkToNothingManager::AdjustGiantessSkill(giant, tiny); // Adjust Size Matter skill
					KillActor(giant, tiny);

					if (!IsLiving(tiny)) {
						SpawnDustParticle(tiny, tiny, "NPC Root [Root]", 3.6);
					} else {
						if (!LessGore()) {
							std::random_device rd;
							std::mt19937 gen(rd());
							std::uniform_real_distribution<float> dis(-0.2, 0.2);
							auto root = find_node(tiny, "NPC Root [Root]");
							if (root) {
								SpawnParticle(tiny, 0.20, "GTS/Damage/Explode.nif", NiMatrix3(), root->world.translate, 2.0, 7, root);
								SpawnParticle(tiny, 0.20, "GTS/Damage/Explode.nif", NiMatrix3(), root->world.translate, 2.0, 7, root);
								SpawnParticle(tiny, 0.20, "GTS/Damage/Explode.nif", NiMatrix3(), root->world.translate, 2.0, 7, root);
								SpawnParticle(tiny, 1.20, "GTS/Damage/ShrinkOrCrush.nif", NiMatrix3(), root->world.translate, get_visual_scale(tiny) * 10, 7, root);

							}
							Runtime::CreateExplosion(tiny, get_visual_scale(tiny)/4, "BloodExplosion");
							Runtime::PlayImpactEffect(tiny, "GtsBloodSprayImpactSetVoreMedium", "NPC Head [Head]", NiPoint3{dis(gen), 0, -1}, 512, true, true);
							Runtime::PlayImpactEffect(tiny, "GtsBloodSprayImpactSetVoreMedium", "NPC L Foot [Lft ]", NiPoint3{dis(gen), 0, -1}, 512, true, false);
							Runtime::PlayImpactEffect(tiny, "GtsBloodSprayImpactSetVoreMedium", "NPC R Foot [Rft ]", NiPoint3{dis(gen), 0, -1}, 512, true, false);
							Runtime::PlayImpactEffect(tiny, "GtsBloodSprayImpactSetVoreMedium", "NPC Spine [Spn0]", NiPoint3{dis(gen), 0, -1}, 512, true, false);
						} else {
							Runtime::PlaySound("BloodGushSound", tiny, 1.0, 0.5);
						}
					}

          Attacked(tiny, giant);
					// ReportCrime(giant, tiny, 1000.0, true);
					// StartCombat(giant, tiny, false);

					AddSMTDuration(giant, 5.0);

					ApplyShakeAtNode(tiny, 20, "NPC Root [Root]", 20.0);
					if (giant->formID == 0x14 && IsDragon(tiny)) {
						CompleteDragonQuest(tiny, false);
					}
					ActorHandle giantHandle = giant->CreateRefHandle();
					ActorHandle tinyHandle = tiny->CreateRefHandle();
					std::string taskname = std::format("STN {}", tiny->formID);

					TaskManager::RunOnce(taskname, [=](auto& update){
						if (!tinyHandle) {
							return;
						}
						if (!giantHandle) {
							return;
						}
						auto giant = giantHandle.get().get();
						auto tiny = tinyHandle.get().get();
						float scale = get_visual_scale(tiny);
						TransferInventory(tiny, giant, scale, false, true, DamageSource::Crushed, true);
						// Actor reset is done within TransferInventory
					});
					if (tiny->formID != 0x14) {
						Disintegrate(tiny, true); // Set critical stage 4 on actors
					} else if (tiny->formID == 0x14) {
						TriggerScreenBlood(50);
						tiny->SetAlpha(0.0); // Player can't be disintegrated, so we make player Invisible
					}

					if (tinyHandle) {
						Runtime::PlaySound("ShrinkToNothingSound", tinyHandle.get().get(), 1.0, 0.5);
					}

					data.state = ShrinkState::Shrinked;
				}
			}
		}
	}


	void ShrinkToNothingManager::Reset() {
		this->data.clear();
	}

	void ShrinkToNothingManager::ResetActor(Actor* actor) {
		if (actor) {
			this->data.erase(actor->formID);
		}
	}

	void ShrinkToNothingManager::Shrink(Actor* giant, Actor* tiny) {
		if (!tiny) {
			return;
		}
		if (!giant) {
			return;
		}
		if (ShrinkToNothingManager::CanShrink(giant, tiny)) {
			ShrinkToNothingManager::GetSingleton().data.try_emplace(tiny->formID, giant);
		}
	}

	bool ShrinkToNothingManager::AlreadyShrinked(Actor* actor) {
		if (!actor) {
			return false;
		}
		auto& m = ShrinkToNothingManager::GetSingleton().data;
		return !(m.find(actor->formID) == m.end());
	}

	bool ShrinkToNothingManager::CanShrink(Actor* giant, Actor* tiny) {
		if (ShrinkToNothingManager::AlreadyShrinked(tiny)) {
			return false;
		}
		if (IsEssential(tiny)) {
			return false;
		}

		return true;
	}

	void ShrinkToNothingManager::AdjustGiantessSkill(Actor* Caster, Actor* Target) { // Adjust Matter Of Size skill on Shrink To Nothing
		if (Caster->formID != 0x14) {
			return; //Bye
		}

		auto GtsSkillLevel = Runtime::GetGlobal("GtsSkillLevel");
		auto GtsSkillRatio = Runtime::GetGlobal("GtsSkillRatio");
		auto GtsSkillProgress = Runtime::GetGlobal("GtsSkillProgress");


		int random = (100 + (rand()% 24 + 1)) / 100;

		if (GtsSkillLevel->value >= 100.0) {
			GtsSkillLevel->value = 100.0;
			GtsSkillRatio->value = 0.0;
			return;
		}

		float skill_level = GtsSkillLevel->value;

		float ValueEffectiveness = std::clamp(1.0 - GtsSkillLevel->value/100, 0.10, 1.0);

		float absorbedSize = (get_visual_scale(Target));
		if (Target->IsDead()) {
			absorbedSize *= 0.1; // Less effective on dead actors
			log::info("Is Dead {}", Target->GetDisplayFullName());
		}
		float oldvaluecalc = 1.0 - GtsSkillRatio->value; //Attempt to keep progress on the next level
		float Total = (((0.24 * random) + absorbedSize/50) * ValueEffectiveness);
		GtsSkillRatio->value += Total * GetXpBonus();

		if (GtsSkillRatio->value >= 1.0) {
			float transfer = clamp(0.0, 1.0, Total - oldvaluecalc);
			GtsSkillRatio->value = transfer;
			GtsSkillLevel->value = skill_level + 1.0;
			GtsSkillProgress->value = GtsSkillLevel->value;
			AddPerkPoints(GtsSkillLevel->value);
		}
	}

	ShrinkData::ShrinkData(Actor* giant) :
		delay(Timer(0.01)),
		state(ShrinkState::Healthy),
		giant(giant ? giant->CreateRefHandle() : ActorHandle()) {
	}
}
