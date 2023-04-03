#include "managers/animation/AnimationManager.hpp"
#include "managers/animation/ThighSandwich.hpp"
#include "managers/ThighSandwichController.hpp"
#include "managers/GtsSizeManager.hpp"
#include "managers/InputManager.hpp"
#include "managers/CrushManager.hpp"
#include "managers/explosion.hpp"
#include "managers/footstep.hpp"
#include "managers/tremor.hpp"
#include "managers/Rumble.hpp"
#include "data/runtime.hpp"
#include "scale/scale.hpp"
#include "spring.hpp"
#include "node.hpp"

 namespace {

    const float MINIMUM_SANDWICH_DISTANCE = 70.0;
	const float MINIMUM_SANDWICH_SCALE_RATIO = 6.0;
	const float SANDWICH_ANGLE = 60;
	const float PI = 3.14159;

    void PrintSuffocate(Actor* pred, Actor* prey) {
        int random = rand() % 3;
        if (random <= 1) {
			ConsoleLog::GetSingleton()->Print("%s was suffocated by the thighs of %s", prey->GetDisplayFullName(), pred->GetDisplayFullName());
		} else if (random == 2) {
			ConsoleLog::GetSingleton()->Print("Thighs of %s suffocated %s to death", pred->GetDisplayFullName(), prey->GetDisplayFullName());
		} else if (random == 3) {
			ConsoleLog::GetSingleton()->Print("%s got smothered between the thighs of %s", prey->GetDisplayFullName(), pred->GetDisplayFullName());
		}
    }
    
    [[nodiscard]] inline RE::NiPoint3 RotateAngleAxis(const RE::NiPoint3& vec, const float angle, const RE::NiPoint3& axis)
	{
		float S = sin(angle);
		float C = cos(angle);

		const float XX = axis.x * axis.x;
		const float YY = axis.y * axis.y;
		const float ZZ = axis.z * axis.z;

		const float XY = axis.x * axis.y;
		const float YZ = axis.y * axis.z;
		const float ZX = axis.z * axis.x;

		const float XS = axis.x * S;
		const float YS = axis.y * S;
		const float ZS = axis.z * S;

		const float OMC = 1.f - C;

		return RE::NiPoint3(
			(OMC * XX + C) * vec.x + (OMC * XY - ZS) * vec.y + (OMC * ZX + YS) * vec.z,
			(OMC * XY + ZS) * vec.x + (OMC * YY + C) * vec.y + (OMC * YZ - XS) * vec.z,
			(OMC * ZX - YS) * vec.x + (OMC * YZ + XS) * vec.y + (OMC * ZZ + C) * vec.z
		);
	}
 }   


 namespace Gts {   
    SandwichingData::SandwichingData(Actor* giant) : giant(giant) {

	}

    std::vector<Actor*> SandwichingData::GetActors() {
		std::vector<Actor*> result;
		for (auto& [key, actor]: this->tinies) {
			result.push_back(actor);
		}
		return result;
	}

    ThighSandwichController& ThighSandwichController::GetSingleton() noexcept {
		static ThighSandwichController instance;
		return instance;
	}

	std::string ThighSandwichController::DebugName() {
		return "ThighSandwichController";
	}

	void SandwichData::UpdateRune(Actor* giant) {
		string node_name = "AnimObjectB";
		if (this->RuneScale) {
			auto node = find_node(actor, node_name, false);
			if (node) {
				node->local.scale = this->ScaleRune;
				update_node(node);
				if (node->local.scale >= 1.0) {
					this->RuneScale = false;
					node->local.scale = 1.0;
					update_node(node);
					return;
				} 
			} 
		} else if (this->RuneShrink) {
			auto node = find_node(actor, node_name, false);
			if (node) {
				node->local.scale = this->ShrinkRune;
				update_node(node);
				if (node->local.scale <= 0.05) {
					node->local.scale = 0.05;
					update_node(node);
					this->RuneShrink = false;
					return;
				}
			} 
		}
	}

    void SandwichingData::Update() {
        auto giant = this->giant;
        if (!giant) {
            return;
        }
    	float giantScale = get_visual_scale(giant);
		this->UpdateRune(giant);

        for (auto& [key, tiny]: this->tinies) {
			if (!tiny) {
				return;
			}
			auto bone = find_node(giant, "AnimObjectA");
			if (!bone) {
				return;
			}
			NiPoint3 tinyLocation = tiny->GetPosition();
			NiPoint3 targetLocation = bone->world.translate;
        	NiPoint3 deltaLocation = targetLocation - tinyLocation;
        	float deltaLength = deltaLocation.Length();

			tiny->SetPosition(targetLocation, true);
			tiny->SetPosition(targetLocation, false);
            if (this->Suffocate) {
                float sizedifference = get_visual_scale(giant)/get_visual_scale(tiny);
			    float damage = 0.02 * sizedifference;
                float hp = GetAV(tiny, ActorValue::kHealth);
			    DamageAV(tiny, ActorValue::kHealth, damage);
                if (damage > hp && !tiny->IsDead()) {
                    this->Remove(tiny);
                    PrintSuffocate(giant, tiny);
                }
            }
			Actor* tiny_is_actor = skyrim_cast<Actor*>(tiny);
			if (tiny_is_actor) {
				auto charcont = tiny_is_actor->GetCharController();
				if (charcont) {
					charcont->SetLinearVelocityImpl((0.0, 0.0, -5.0, 0.0)); // Needed so Actors won't fall down.
				}
            }
        }
    }

    void ThighSandwichController::Update() {
        for (auto& [key, SandwichData]: this->data) {
			SandwichData.Update();
		}
    }

	std::vector<Actor*> ThighSandwichController::GetSandwichTargetsInFront(Actor* pred, std::size_t numberOfPrey) {
		// Get vore target for actor
		if (!pred) {
			return {};
		}
		auto charController = pred->GetCharController();
		if (!charController) {
			return {};
		}

		NiPoint3 predPos = pred->GetPosition();

		auto preys = find_actors();

		// Sort prey by distance
		sort(preys.begin(), preys.end(),
		     [predPos](const Actor* preyA, const Actor* preyB) -> bool
		{
			float distanceToA = (preyA->GetPosition() - predPos).Length();
			float distanceToB = (preyB->GetPosition() - predPos).Length();
			return distanceToA < distanceToB;
		});

		// Filter out invalid targets
		preys.erase(std::remove_if(preys.begin(), preys.end(),[pred, this](auto prey)
		{
			return !this->CanSandwich(pred, prey);
		}), preys.end());;

		// Filter out actors not in front
		auto actorAngle = pred->data.angle.z;
		RE::NiPoint3 forwardVector{ 0.f, 1.f, 0.f };
		RE::NiPoint3 actorForward = RotateAngleAxis(forwardVector, -actorAngle, { 0.f, 0.f, 1.f });

		NiPoint3 predDir = actorForward;
		predDir = predDir / predDir.Length();
		preys.erase(std::remove_if(preys.begin(), preys.end(),[predPos, predDir](auto prey)
		{
			NiPoint3 preyDir = prey->GetPosition() - predPos;
			if (preyDir.Length() <= 1e-4) {
				return false;
			}
			preyDir = preyDir / preyDir.Length();
			float cosTheta = predDir.Dot(preyDir);
			return cosTheta <= 0; // 180 degress
		}), preys.end());

		// Filter out actors not in a truncated cone
		// \      x   /
		//  \  x     /
		//   \______/  <- Truncated cone
		//   | pred |  <- Based on width of pred
		//   |______|
		float predWidth = 70 * get_visual_scale(pred);
		float shiftAmount = fabs((predWidth / 2.0) / tan(SANDWICH_ANGLE/2.0));

		NiPoint3 coneStart = predPos - predDir * shiftAmount;
		preys.erase(std::remove_if(preys.begin(), preys.end(),[coneStart, predWidth, predDir](auto prey)
		{
			NiPoint3 preyDir = prey->GetPosition() - coneStart;
			if (preyDir.Length() <= predWidth*0.4) {
				return false;
			}
			preyDir = preyDir / preyDir.Length();
			float cosTheta = predDir.Dot(preyDir);
			return cosTheta <= cos(SANDWICH_ANGLE*PI/180.0);
		}), preys.end());

		// Reduce vector size
		if (preys.size() > numberOfPrey) {
			preys.resize(numberOfPrey);
		}

		return preys;
	}

	bool ThighSandwichController::CanSandwich(Actor* pred, Actor* prey) {
		if (pred == prey) {
			return false;
		}

		if (!Runtime::HasPerkTeam(pred, "VorePerk")) {
			return false;
		}

		float pred_scale = get_visual_scale(pred);
		float prey_scale = get_visual_scale(prey);
		if (IsDragon(prey)) {
			prey_scale *= 2.0;
		}

		float sizedifference = pred_scale/prey_scale;

		float MINIMUM_VORE_SCALE = MINIMUM_SANDWICH_SCALE_RATIO;

		float balancemode = SizeManager::GetSingleton().BalancedMode();

		if (balancemode == 2.0) { // This is checked only if Balance Mode is enabled. Enables HP requirement on Vore.
			float getmaxhp = GetMaxAV(prey, ActorValue::kHealth);
			float gethp = GetAV(prey, ActorValue::kHealth);
			float healthrequirement = getmaxhp/pred_scale;
			if (pred->formID == 0x14, gethp > healthrequirement) {
				DamageAV(prey, ActorValue::kHealth, 6 * sizedifference);
				DamageAV(pred, ActorValue::kStamina, 26/sizedifference);
				Notify("{} is too healthy to be eaten", prey->GetDisplayFullName());
				return false;
			}
		}

		float prey_distance = (pred->GetPosition() - prey->GetPosition()).Length();
		if (pred->formID == 0x14 && prey_distance <= (MINIMUM_SANDWICH_DISTANCE * pred_scale) && pred_scale/prey_scale < MINIMUM_VORE_SCALE) {
			return false;
		}
		if (prey_distance <= (MINIMUM_SANDWICH_DISTANCE * pred_scale) && pred_scale/prey_scale > MINIMUM_VORE_SCALE) {
				if ((prey->IsEssential() && Runtime::GetBool("ProtectEssentials")) || Runtime::HasSpell(prey, "StartVore")) {
					return false;
				} else {
					return true;
				}
			}
		else {
			return false;
		}
	}

	void ThighSandwichController::StartSandwiching(Actor* pred, Actor* prey) {
        auto& sandwiching = ThighSandwichController::GetSingleton();
		if (!sandwiching.CanSandwich(pred, prey)) {
			return;
		}
		auto& data = sandwiching.GetSandwichingData(pred);
		data.AddTiny(prey);
		AnimationManager::StartAnim("ThighEnter", pred);
	}

    void SandwichingData::AddTiny(Actor* tiny) {
		this->tinies.try_emplace(tiny, tiny);
	}

    void SandwichingData::ReleaseAll() {
		this->tinies.clear();
	}

    void SandwichingData::Remove(Actor* tiny) {
        this->tinies.erase(tiny);
    }

    void SandwichingData::EnableSuffocate(bool enable) {
		this->Suffocate = enable;
	}

    SandwichingData& ThighSandwichController::GetSandwichingData(Actor* giant) {
		// Create it now if not there yet
		this->data.try_emplace(giant, giant);

		return this->data.at(giant);
	}
 }