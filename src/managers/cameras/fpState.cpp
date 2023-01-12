#include "managers/cameras/fpState.hpp"
#include "data/runtime.hpp"
#include "scale/scale.hpp"

using namespace RE;
using namespace Gts;

namespace  Gts {
	float FirstPersonCameraState::ProneAdjustment() {
		return clamp(0.25, 20.0, 3.0 * Runtime::GetFloat("ProneOffsetFP"));
	}

	NiPoint3 FirstPersonCameraState::GetPlayerLocalOffsetProne(const NiPoint3& cameraPos) {
		NiPoint3 pos = this->GetPlayerLocalOffset(cameraPos);
		auto player = PlayerCharacter::GetSingleton();
		if (player) {
			auto scale = get_visual_scale(player);
			pos += this->ProneAdjustment(cameraPos)*scale;
		}
		return pos;
	}

	bool FirstPersonCameraState::PermitManualEdit() {
		return false;
	}
	bool FirstPersonCameraState::PermitTransition() {
		return false;
	}
}
