#include "managers/cameras/util.hpp"
#include "node.hpp"

using namespace RE;

namespace Gts {
	void SetINIFloat(std::string_view name, float value) {
		auto ini_conf = INISettingCollection::GetSingleton();
		Setting* setting = ini_conf->GetSetting(name);
		if (setting) {
			setting->data.f=value; // If float
			ini_conf->WriteSetting(setting);
		}
	}

	float GetINIFloat(std::string_view name) {
		auto ini_conf = INISettingCollection::GetSingleton();
		Setting* setting = ini_conf->GetSetting(name);
		if (setting) {
			return setting->data.f;
		}
		return -1.0;
	}

	void EnsureINIFloat(std::string_view name, float value) {
		auto currentValue = GetINIFloat(name);
		if (fabs(currentValue - value) > 1e-3) {
			SetINIFloat(name, value);
		}
	}

	void UpdateThirdPerson() {
		auto camera = PlayerCamera::GetSingleton();
		auto player = PlayerCharacter::GetSingleton();
		if (camera && player) {
			camera->UpdateThirdPerson(player->IsWeaponDrawn());
		}
	}

	void ResetIniSettings() {
		EnsureINIFloat("fOverShoulderPosX:Camera", 30.0);
		EnsureINIFloat("fOverShoulderPosY:Camera", 30.0);
		EnsureINIFloat("fOverShoulderPosZ:Camera", -10.0);
		EnsureINIFloat("fOverShoulderCombatPosX:Camera", 0.0);
		EnsureINIFloat("fOverShoulderCombatPosY:Camera", 0.0);
		EnsureINIFloat("fOverShoulderCombatPosZ:Camera", 20.0);
		EnsureINIFloat("fVanityModeMaxDist:Camera", 600.0);
		EnsureINIFloat("fVanityModeMinDist:Camera", 155.0);
		EnsureINIFloat("fMouseWheelZoomSpeed:Camera", 0.8000000119);
		EnsureINIFloat("fMouseWheelZoomIncrement:Camera", 0.075000003);
		UpdateThirdPerson();
	}

	NiCamera* GetNiCamera() {
		auto camera = PlayerCamera::GetSingleton();
		auto cameraRoot = camera->cameraRoot.get();
		NiCamera* niCamera = nullptr;
		for (auto child: cameraRoot->GetChildren()) {
			NiAVObject* node = child.get();
			if (node) {
				NiCamera* casted = netimmerse_cast<NiCamera*>(node);
				if (casted) {
					niCamera = casted;
					break;
				}
			}
		}
		return niCamera;
	}
	void UpdateWorld2ScreetMat(NiCamera* niCamera) {
		auto camNi = niCamera ? niCamera : GetNiCamera();
		typedef void (*UpdateWorldToScreenMtx)(RE::NiCamera*);
		static auto toScreenFunc = REL::Relocation<UpdateWorldToScreenMtx>(REL::RelocationID(69271, 70641).address());
		toScreenFunc(camNi);
	}

	#ifdef ENABLED_SHADOW
	ShadowSceneNode* GetShadowMap() {
		auto player = PlayerCharacter::GetSingleton();
		if (player) {
			auto searchRoot = player->GetCurrent3D();
			if (searchRoot) {
				NiNode* parent = searchRoot->parent;
				while (parent) {
					ShadowSceneNode* shadowNode = skyrim_cast<ShadowSceneNode*>(parent);
					if (shadowNode) {
						return shadowNode;
					}
					parent = parent->parent;
				}
			}
		}
		return nullptr;
	}
	#endif

	void UpdateSceneManager(NiPoint3 camLoc) {
		auto sceneManager = UI3DSceneManager::GetSingleton();
		if (sceneManager) {
			// Cache
			sceneManager->cachedCameraPos = camLoc;

			#ifdef ENABLED_SHADOW
			// Shadow Map
			auto shadowNode = sceneManager->shadowSceneNode;
			if (shadowNode) {
				shadowNode->GetRuntimeData().cameraPos = camLoc;
			}
			#endif

			// Camera
			auto niCamera = sceneManager->camera;
			if (niCamera) {
				niCamera->world.translate = camLoc;
				UpdateWorld2ScreetMat(niCamera.get());
			}
		}
	}

