#include "managers/contact.h"

#include "util.h"

using namespace SKSE;
using namespace RE;
using namespace REL;
using namespace Gts;

namespace {
	// From https://github.com/ersh1/Precision, https://github.com/adamhynek/activeragdoll/ and https://github.com/adamhynek/higgs
	enum class WorldExtensionIds : int32_t
	{
		kAnonymous = -1,
		kBreakOffParts = 1000,
		kCollisionCallback = 1001
	};


	hkpWorldExtension* findWorldExtension(hkpWorld* a_world, WorldExtensionIds a_id) {
		using func_t = decltype(&findWorldExtension);
		REL::Relocation<func_t> func{ RELOCATION_ID(60549, 61397) };
		return func(a_world, a_id);
	}

	bool requireCollisionCallbackUtil(hkpWorld* a_world) {
		using func_t = decltype(&requireCollisionCallbackUtil);
		REL::Relocation<func_t> func{ RELOCATION_ID(60588, 61437) };
		return func(a_world);
	}

	bool releaseCollisionCallbackUtil(hkpWorld* a_world) {
		using func_t = decltype(&releaseCollisionCallbackUtil);
		REL::Relocation<func_t> func{ RELOCATION_ID(61800, 62715) };
		return func(a_world);
	}

	void addContactListener(RE::hkpWorld* a_world, RE::hkpContactListener* a_worldListener){
		using func_t = decltype(&addContactListener);
		REL::Relocation<func_t> func{ RELOCATION_ID(60543, 61383) };
		return func(a_world, a_worldListener);
	}

	void removeContactListener(hkpWorld* a_this, hkpContactListener* a_worldListener)
	{
		hkArray<hkpContactListener*>& listeners = a_this->contactListeners;

		for (int i = 0; i < listeners.size(); i++) {
			hkpContactListener* listener = listeners[i];
			if (listener == a_worldListener) {
				listeners[i] = nullptr;
				return;
			}
		}
	}

	void addWorldPostSimulationListener(RE::hkpWorld* a_world, RE::hkpWorldPostSimulationListener* a_worldListener) {
		using func_t = decltype(&addWorldPostSimulationListener);
		REL::Relocation<func_t> func{ RELOCATION_ID(60538, 61366) };
		return func(a_world, a_worldListener);
	}

	void removeWorldPostSimulationListener(RE::hkpWorld* a_world, RE::hkpWorldPostSimulationListener* a_worldListener) {
		using func_t = decltype(&removeWorldPostSimulationListener);
		REL::Relocation<func_t> func{ RELOCATION_ID(60539, 61367) };
		return func(a_world, a_worldListener);
	}

	void print_collision_groups(std::uint64_t flags) {
		std::map<std::string, COL_LAYER> named_layers {
			{ "kStatic", COL_LAYER::kStatic },
			{ "kAnimStatic", COL_LAYER::kAnimStatic },
			{ "kTransparent", COL_LAYER::kTransparent },
			{ "kClutter", COL_LAYER::kClutter },
			{ "kWeapon", COL_LAYER::kWeapon },
			{ "kProjectile", COL_LAYER::kProjectile },
			{ "kSpell", COL_LAYER::kSpell },
			{ "kBiped", COL_LAYER::kBiped },
			{ "kTrees", COL_LAYER::kTrees },
			{ "kProps", COL_LAYER::kProps },
			{ "kWater", COL_LAYER::kWater },
			{ "kTrigger", COL_LAYER::kTrigger },
			{ "kTerrain", COL_LAYER::kTerrain },
			{ "kTrap", COL_LAYER::kTrap },
			{ "kNonCollidable", COL_LAYER::kNonCollidable },
			{ "kCloudTrap", COL_LAYER::kCloudTrap },
			{ "kGround", COL_LAYER::kGround },
			{ "kPortal", COL_LAYER::kPortal },
			{ "kDebrisSmall", COL_LAYER::kDebrisSmall },
			{ "kDebrisLarge", COL_LAYER::kDebrisLarge },
			{ "kAcousticSpace", COL_LAYER::kAcousticSpace },
			{ "kActorZone", COL_LAYER::kActorZone },
			{ "kProjectileZone", COL_LAYER::kProjectileZone },
			{ "kGasTrap", COL_LAYER::kGasTrap },
			{ "kShellCasting", COL_LAYER::kShellCasting },
			{ "kTransparentWall", COL_LAYER::kTransparentWall },
			{ "kInvisibleWall", COL_LAYER::kInvisibleWall },
			{ "kTransparentSmallAnim", COL_LAYER::kTransparentSmallAnim },
			{ "kClutterLarge", COL_LAYER::kClutterLarge },
			{ "kCharController", COL_LAYER::kCharController },
			{ "kStairHelper", COL_LAYER::kStairHelper },
			{ "kDeadBip", COL_LAYER::kDeadBip },
			{ "kBipedNoCC", COL_LAYER::kBipedNoCC },
			{ "kAvoidBox", COL_LAYER::kAvoidBox },
			{ "kCollisionBox", COL_LAYER::kCollisionBox },
			{ "kCameraSphere", COL_LAYER::kCameraSphere },
			{ "kDoorDetection", COL_LAYER::kDoorDetection },
			{ "kConeProjectile", COL_LAYER::kConeProjectile },
			{ "kCamera", COL_LAYER::kCamera },
			{ "kItemPicker", COL_LAYER::kItemPicker },
			{ "kLOS", COL_LAYER::kLOS },
			{ "kPathingPick", COL_LAYER::kPathingPick },
			{ "kUnused", COL_LAYER::kUnused },
			{ "kUnused", COL_LAYER::kUnused },
			{ "kSpellExplosion", COL_LAYER::kSpellExplosion },
			{ "kDroppingPick", COL_LAYER::kDroppingPick },
		};
		for (const auto& [key, value] : named_layers) {
			auto layer_flag = (static_cast<uint64_t>(1) << static_cast<uint64_t>(value));
			if ((flags & layer_flag) != 0) {
				log::info(" - Collides with {}", key);
			}
		}

	}
}

