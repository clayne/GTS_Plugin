#include "managers/animation/AnimationManager.hpp"
#include "hooks/hkbBehaviorGraph.hpp"
#include "managers/animation/AnimationManager.hpp"

using namespace RE;
using namespace SKSE;
using namespace Gts;

namespace Hooks
{
	void Hook_hkbBehaviorGraph::Hook() {
		log::info("Hooking hkbBehaviorGraph");
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_hkbBehaviorGraph[0] };

		_Update = vtbl.write_vfunc(0x05, Update);
	}

	void Hook_hkbBehaviorGraph::Update(hkbBehaviorGraph* a_this, const hkbContext& a_context, float a_timestep) {
		float anim_speed = 1.0;
		for (auto actor: find_actors()) {
			BSAnimationGraphManagerPtr animGraphManager;
			if (actor->GetAnimationGraphManager(animGraphManager)) {
				for (auto& graph : animGraphManager->graphs) {
					if (graph) {
						if (a_this == graph->behaviorGraph) {
							float multi = AnimationManager::GetAnimSpeed(actor);
							anim_speed *= multi;
						}
						if (a_this == graph->characterInstance) {
							if (actor->formID == 0x14) {
								log::info("Current lod of {} is {}", actor->GetDisplayFullName(), graph->characterInstance->currentLOD);
							}
						}
					}
				}
			}
		}
		_Update(a_this, a_context, a_timestep * anim_speed);
	}
}
