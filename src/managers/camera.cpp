#include "managers/camera.hpp"
#include "managers/cameras/camutil.hpp"
#include "managers/highheel.hpp"
#include "util.hpp"
#include "actorUtils.hpp"
#include "data/runtime.hpp"
#include "data/time.hpp"
#include "data/persistent.hpp"
#include "Config.hpp"
#include "node.hpp"
#include "data/time.hpp"

using namespace SKSE;
using namespace RE;
using namespace REL;
using namespace Gts;



namespace Gts {
	CameraManager& CameraManager::GetSingleton() noexcept {
		static CameraManager instance;
		return instance;
	}

	std::string CameraManager::DebugName() {
		return "CameraManager";
	}

	void CameraManager::Start() {
		//ResetIniSettings();
	}

	void CameraManager::CameraUpdate() {
		CameraState* currentState = this->GetCameraState();

		// Handles Transitioning
		if (currentState != this->currentState) {
			if (this->currentState) {
				this->currentState->ExitState();
			}
			if (currentState) {
				currentState->EnterState();
			}
			auto prevState = this->currentState;
			this->currentState = currentState;
			if (prevState) {
				if (currentState) {
					if (currentState->PermitTransition() && prevState->PermitTransition()) {
						this->transitionState.reset(new TransState(prevState, currentState));
						currentState = this->transitionState.get();
					} else {
						this->transitionState.reset(nullptr);
					}
				} else {
					this->transitionState.reset(nullptr);
				}
			} else {
				this->transitionState.reset(nullptr);
			}
		} else {
			if (this->transitionState) {
				if (!this->transitionState->IsDone()) {
					currentState = this->transitionState.get();
				} else {
					this->transitionState.reset(nullptr);
				}
			}
		}

		// Handles updating the camera
		if (currentState) {

			auto player = PlayerCharacter::GetSingleton();
			bool isProne = IsProne(player);

			// Get scale based on camera state
			float scale = currentState->GetScale();

			// Do any scale overrides
			auto playerData = Persistent::GetSingleton().GetData(player);
			if (playerData) {
				playerData.scaleOverride = currentState->GetScaleOverride();
			}

			// Get current camera position in player space
			auto cameraPosLocal  = GetCameraPosLocal();

			// Get either normal or combat offset
			NiPoint3 offset;
			if (player != nullptr && player->IsWeaponDrawn()) {
				offset = currentState->GetCombatOffset(cameraPosLocal, isProne);
			} else {
				offset = currentState->GetOffset(cameraPosLocal, isProne);
			}

			NiPoint3 playerLocalOffset = currentState->GetPlayerLocalOffset(cameraPosLocal, isProne);

			if (currentState->PermitManualEdit()) {
				this->smoothOffset.target = this->manualEdit;
			}

			offset += this->smoothOffset.value;
			this->smoothScale.target = scale;

			// Apply camera scale and offset
			UpdateCamera(this->smoothScale.value, offset, playerLocalOffset);
		}
	}

	CameraState* CameraManager::GetCameraStateTP() {
		int cameraMode = Runtime::GetInt("CameraMode");
		// Third Person states
		// 0 is disabled
		// 1 is normal
		// 2 is alt camera
		// 3 is Between Feet
		// 4 is Left Feet
		// 5 is Right Feet
		switch (cameraMode) {
			case 1: {
				return &this->normalState;
			}
			case 2: {
				return &this->altState;
			}
			case 3: {
				return &this->footState;
			}
			case 4: {
				return &this->footLState;
			}
			case 5: {
				return &this->footRState;
			}
			default: {
				return nullptr;
			}
		}
	}

	CameraState* CameraManager::GetCameraStateFP() {
		// First Person states
		// 0 is normal
		// 1 is combat
		// 2 is loot
		int FirstPersonMode = Runtime::GetInt("FirstPersonMode");
		switch (FirstPersonMode) {
			case 0: {
				return &this->fpState;
			}
			case 1: {
				return &this->fpCombatState;
			}
			case 2: {
				return &this->fpLootState;
			}
			default: {
				return nullptr;
			}
		}
	}

	// Decide which camera state to use
	CameraState* CameraManager::GetCameraState() {
		if (!Runtime::GetBool("EnableCamera") || IsFreeCamera()) {
			return nullptr;
		}
		//=========================================================================

		bool AllowFpCamera = true; // !!!!!Disabled for global release for now!!!!!

		//^^^^^^^===================================================================
		auto playerCamera = PlayerCamera::GetSingleton();
		if (!playerCamera) {
			return nullptr;
		}
		auto playerCameraState = playerCamera->currentState;
		if (!playerCameraState) {
			return nullptr;
		}
		RE::CameraState playerCameraMode = playerCameraState->id;
		switch (playerCameraMode) {
			// Fp state
			case RE::CameraState::kFirstPerson: {
				if (AllowFpCamera) {
					return this->GetCameraStateFP();
				} else {
					return nullptr;
				}
			}
			// All these are TP like states
			case RE::CameraState::kThirdPerson:
			case RE::CameraState::kAutoVanity:
			case RE::CameraState::kFurniture:
			case RE::CameraState::kMount:
			case RE::CameraState::kBleedout:
			case RE::CameraState::kDragon: {
				return this->GetCameraStateTP();
			}
			// These ones should be scaled but not adjusted
			// any other way like pointing at feet when using
			// kIronSights
			case RE::CameraState::kVATS:
			case RE::CameraState::kFree:
			case RE::CameraState::kPCTransition:
			case RE::CameraState::kIronSights: {
				return &this->scaledVanillaState;
			}
			// These should not be touched at all
			case RE::CameraState::kTween:
			case RE::CameraState::kAnimated: {
				return nullptr;
			}
			// Catch all in case I forgot something
			default: {
				return nullptr;
			}
		}
	}

	void CameraManager::AdjustUpDown(float amt) {
		this->manualEdit.z += amt;
	}
	void CameraManager::ResetUpDown() {
		this->manualEdit.z = 0.0;
	}

	void CameraManager::AdjustLeftRight(float amt) {
		this->manualEdit.x += amt;
	}
	void CameraManager::ResetLeftRight() {
		this->manualEdit.x = 0.0;
	}
}
