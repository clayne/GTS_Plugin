#pragma once

#include "events.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;
using namespace Gts;

namespace Gts {
	enum class DeathSource {
		Crushed = 0,
		HandCrushed = 1,
		Shrinked = 2,
		Vored = 3,
		ThighCrushed = 4,
		ThighSandwiched = 5,
	};

	void PlayAnimation(Actor* actor, std::string_view animName);

	void TransferInventory(Actor* from, Actor* to, bool keepOwnership, bool removeQuestItems);

	void Disintegrate(Actor* actor);
	void UnDisintegrate(Actor* actor);

	void SetRestrained(Actor* actor);
	void SetUnRestrained(Actor* actor);

	void SetDontMove(Actor* actor);
	void SetMove(Actor* actor);

	std::vector<hkpRigidBody*> GetActorRBs(Actor* actor);
	void PushActorAway(TESObjectREFR* source, Actor* receiver, float afKnockbackForce);
	void KnockAreaEffect(TESObjectREFR* source, float afMagnitude, float afRadius);
	void ApplyHavokImpulse(TESObjectREFR* target, float afX, float afY, float afZ, float afMagnitude);

	void CompleteDragonQuest();
	bool IsDragon(Actor* actor);

	bool IsProne(Actor* actor);

	float get_distance_to_actor(Actor* receiver, Actor* target);

	bool IsJumping(Actor* actor);

	void ApplyShake(Actor* caster, float modifier);
	void ApplyShakeAtNode(Actor* caster, float modifier, std::string_view node);
	void ApplyShakeAtPoint(Actor* caster,float modifier, const NiPoint3& coords);
	void EnableFreeCamera();

	bool AllowDevourment();
	void CallDevourment(Actor* giant, Actor* tiny);
	void CallGainWeight(Actor* giant, float value);
	void CallVampire();

	void DoSizeEffect(Actor* giant, float modifier, FootEvent kind, std::string_view node);
	void DoDamageEffect(Actor* giant, float damage, float radius, int random);

	void PrintDeathSource(Actor* giant, Actor* tiny, const DeathSource& source);
}
