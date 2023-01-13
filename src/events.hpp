#pragma once

#include "profiler.hpp"

using namespace std;
using namespace RE;
using namespace SKSE;

namespace Gts {
	struct UnderFoot {
		Actor* giant;
		Actor* small;
		float force;
	};

	class EventListener {
		public:
			Profiler profiler;

			// Get name used for debug prints
			virtual std::string DebugName() = 0;

			// Called on Live (non paused) gameplay
			virtual void Update();

			// Called on Papyrus OnUpdate
			virtual void PapyrusUpdate();

			// Called on Havok update (when processing hitjobs)
			virtual void HavokUpdate();

			// Called when the camera update event is fired (in the TESCameraState)
			virtual void CameraUpdate();

			// Called on game load started (not yet finished)
			// and when new game is selected
			virtual void Reset();

			// Called when game is enabled (while not paused)
			virtual void Enabled();

			// Called when game is disabled (while not paused)
			virtual void Disabled();

			// Called when a game is started after a load/newgame
			virtual void Start();

			// Called when all forms are loaded (during game load before mainmenu)
			virtual void DataReady();

			// Called when an actor is reset
			virtual void ResetActor(Actor* actor);

			// Called when an actor has an item equipped
			virtual void ActorEquip(Actor* actor);

			// Called when an actor has is fully loaded
			virtual void ActorLoaded(Actor* actor);

			// Called when a papyrus hit event is fired
			virtual void HitEvent(const TESHitEvent* evt);

			// Called when an actor is squashed underfoot
			virtual void UnderFootEvent(const UnderFoot* evt);
	};

	class EventDispatcher {
		public:
			static void ReportProfilers();
			static void AddListener(EventListener* listener);
			static void DoUpdate();
			static void DoPapyrusUpdate();
			static void DoHavokUpdate();
			static void DoCameraUpdate();
			static void DoReset();
			static void DoEnabled();
			static void DoDisabled();
			static void DoStart();
			static void DoDataReady();
			static void DoResetActor(Actor* actor);
			static void DoActorEquip(Actor* actor);
			static void DoActorLoaded(Actor* actor);
			static void DoHitEvent(const TESHitEvent* evt);
			static void DoUnderFootEvent(const UnderFoot* evt);
		private:
			[[nodiscard]] static EventDispatcher& GetSingleton();
			std::vector<EventListener*> listeners;
	};
}
