#include "papyrus/camera.h"
#include "data/persistent.h"


using namespace SKSE;
using namespace Gts;
using namespace RE;
using namespace RE::BSScript;

namespace {
	constexpr std::string_view PapyrusClass = "GtsCamera";

	void SetEnableCollisionActor(StaticFunctionTag*, bool enabled) {
		Persistent::GetSingleton().camera_collisions.enable_actor = enabled;
	}

	bool GetEnableCollisionActor(StaticFunctionTag*) {
		return Persistent::GetSingleton().camera_collisions.enable_actor;
	}

	void SetEnableCollisionTree(StaticFunctionTag*, bool enabled) {
		Persistent::GetSingleton().camera_collisions.enable_trees = enabled;
	}

	bool GetEnableCollisionTree(StaticFunctionTag*) {
		return Persistent::GetSingleton().camera_collisions.enable_trees;
	}

	void SetEnableCollisionDebris(StaticFunctionTag*, bool enabled) {
		Persistent::GetSingleton().camera_collisions.enable_debris = enabled;
	}

	bool GetEnableCollisionDebris(StaticFunctionTag*) {
		return Persistent::GetSingleton().camera_collisions.enable_debris;
	}

	void SetEnableCollisionTerrain(StaticFunctionTag*, bool enabled) {
		Persistent::GetSingleton().camera_collisions.enable_terrain = enabled;
	}

	bool GetEnableCollisionTerrain(StaticFunctionTag*) {
		return Persistent::GetSingleton().camera_collisions.enable_terrain;
	}

	void SetCollisionScale(StaticFunctionTag*, float scale) {
		log::info("Setting Collision Scale: {}", scale);
		Persistent::GetSingleton().camera_collisions.above_scale = scale;
	}

	float GetCollisionScale(StaticFunctionTag*) {
		log::info("Getting Collision Scale: {}", Persistent::GetSingleton().camera_collisions.above_scale);
		return Persistent::GetSingleton().camera_collisions.above_scale;
	}
}

namespace Gts {
	bool register_papyrus_camera(IVirtualMachine* vm) {
		vm->RegisterFunction("SetEnableCollisionActor", PapyrusClass, SetEnableCollisionActor);
		vm->RegisterFunction("GetEnableCollisionActor", PapyrusClass, GetEnableCollisionActor);
		vm->RegisterFunction("SetEnableCollisionTree", PapyrusClass, SetEnableCollisionTree);
		vm->RegisterFunction("GetEnableCollisionTree", PapyrusClass, GetEnableCollisionTree);
		vm->RegisterFunction("SetEnableCollisionDebris", PapyrusClass, SetEnableCollisionDebris);
		vm->RegisterFunction("GetEnableCollisionDebris", PapyrusClass, GetEnableCollisionDebris);
		vm->RegisterFunction("SetEnableCollisionTerrain", PapyrusClass, SetEnableCollisionTerrain);
		vm->RegisterFunction("GetEnableCollisionTerrain", PapyrusClass, GetEnableCollisionTerrain);
		vm->RegisterFunction("SetCollisionScale", PapyrusClass, SetCollisionScale);
		vm->RegisterFunction("GetCollisionScale", PapyrusClass, GetCollisionScale);

		return true;
	}
}
