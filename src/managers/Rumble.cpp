// Module that handles rumbling
#include "managers/Rumble.hpp"
#include "events.hpp"
#include "timer.hpp"
#include "spring.hpp"
#include "data/time.hpp"

using namespace std;
using namespace SKSE;
using namespace RE;

namespace Gts {

	RumbleData::RumbleData(float intensity, float duration, std::string_view node) {
		this->state = RumpleState::RampingUp;
		this->duration = duration;
		this->currentIntensity.target = intensity;
		this->currentIntensity.halflife = 0.5; // How fast rumbles start/stop
		this->node = node;
		this->startTime = 0.0;
	}

	void RumbleData::ChangeTargetIntensity(float intensity) {
		this->currentIntensity.target = intensity;
		this->state = RumpleState::RampingUp;
		this->startTime = 0.0;
	}
	void RumbleData::ChangeDuration(float duration) {
		this->duration = duration;
		this->state = RumpleState::RampingUp;
		this->startTime = 0.0;
	}

	ActorRumbleData::ActorRumbleData() {
		this->delay = Timer(0.40);
		this->tags = {};
	}

	Rumble& Rumble::GetSingleton() noexcept {
		static Rumble instance;
		return instance;
	}

	std::string Rumble::DebugName() {
		return "Rumble";
	}

	void Rumble::Reset() {
		this->data.clear();
	}
	void Rumble::ResetActor(Actor* actor) {
		this->data.erase(actor);
	}

	void Rumble::Start(std::string_view tag, Actor* giant, float intensity, std::string_view node) {
		Rumble::For(tag, giant, intensity, node, 0);
	}
  void Rumble::Start(std::string_view tag, Actor* giant, float intensity) {
		Rumble::For(tag, giant, intensity, "NPC Root [Root]", 0);
	}
	void Rumble::Stop(std::string_view tag, Actor* giant) {
		try {
			this->data.at(giant).tags.at(tag).state = RumpleState::RampingDown;
		} catch (std::out_of_range e) {}
	}

	void Rumble::For(std::string_view tag, Actor* giant, float intensity, std::string_view node, float duration) {
		this->data.try_emplace(giant);
		this->data[giant].tags.try_emplace(tag, intensity, duration, node);
		// Reset if alreay there (but don't reset the intensity this will let us smooth into it)
		this->data[giant].tags[tag].ChangeTargetIntensity(intensity);
		this->data[giant].tags[tag].ChangeDuration(duration);
	}

	void Rumble::Once(std::string_view tag, Actor* giant, float intensity, std::string_view node) {
		Rumble::For(tag, giant, intensity, node, 1.0);
	}

  void Rumble::Once(std::string_view tag, Actor* giant, float intensity) {
	  Rumble::Once(tag, giant, intensity, "NPC Root [Root]");
	}


	void Rumble::Update() {
		for (const auto &[actor, data]: this->data) {
			if (data.delay.ShouldRun()) {
				// Update values based on time passed
				float dt = data.timer.TimeDelta();
				for (const auto &[tag, rumbleData]: data.tags) {
					switch (rumbleData.state) {
						case RumpleState::RampingUp: {
							// Increasing intensity just let the spring do its thing
							if (fabs(rumbleData.currentIntensity.value - rumbleData.currentIntensity.target)) {
								< 1e-3 {
									// When spring is done move the state onwards
									rumbleData.state = RumpleState::Rumbling;
									rumbleData.startTime = Time::WorldTimeElapsed();
								}
							}
							break;
						}
						case RumpleState::Rumbling: {
							// At max intensity
							rumbleData.currentIntensity.value = rumbleData.currentIntensity.target;
							if (Time::WorldTimeElapsed() > rumbleData.startTime + rumbleData.duration) {
								rumbleData.state = RumpleState::RampingDown;
							}
							break;
						}
						case RumpleState::RampingDown: {
							// Stoping the rumbling
							rumbleData.currentIntensity.target = 0; // Ensure ramping down is going to zero intensity
							rumbleData.currentIntensity.Update(dt);
							if (fabs(rumbleData.currentIntensity.value) <= 1e-3) {
								// Stopped
								rumbleData.state = RumpleState::Still;
							}
							break;
						}
						case RumpleState::Still: {
							// All finished cleanup
							data.erase(tag);
						}
					}
				}
				// Now collect the data
				//    - Multiple effects can add rumble to the same node
				//    - We sum those effects up into cummulativeIntensity
				std::unordered_map<NiAVObject*, float> cummulativeIntensity;
				for (const auto &[tag, rumbleData]: data.tags) {
					auto node = find_node(actor, rumbleData.node);
					if (node) {
						cummulativeIntensity.try_emplace(node);
						cummulativeIntensity[node] += rumbleData.currentIntensity.value;
					}
				}
				// Now do the rumble
				//   - Also add up the volume for the rumble
				float cummulativeVolume = (4 * get_visual_scale(caster))/get_distance_to_camera(attach->world.translate);
				for (const auto &[node, intensity]: cummulativeIntensity) {
					ApplyShakeAtPoint(actor, 0.4 * intensity, node->world.translate);
					cummulativeVolume *= modifier;
				}
				// Lastly play the sound
				Runtime::PlaySoundAtNode("RumbleWalkSound", actor, cummulativeVolume, 1.0, node);
			}
		}
	}
}