	void UpdateRenderManager(NiPoint3 camLoc) {
		auto renderManager = UIRenderManager::GetSingleton();
		if (renderManager) {
			#ifdef ENABLED_SHADOW
			// Shadow Map
			auto shadowNode = renderManager->shadowSceneNode;
			if (shadowNode) {
				shadowNode->GetRuntimeData().cameraPos = camLoc;
			}
			#endif

			// Camera
			auto niCamera = renderManager->camera;
			if (niCamera) {
				niCamera->world.translate = camLoc;
				UpdateWorld2ScreetMat(niCamera.get());
			}
		}
	}

	void UpdateNiCamera(NiPoint3 camLoc) {
		auto niCamera = GetNiCamera();
		if (niCamera) {
			niCamera->world.translate = camLoc;
			UpdateWorld2ScreetMat(niCamera);
			update_node(niCamera);
		}

		#ifdef ENABLED_SHADOW
		auto shadowNode = GetShadowMap();
		if (shadowNode) {
			shadowNode->GetRuntimeData().cameraPos = camLoc;
		}
		#endif
	}

	void UpdatePlayerCamera(NiPoint3 camLoc) {
		auto camera = PlayerCamera::GetSingleton();
		if (camera) {
			auto cameraRoot = camera->cameraRoot;
			if (cameraRoot) {
				cameraRoot->local.translate = camLoc;
				cameraRoot->world.translate = camLoc;
				update_node(cameraRoot.get());
			}
		}
	}

	// Get's camera position relative to the player
	NiPoint3 GetCameraPosLocal() {
		auto camera = PlayerCamera::GetSingleton();
		if (camera) {
			auto currentState = camera->currentState;
			if (currentState) {
				auto player = PlayerCharacter::GetSingleton();
				if (player) {
					auto model = player->Get3D(false);
					if (model) {
						NiPoint3 cameraLocation;
						currentState->GetTranslation(cameraLocation);
						auto playerTransInve = model->world.Invert();
						// Get Scaled Camera Location
						return playerTransInve*cameraLocation;
					}
				}
			}
		}
		return NiPoint3();
	}

	void ScaleCamera(float scale, NiPoint3 offset) {
		auto camera = PlayerCamera::GetSingleton();
		auto cameraRoot = camera->cameraRoot;
		auto player = PlayerCharacter::GetSingleton();
		auto currentState = camera->currentState;

		if (cameraRoot) {
			if (currentState) {
				NiPoint3 cameraLocation;
				currentState->GetTranslation(cameraLocation);
				if (player) {
					if (scale > 1e-4) {
						auto model = player->Get3D(false);
						if (model) {
							auto playerTrans = model->world;
							auto playerTransInve = model->world.Invert();

							// Make the transform matrix for our changes
							NiTransform adjustments = NiTransform();
							adjustments.scale = scale;

							// Get Scaled Camera Location
							auto targetLocationWorld = playerTrans*(adjustments*(playerTransInve*cameraLocation));

							auto parent = cameraRoot->parent;
							NiTransform transform = parent->world.Invert();
							auto targetLocationLocal = transform * targetLocationWorld;

							// Get shifted Camera Location
							NiTransform adjustments2 = NiTransform();
							adjustments2.transform = offset * scale;
							auto targetLocationLocalShifted = adjustments2 * targetLocationLocal;

							UpdatePlayerCamera(targetLocationLocalShifted);
							UpdateNiCamera(targetLocationLocalShifted);
							// UpdateSceneManager(targetLocationLocal);
							// UpdateRenderManager(targetLocationLocal);
						}
					}
				}
			}
		}
	}
}
