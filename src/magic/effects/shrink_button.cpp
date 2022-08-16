#include "magic/effects/shrink_button.hpp"
#include "magic/effects/common.hpp"
#include "magic/magic.hpp"
#include "scale/scale.hpp"
#include "data/runtime.hpp"
#include "util.hpp"

namespace Gts {
	bool ShrinkButton::StartEffect(EffectSetting* effect) { // NOLINT
		auto& runtime = Runtime::GetSingleton();
		return (effect == runtime.ShrinkPCButton);
	}

	void ShrinkButton::OnUpdate() {
		auto caster = GetCaster();
		if (!caster) {
			return;
		}
		auto target = GetTarget();
		if (!target) {
			return;
		}

		float caster_scale = get_visual_scale(caster);
		float stamina = clamp(0.25, 1.0, GetStaminaPercentage(caster));
		if (casterScale > 0.25) {
			DamageAV(caster, ActorValue::kStamina, 0.25 * (caster_scale * 0.5 + 0.5) * stamina * time_scale());
			Shrink(caster, 0.0025*stamina, 0.0);
		}
	}
}
