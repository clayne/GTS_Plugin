#pragma once
#include "magic/magic.hpp"
// Module that handles footsteps


using namespace std;
using namespace SKSE;
using namespace RE;

namespace Gts {
	class Absorb : public Magic {
		public:
			Absorb(ActiveEffect* effect);

			virtual void OnUpdate() override;

			virtual void OnFinish() override;

			virtual std::string GetName() override;
		private:
			bool true_absorb = false;
	};
}
