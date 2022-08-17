#include "magic/effects/grow_button.hpp"
#include "magic/effects/common.hpp"
#include "magic/magic.hpp"
#include "scale/scale.hpp"
#include "data/runtime.hpp"
#include "util.hpp"

namespace Gts {
	std::string GrowButton::GetName() {
		return "GrowButton";
	}

	bool GrowButton::StartEffect(EffectSetting* effect) { // NOLINT
		auto& runtime = Runtime::GetSingleton();

		return effect == runtime.GrowPcButton;
	}

	void GrowButton::OnStart() {
		Actor* caster = GetCaster();
		if (!caster) {
			return;
		}
		auto& runtime = Runtime::GetSingleton();

		//BSSoundHandle growth_sound = BSSoundHandle::BSSoundHandle();
		//auto audio_manager = BSAudioManager::GetSingleton();
		//BSISoundDescriptor* sound_descriptor = runtime.growthSound;
		//audio_manager->BuildSoundDataFromDescriptor(growth_sound, sound_descriptor);
		//growth_sound.Play();

	}

	void GrowButton::OnUpdate() {
		auto caster = GetCaster();
		if (!caster) {
			return;
		}

		float caster_scale = get_visual_scale(caster);
		float stamina = clamp(0.05, 1.0, GetStaminaPercentage(caster));
		DamageAV(caster, ActorValue::kStamina, 0.45 * (caster_scale * 0.5 + 0.5) * stamina * TimeScale());
		Grow(caster, 0.0025 * stamina, 0.0);
		shake_camera(caster, 0.25, 1.0);
		shake_controller(0.25, 0.25, 1.0);
	}
}
