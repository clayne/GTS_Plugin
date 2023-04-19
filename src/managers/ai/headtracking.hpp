#pragma once
// Module for the Gts Related code
#include <vector>
#include <atomic>
#include <unordered_map>

#include <RE/Skyrim.h>

#include "events.hpp"
#include "node.hpp"

using namespace std;
using namespace RE;

namespace Gts {
	class Headtracking : public EventListener  {
		public:
			[[nodiscard]] static Headtracking& GetSingleton() noexcept;

			virtual std::string DebugName() override;
			void FixHeadtracking(Actor* me);
	};
}