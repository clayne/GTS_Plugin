#pragma once
// Module that handles footsteps

#include "managers/events.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;

namespace Gts {
	class TremorManager : public EventListener {
		public:
			[[nodiscard]] static TremorManager& GetSingleton() noexcept;

			virtual std::string DebugName() override;
			virtual void OnImpact(const Impact& impact) override;

			inline bool GetFP(); // TODO: Fix this
	};
}
