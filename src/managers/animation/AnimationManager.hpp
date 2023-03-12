#pragma once
#include "events.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;

namespace Gts
{
	class AnimationManager : public EventListener
	{
		public:
			[[nodiscard]] static AnimationManager& GetSingleton() noexcept;

			virtual std::string DebugName() override;
			virtual void UpdateActors(Actor* target);
			void ActorAnimEvent(Actor* actor, const std::string_view& tag, const std::string_view& payload);
			void AdjustAnimSpeed(Actor* actor, float bonus);

			void Test(Actor * giant, Actor* tiny);
	};
}