namespace Gts {

	void ContactListener::ContactPointCallback(const hkpContactPointEvent& a_event)
	{
		// log::info("ContactPointCallback");
	}

	void ContactListener::CollisionAddedCallback(const hkpCollisionEvent& a_event)
	{
		// log::info("CollisionAddedCallback");
	}

	void ContactListener::CollisionRemovedCallback(const hkpCollisionEvent& a_event)
	{
		// log::info("CollisionRemovedCallback");
	}

	void ContactListener::PostSimulationCallback(hkpWorld* a_world)
	{
		// log::info("PostSimulationCallback");
	}

	void ContactListener::detach() {
		if (world) {
			BSWriteLockGuard lock(world->worldLock);
			auto collisionCallbackExtension = findWorldExtension(world, WorldExtensionIds::kCollisionCallback);
			if (collisionCallbackExtension) {
				releaseCollisionCallbackUtil(world->GetWorld2());
			}
			removeContactListener(world->GetWorld2(), this);
			removeWorldPostSimulationListener(world->GetWorld2(), this);
			this->world = nullptr;
		}
	}
	void ContactListener::attach(NiPointer<bhkWorld> world) {
		// Only runs if current world is nullptr and new is not
		if (!this->world && world) {
			this->world = world;
			BSWriteLockGuard lock(world->worldLock);
			requireCollisionCallbackUtil(world->GetWorld2());
			addContactListener(world->GetWorld2(), this);
			addWorldPostSimulationListener(world->GetWorld2(), this);

			RE::bhkCollisionFilter* filter = static_cast<bhkCollisionFilter*>(world->GetWorld2()->collisionFilter);
			log::info("CameraSphere Collision Groups");
			print_collision_groups(filter->layerBitfields[static_cast<uint8_t>(COL_LAYER::kCameraSphere)]);
			log::info("Camera Collision Groups");
			print_collision_groups(filter->layerBitfields[static_cast<uint8_t>(COL_LAYER::kCamera)]);
			filter->layerBitfields[static_cast<uint8_t>(COL_LAYER::kCameraSphere)] = 0;
			filter->layerBitfields[static_cast<uint8_t>(COL_LAYER::kCamera)] = 0;

		}
	}

	void ContactListener::ensure_last() {
		// Ensure our listener is the last one (will be called first)
		hkArray<hkpContactListener*>& listeners = world->GetWorld2()->contactListeners;
		if (listeners[listeners.size() - 1] != this) {
			BSWriteLockGuard lock(world->worldLock);

			int numListeners = listeners.size();
			int listenerIndex = -1;

			// get current index of our listener
			for (int i = 0; i < numListeners; ++i) {
				if (listeners[i] == this) {
					listenerIndex = i;
					break;
				}
			}

			if (listenerIndex >= 0) {
				for (int i = listenerIndex + 1; i < numListeners; ++i) {
					listeners[i - 1] = listeners[i];
				}
				listeners[numListeners - 1] = this;
			}
		}
	}


	ContactManager& ContactManager::GetSingleton() noexcept {
		static ContactManager instance;
		return instance;
	}

	void ContactManager::Update() {
		auto playerCharacter = PlayerCharacter::GetSingleton();

		auto cell = playerCharacter->GetParentCell();
		if (!cell) return;

		auto world = RE::NiPointer<RE::bhkWorld>(cell->GetbhkWorld());
		if (!world) return;
		ContactListener& contactListener = this->listener;

		if (contactListener.world != world) {
			contactListener.detach();
			contactListener.attach(world);
			contactListener.ensure_last();
		}
	}
}
