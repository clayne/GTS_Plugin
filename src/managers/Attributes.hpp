#pragma once
// Module that handles AttributeValues
#include "events.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;

namespace Gts {

	class AttributeManager : public EventListener {
		public:
			[[nodiscard]] static AttributeManager& GetSingleton() noexcept;

			virtual std::string DebugName() override;
			virtual void Update() override;

			void Augmentation();
			void OverrideBonus(float Value);
			float GetAttributeBonus(Actor* actor, float Value);
		private:
			float MovementSpeedBonus = 0.0;
			bool BlockMessage = false;
			SoftPotential speed_adjustment_walk { 
				.k = 0.265, // 0.125
				.n = 1.11, // 0.86
				.s = 2.0, // 1.12 
				.o = 1.0,
				.a = 0.0,  //Default is 0
			};	
	};
}
