#include "managers/cameras/tp/footL.hpp"
#include "managers/cameras/camutil.hpp"
#include "managers/highheel.hpp"
#include "data/runtime.hpp"
#include "scale/scale.hpp"
#include "node.hpp"
#include "util.hpp"
#include "actorUtils.hpp"

using namespace RE;

namespace {
	const float OFFSET = -0.02f * 70.0f; // About 2cm down
}

namespace Gts {
	NiPoint3 FootcameraL::GetFootPos() {
		float base_hh = 0;
		const std::string_view leftFootLookup = "NPC L Foot [Lft ]";
		auto player = PlayerCharacter::GetSingleton();
		if (player) {
			auto rootModel = player->Get3D(false);
			if (rootModel) {
				auto transform = rootModel->world.Invert();
				auto leftFoot = find_node(player, leftFootLookup);
				if (leftFoot != nullptr) {
					float playerScale = get_visual_scale(player);
					auto leftPosLocal = transform * (leftFoot->world * NiPoint3());
					this->smoothFootPos.target = leftPosLocal;
					if (!IsProne(player)) {
						NiPoint3 highheelOffset = HighHeelManager::GetHHOffset(player);
						if (highheelOffset.Length() > 1e-4) {
							this->smoothFootPos.target.z += OFFSET*playerScale;
							this->smoothFootPos.target -= highheelOffset * 0.8;
						}
					}
				}
			}
		}
		return this->smoothFootPos.value;
	}
}
