#include "hooks/playerCharacter.hpp"
#include "managers/Attributes.hpp"
#include "data/runtime.hpp"
#include "data/persistent.hpp"
#include "data/plugin.hpp"
#include "events.hpp"
#include "scale/scale.hpp"

using namespace RE;
using namespace Gts;

namespace Hooks
{
	// BGSImpactManager
	void Hook_PlayerCharacter::Hook() {
		logger::info("Hooking PlayerCharacter");
		REL::Relocation<std::uintptr_t> Vtbl{ RE::VTABLE_PlayerCharacter[0] };
		_HandleHealthDamage = Vtbl.write_vfunc(REL::Relocate(0x104, 0x104, 0x106), HandleHealthDamage);
		_AddPerk = Vtbl.write_vfunc(REL::Relocate(0x0FB, 0x0FB, 0x0FD), AddPerk);
		_RemovePerk = Vtbl.write_vfunc(REL::Relocate(0x0FC, 0x0FC, 0x0FE), RemovePerk);

		REL::Relocation<std::uintptr_t> Vtbl5{ RE::VTABLE_PlayerCharacter[5] };
		_GetActorValue = Vtbl5.write_vfunc(0x01, GetActorValue);
		_GetPermanentActorValue = Vtbl5.write_vfunc(0x02, GetPermanentActorValue);
	}

	void Hook_PlayerCharacter::HandleHealthDamage(PlayerCharacter* a_this, Actor* a_attacker, float a_damage) {
		if (a_attacker) {
			if (Runtime::HasPerkTeam(a_this, "SizeReserveAug")) { // Size Reserve Augmentation
				auto Cache = Persistent::GetSingleton().GetData(a_this);
				if (Cache) {
					Cache->SizeReserve += -a_damage/3000;
				}
				a_damage *= 0.05;
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

	float Hook_PlayerCharacter::GetActorValue(PlayerCharacter* a_this, ActorValue a_akValue) {
		if (Plugin::Ready()) {
			float actual_value = _GetActorValue(a_this, a_akValue);
			float bonus = 1.0;
			auto player = PlayerCharacter::GetSingleton();
			float scale = get_visual_scale(player);
			auto& attributes = AttributeManager::GetSingleton();
			log::info("Get AV, scale: {}", scale);
			if (a_akValue == ActorValue::kHealth) {
				bonus = attributes.GetAttributeBonus(player, 1.0);
				return actual_value * bonus;
			}
			if (a_akValue == ActorValue::kCarryWeight) {
				bonus = attributes.GetAttributeBonus(player, 2.0);
				return actual_value * bonus;
			}
			if (a_akValue == ActorValue::kSpeedMult) {
				bonus = attributes.GetAttributeBonus(player, 3.0);
				return actual_value * bonus;
			}
			if (a_akValue == ActorValue::kAttackDamageMult) {
				bonus = attributes.GetAttributeBonus(player, 4.0);
				return actual_value * bonus;
			} else {
				return actual_value;
			}
		} else {
			return _GetActorValue(player, a_akValue);
		}
	}

	float Hook_PlayerCharacter::GetPermanentActorValue(PlayerCharacter* a_this, ActorValue a_akValue) {
		if (Plugin::Ready()) {
			log::info("Get Perma AV");
			log::info("a_this: {}", GetRawName(a_this));
			float actual_value = _GetPermanentActorValue(a_this, a_akValue);
			return actual_value * 2.5;
		} else {
			return _GetPermanentActorValue(a_this, a_akValue);
		}
	}
}
