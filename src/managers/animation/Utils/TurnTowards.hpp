#pragma once
#include "node.hpp"
#define _USE_MATH_DEFINES
#include <cmath>

using namespace RE;

namespace Gts {

	template<typename U>
	bool FaceTowardsPoint(U& anyTiny, const NiPoint3& point) {
		Actor* tiny =  GetActorPtr(anyTiny);

		if (!tiny) {
			return false;
		}

		auto tinyPos = tiny->GetPosition();

    NiPoint3 forwards = NiPoint3(0, 1, 0);
    NiPoint3 direction = point - tinyPos;
    direction.z = 0;
    direction = direction / direction.Length();
    float newAngle = acos(forwards.Dot(direction));

		tiny->data.angle.z = newAngle;

		return true;
	}

	template<typename T, typename U>
	bool FaceTowards(T& anyGiant, U& anyTiny, std::string_view bone_name) {
		Actor* giant = GetActorPtr(anyGiant);
		if (!giant) {
			return false;
		}
		auto bone = find_node(giant, bone_name);
		if (!bone) {
			return false;
		}
		return FaceTowardsPoint(anyTiny, bone->world.translate);
	}

	template<typename T, typename U>
	bool FaceTowards(T& anyGiant, U& anyTiny) {
		return FaceTowardsPoint(anyTiny, anyGiant->GetPosition());
	}

  // Specialise so we can just use FaceTowards instead of FaceTowardsPoint
  // for all
  template<typename T>
	bool FaceTowards(T& anyTiny, const NiPoint3& point) {
    return FaceTowardsPoint(anyTiny, point);
  }

  template<typename T, typename U>
	bool FaceSame(T& anyGiant, U& anyTiny) {
		Actor* giant = GetActorPtr(anyGiant);
		if (!giant) {
			return false;
		}
    Actor* tiny = GetActorPtr(anyTiny);
		if (!tiny) {
			return false;
		}
    auto giantAngle = giant->data.angle.z;
    tiny->data.angle.z = giantAngle;
		return true;
	}

  template<typename T, typename U>
	bool FaceOpposite(T& anyGiant, U& anyTiny) {
		Actor* giant = GetActorPtr(anyGiant);
		if (!giant) {
			return false;
		}
    Actor* tiny = GetActorPtr(anyTiny);
		if (!tiny) {
			return false;
		}
    auto giantAngle = giant->data.angle.z;
    float opposite = giantAngle - M_PI;
    if (opposite < 0.0) {
      opposite += 2*M_PI;
    }
    tiny->data.angle.z = opposite;
		return true;
	}

}
