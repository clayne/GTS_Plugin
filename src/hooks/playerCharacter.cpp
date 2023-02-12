#include "hooks/playerCharacter.hpp"
#include "managers/Attributes.hpp"
#include "data/runtime.hpp"
#include "data/persistent.hpp"
#include "data/plugin.hpp"
#include "events.hpp"
#include "scale/scale.hpp"
#include "timer.hpp"
#include "utils/debug.hpp"

using namespace RE;
using namespace Gts;

namespace Hooks
{
	void Hook_PlayerCharacter::Hook() {
		logger::info("Hooking PlayerCharacter");
		REL::Relocation<std::uintptr_t> Vtbl{ RE::VTABLE_PlayerCharacter[0] };
		_HandleHealthDamage = Vtbl.write_vfunc(REL::Relocate(0x104, 0x104, 0x106), HandleHealthDamage);
		_AddPerk = Vtbl.write_vfunc(REL::Relocate(0x0FB, 0x0FB, 0x0FD), AddPerk);
		_RemovePerk = Vtbl.write_vfunc(REL::Relocate(0x0FC, 0x0FC, 0x0FE), RemovePerk);
		_SetSize = Vtbl.write_vfunc(REL::Relocate(0x0D9, 0x0D9, 0x0DB), SetSize);
		_Move = Vtbl.write_vfunc(REL::Relocate(0x0C8, 0x0C8, 0x0CA), Move);
		//_ProcessTracking = (Vtbl.write_vfunc(REL::Relocate(0x122, 0x122, 0x124), ProcessTracking));

		REL::Relocation<std::uintptr_t> Vtbl5{ RE::VTABLE_PlayerCharacter[5] };
		_GetActorValue = Vtbl5.write_vfunc(0x01, GetActorValue);
		_GetPermanentActorValue = Vtbl5.write_vfunc(0x02, GetPermanentActorValue);
		_GetBaseActorValue = Vtbl5.write_vfunc(0x03, GetBaseActorValue);
	}

	void Hook_PlayerCharacter::HandleHealthDamage(PlayerCharacter* a_this, Actor* a_attacker, float a_damage) {
		if (a_attacker) {
			float damagemult = AttributeManager::GetSingleton().GetAttributeBonus(a_attacker, ActorValue::kAttackDamageMult);
			float damage = (a_damage * damagemult) - a_damage;
			if (damage < 0) {
				DamageAV(a_this, ActorValue::kHealth, -damage); // Damage hp
			} if (damage > 0) {
				a_this->AsActorValueOwner()->RestoreActorValue(ACTOR_VALUE_MODIFIER::kDamage, ActorValue::kHealth, damage); // Restore hp
			}
			if (Runtime::HasPerkTeam(a_this, "SizeReserveAug")) { // Size Reserve Augmentation
				auto Cache = Persistent::GetSingleton().GetData(a_this);
				if (Cache) {
					Cache->SizeReserve += -a_damage/3000;
				}
			}
		}
		_HandleHealthDamage(a_this, a_attacker, a_damage);
	}

	void Hook_PlayerCharacter::AddPerk(PlayerCharacter* a_this, BGSPerk* a_perk, std::uint32_t a_rank) {
		_AddPerk(a_this, a_perk, a_rank);
		AddPerkEvent evt = AddPerkEvent {
			.actor = a_this,
			.perk = a_perk,
			.rank = a_rank,
		};
		EventDispatcher::DoAddPerk(evt);
	}

	void Hook_PlayerCharacter::RemovePerk(PlayerCharacter* a_this, BGSPerk* a_perk) {
		RemovePerkEvent evt = RemovePerkEvent {
			.actor = a_this,
			.perk = a_perk,
		};
		EventDispatcher::DoRemovePerk(evt);
		_RemovePerk(a_this, a_perk);
	}

	float Hook_PlayerCharacter::GetActorValue(ActorValueOwner* a_owner, ActorValue a_akValue) { // Override Carry Weight and Sneak
		float value = _GetActorValue(a_owner, a_akValue);
		if (Plugin::Ready()) {
			Actor* a_this = skyrim_cast<Actor*>(a_owner);
			if (a_this) {
				value = AttributeManager::AlterGetAv(a_this, a_akValue, value);
			}
		}
		return value;
	}

	float Hook_PlayerCharacter::GetBaseActorValue(ActorValueOwner* a_owner, ActorValue a_akValue) { // Override Health
		float value = _GetBaseActorValue(a_owner, a_akValue);
		/*if (Plugin::Ready()) {
			Actor* a_this = skyrim_cast<Actor*>(a_owner);
			float bonus = 1.0;
			if (a_this) {
				value = AttributeManager::AlterGetBaseAv(a_this, a_akValue, value);
			}
		}
		log::info("value: {}", value);*/
		return value;
	}

	float Hook_PlayerCharacter::GetPermanentActorValue(ActorValueOwner* a_owner, ActorValue a_akValue) { // Override Carry Weight and Damage
		float value = _GetPermanentActorValue(a_owner, a_akValue);
		if (Plugin::Ready()) {
			Actor* a_this = skyrim_cast<Actor*>(a_owner);
			if (a_this) {
				value = AttributeManager::AlterGetPermenantAv(a_this, a_akValue, value);
			}
		}
		return value;
	}

	void Hook_PlayerCharacter::SetSize(PlayerCharacter* a_this, float a_size) {
		// _SetSize(a_this, a_size);
	}

	void Hook_PlayerCharacter::Move(PlayerCharacter* a_this, float a_arg2, const NiPoint3& a_position) { // Override Movement Speed
		float bonus = AttributeManager::AlterMovementSpeed(a_this, a_position);
		return _Move(a_this, a_arg2, a_position * bonus);
	}

	void Hook_PlayerCharacter::ProcessTracking(PlayerCharacter* a_this, float a_delta, NiAVObject* a_obj3D) {
		float adjust = Runtime::GetFloat("ConversationCameraComp");
		auto player = PlayerCharacter::GetSingleton()->Get3D();
		_ProcessTracking(a_this, a_delta, a_obj3D);
	}
}
