#include "utils/fmt.hpp"


auto fmt::formatter<ACTOR_VALUE_MODIFIER>::format(ACTOR_VALUE_MODIFIER v, format_context& ctx) const {
  string_view name = "unknown";
  switch (v) {
    case ACTOR_VALUE_MODIFIER::kPermanent: name = "kPermanent"; break;
    case ACTOR_VALUE_MODIFIER::kTemporary: name = "kTemporary"; break;
    case ACTOR_VALUE_MODIFIER::kDamage: name = "kDamage"; break;
  }
  return formatter<string_view>::format(name, ctx);
}
