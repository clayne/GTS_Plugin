#pragma once
#include "events.hpp"
#include <functional>

using namespace std;
using namespace SKSE;
using namespace RE;

namespace Gts
{
  // This data is passed to an animation that is in progress
  //   It is created when StartAnim is called
  //   It is destroyed after ActorAnimEvent if stage == 0
  //    Therefore when an anim is finished set stage = 0 to tell
  //    the anim system that we are done
  //    If stage is not set to 0 when done then a new anim of the same kind
  //    cannot be started for this actor (as it will think the old anim
  //    hasn't finished yet)
  struct AnimationEventData {
    // Giant must exists and is the one the animation is happening on
    //   Maybe rename this to caster
    const Actor& giant;
    // This is the thing being grabbed. It is an TESObjectREFR so we could also
    //   pick up other objects
    // Set to nullptr for no object
    const TESObjectREFR* tiny;
    // Stage a value of 0 means finished and will be cleaned up
    std::size_t stage;
    // Anim speed, can be adjusted via the animation itself or via AdjustAnimSpeed
    float animSpeed;
    // Make true during a stage of the anim to allow animSpeed adjust via AdjustAnimSpeed
    bool canEditAnimSpeed = false;

    AnimationEventData(const Actor& giant, const TESObjectREFR* tiny);
  };

  // Holds data that links a animation tag to a callback and group
  struct AnimationEvent {
    // The callback to run
    std::function<void(AnimationEventData&) callback;
    // This is a tag used to link animation data from one call to another
    //   All animations of the same kind i.e Stomp has GTSstompimpactR, GTSstompimpactL etc
    //   will have the same group name.
    // When an animation is started data with this group name is created
    //  At every stage this data will be passed to any animation registered with this groupname
    std::string group;

    AnimationEvent(std::function<void(AnimationEventData&) callback, std::string group);
  };

  // Holds data that links a trigger to a behaviour and group
  //
  // These are just friendly names to start an animation
  struct TriggerData {
    // Name to send to the animation graph
    std::string behavor;
    // The name of the data to be created
    std::string group;

    TriggerData(std::string_view behavor, std::string_view group);
  };

	class AnimationManager : public EventListener
	{
		public:
			[[nodiscard]] static AnimationManager& GetSingleton() noexcept;

			virtual std::string DebugName() override;
      virtual void DataReady() override;
      virtual void Update() override;
			virtual void ActorAnimEvent(Actor* actor, const std::string_view& tag, const std::string_view& payload) override;


      // Change speed of all animations that are currently playing on the PLAYER
      // that have the data.canEditAnimSpeed == true
      //
      // Each anim gets it's own adjustable speed
      static void AdjustAnimSpeed(float bonus);

      // Register an animation event to a function callback
      //
      // Should be of the form:
      // ```cpp
      // AnimationManager::RegisterEvent("GTSstompimpactR", "Stomp", GTSstompimpactR);
      // AnimationManager::RegisterEvent("GTSstompimpactL", "Stomp", GTSstompimpactL);
      // ```
      //
      // Where group is name of the collection to share data for
      static void RegisterEvent(std::string_view name, std::string_view group, std::function<void(const AnimationEventData&)> func);


      // Register a trigger to a behavior to start
      //
      // This is used to link the friendly anim name to the complex behaviour name
      //  while also creating any associated animation data that matches the group name
      static void RegisterTrigger(std::string_view trigger, std::string_view group, std::string_view behavior);

      // Start an anuimation with NO object to carry
      static void StartAnim(std::string_view trigger, const Actor& giant);
      // Start the animation WITH an object to carry. We use TESObjectREFR over Actor so that we can pick up anything
      static void StartAnim(std::string_view trigger, const Actor& giant, const TESObjectREFR* tiny);

    protected:
      std::unordered_map<Actor*, std::unordered_map<std::string, AnimationEventData>> data;
      std::unordered_map<std::string, AnimationEvent>> eventCallbacks;
      std::unordered_map<std::string, TriggerData> triggers;
	};

  void ShakeAndSound(Actor* caster, float volume, const std::string_view& node);
}
