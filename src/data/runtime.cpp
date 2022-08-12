#include "data/runtime.h"
#include "Config.h"

using namespace SKSE;
using namespace RE;

namespace {
	template <class T>
	T* find_form(std::string_view lookup_id) {
		// From https://github.com/Exit-9B/MCM-Helper/blob/a39b292909923a75dbe79dc02eeda161763b312e/src/FormUtil.cpp
		std::string lookup_id_str(lookup_id);
		std::istringstream ss{ lookup_id_str };
		std::string plugin, id;

		std::getline(ss, plugin, '|');
		std::getline(ss, id);
		RE::FormID relativeID;
		std::istringstream{ id } >> std::hex >> relativeID;
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		return dataHandler ? dataHandler->LookupForm<T>(relativeID, plugin) : nullptr;
	}
}

namespace Gts {
	Runtime& Runtime::GetSingleton() noexcept {
		static Runtime instance;
		return instance;
	}

	void Runtime::Load() {
		this->lFootstepL = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetLFootstepL());
		this->lFootstepR = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetLFootstepR());

		this->lJumpLand = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetLJumpLand());

		this->xlFootstepL = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXLFootstepL());
		this->xlFootstepR = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXLFootstepR());

		this->xlRumbleL = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXLRumbleL());
		this->xlRumbleR = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXLRumbleR());

		this->xlSprintL = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXLSprintL());
		this->xlSprintR = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXLSprintR());

		this->xxlFootstepL = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXXLFootstepL());
		this->xxlFootstepR = find_form<BGSSoundDescriptorForm>(Config::GetSingleton().GetSound().GetXXLFootstepR());

		this->smallMassiveThreat = find_form<EffectSetting>(Config::GetSingleton().GetSpellEffects().GetSmallMassiveThreat());
		
		this->explosiveGrowth1 = find_form<EffectSetting>("GTS.esp|007928"); // < Growth Spurt shouts
			
		this->explosiveGrowth2 = find_form<EffectSetting>("GTS.esp|1E42A5");
		
		this->explosiveGrowth3 = find_form<EffectSetting>("GTS.esp|1E42A6");
		///Shrink Spells
		this->ShrinkPCButton = find_form<EffectSetting>("GTS.esp|10A6CF"); // <- Shrink PC in size on button press.
		this->ShrinkBack = find_form<EffectSetting>("GTS.esp|005369"); // < - Spell that restores size back to normal.
		this->ShrinkBackNPC = find_form<EffectSetting>("GTS.esp|00536C"); // < - Spell that restores size back to normal, for NPC.
		this->ShrinkSpell = find_form<EffectSetting>("GTS.esp|002850"); // <- Shrink Self over time [Hands]
		this->ShrinkAlly = find_form<EffectSetting>("GTS.esp|0058D1"); // <- Shrink ally without hostile damage and no aggro.
		
		this->ShrinkEnemy = find_form<EffectSetting>("GTS.esp|00387B");
		this->ShrinkEnemyAOE = find_form<EffectSetting>("GTS.esp|0DCDC5");
		this->ShrinkEnemyAOEMast = find_form<EffectSetting>("GTS.esp|0DCDCA");
		this->SwordEnchant = find_form<EffectSetting>("GTS.esp|00FA9E");

		this->ShrinkToNothing = find_form<EffectSetting>("GTS.esp|009979"); // <- Absorbs someone
		///End
		
		///Ally/Grow Spells
		this->SlowGrowth = find_form<EffectSetting>("GTS.esp|019C3D"); // <- slow growth spell [Hands]. Release and grow over time.
		this->GrowthSpell = find_form<EffectSetting>("GTS.esp|0022EB"); // <- Grow Spell [Hands]
		this->GrowPcButton = find_form<EffectSetting>("GTS.esp|002DB5"); // <- Grow PC in size on button press
		
		this->AllyGrowSize = find_form<EffectSetting>("GTS.esp|0058D5");  // <- Increase Ally Size [Hands]

		this->GrowAllySizeButton = find_form<EffectSetting>("GTS.esp|123BE3");  // <- Makes ally grow for 2 sec on button press.
		this->ShrinkAllySizeButton = find_form<EffectSetting>("GTS.esp|123BE4"); // <- Makes ally shrink for 2 sec on button press.

		this->AllyCrushGrowth = find_form<EffectSetting>("GTS.esp|2028B6"); // < Grow on Crush. NPC only. Player triggers CrushGrowth on crushing someone via Crush() function in SP.
		///End
		
		///Others
		this->GlobalVoreGrowth = find_form<EffectSetting>("GTS.esp|216CCC"); // < Vore Growth, used for both PC and Followers 
		///End

		this->footstepExplosion = find_form<BGSExplosion>(Config::GetSingleton().GetExplosions().GetFootstepExplosion());

		this->GrowthOnHitPerk = find_form<BGSPerk>("GTS.esp|30EE52");

		this->hhBonus = find_form<BGSPerk>(Config::GetSingleton().GetPerks().GetHHBonus());
		
	}
}
