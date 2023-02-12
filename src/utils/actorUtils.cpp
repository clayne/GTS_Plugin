#include "utils/actorUtils.hpp"
#include "utils/findActor.hpp"
#include "utils/papyrusUtils.hpp"
#include "data/runtime.hpp"
#include "data/re.hpp"

using namespace RE;
using namespace Gts;

namespace {
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
}

RE::ExtraDataList::~ExtraDataList() {
}

namespace Gts {
	void PlayAnimation(Actor* actor, std::string_view animName) {
		actor->NotifyAnimationGraph(animName);
	}

	void TransferInventory(Actor* from, Actor* to, bool keepOwnership, bool removeQuestItems) {
		log::info("Trying to transfer inventory from {} to {}", from->GetDisplayFullName(), to->GetDisplayFullName());
		for (auto &[a_object, invData]: from->GetInventory()) {
			log::info("For loop");
			const auto& [count, entry] = invData;
			if (!removeQuestItems && entry->IsQuestObject()) {
				log::info("Remove quest items is false, continuing");
				continue;
			}
			log::info("Adding extra lists");
			RE::ExtraDataList* a_extraList = nullptr; //CreateExDataList();
			log::info("Extra Data Complete");
			/*if (keepOwnership) {
				a_extraList->SetOwner(entry->GetOwner());
			} else {
				a_extraList->SetOwner(to);
			}*/
			log::info("Adding to container");
			to->AddObjectToContainer(a_object, a_extraList, count, from);
			log::info("Complete, adding items");
			delete a_extraList;
			from->RemoveItem(a_object, 1, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr, nullptr, nullptr);
			log::info("Dalloced");
		}
	}

	void Disintegrate(Actor* actor) {
		actor->GetActorRuntimeData().criticalStage.set(ACTOR_CRITICAL_STAGE::kDisintegrateEnd);
		// CallFunctionOn(actor, "actor", "SetCriticalStage", 4);
		actor->Disable();
	}

	void UnDisintegrate(Actor* actor) {
		actor->GetActorRuntimeData().criticalStage.reset(ACTOR_CRITICAL_STAGE::kDisintegrateEnd);
		// CallFunctionOn(actor, "Actor", "SetCriticalStage", 0);
		// actor->Enable();
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
		/*NiPoint3 sourceLoc = source->GetPosition();
		   NiPoint3 destinationLoc = receiver->GetPosition();
		   NiPoint3 direction = destinationLoc - sourceLoc;
		   NiPoint3 niImpulse  = direction * afKnockBackForce/direction.Length();
		   hkVector4 impulse = hkVector4(niImpulse.x, niImpulse.y, niImpulse.z, 1.0);

		   auto rbs = GetActorRB(receiver);
		   for (auto rb: rbs) {
		        auto& motion = rb->motion;
		        motion.ApplyLinearImpulse(impulse);
		        log::info("Trying to push actor {} away", receiver);
		   }*/
		if (receiver->IsDead()) {
			return;
		}
		CallFunctionOn(source, "ObjectReference", "PushActorAway", receiver, afKnockBackForce);
	}
	void KnockAreaEffect(TESObjectREFR* source, float afMagnitude, float afRadius) {
		// NiPoint3 sourceLoc = source->GetPosition();
		// for (auto actor: find_actors()) {
		// 	if ((actor->GetPosition() - sourceLoc).Length() < afRadius) {
		// 		auto aiProc = actor->currentProcess;
		// 		if (aiProc) {
		// 			aiProc->KnockExplosion(actor, sourceLoc, afMagnitude);
		// 		}
		// 	}
		// }
		CallFunctionOn(source, "ObjectReference", "KnockAreaEffect", afMagnitude, afRadius);
	}
	void ApplyHavokImpulse(TESObjectREFR* target, float afX, float afY, float afZ, float afMagnitude) {
		/*NiPoint3 direction = NiPoint3(afX, afY, afZ);
		   NiPoint3 niImpulse = direction * afMagnitude/direction.Length();
		   hkVector4 impulse = hkVector4(niImpulse.x, niImpulse.y, niImpulse.z, 0.0);//hkVector4(niImpulse.x, niImpulse.y, niImpulse.z, 0.0);

		   auto rbs = GetActorRB(target);
		   for (auto rb: rbs) {
		        auto& motion = rb->motion;
		        motion.ApplyLinearImpulse(impulse);
		   }*/
		CallFunctionOn(target, "ObjectReference", "ApplyHavokImpulse", afX, afY, afZ, afMagnitude);
	}

	bool IsDragon(Actor* actor) {
		return (
			std::string(actor->GetDisplayFullName()).find("ragon") != std::string::npos
			|| Runtime::IsRace(actor, "dragonRace")
			);
	}

	bool IsProne(Actor* actor) {
		return actor!= nullptr && actor->formID == 0x14 && Runtime::GetBool("ProneEnabled") && actor->AsActorState()->IsSneaking();
	}

	bool IsJumping(Actor* actor) {
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

	float get_distance_to_actor(Actor* receiver, Actor* target) {
		if (target) {
			auto point_a = receiver->GetPosition();
			auto point_b = target->GetPosition();
			auto delta = point_a - point_b;
			return delta.Length();
		}
		return 3.4028237E38; // Max float
	}
}
