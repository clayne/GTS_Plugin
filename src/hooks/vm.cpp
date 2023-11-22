#include "hooks/vm.hpp"
#include "events.hpp"

using namespace RE;
using namespace RE::BSScript;
using namespace RE::BSScript::Internal;
using namespace Gts;

namespace Hooks
{
	// BGSImpactManager
	void Hook_VM::Hook() {
		log::info("Hooking VirtualMachine");
		REL::Relocation<std::uintptr_t> Vtbl{ VTABLE_BSScript__Internal__VirtualMachine[0] };
		_SendEvent = Vtbl.write_vfunc(REL::Relocate(0x24, 0x24, 0x26), SendEvent);
	}

	void Hook_VM::SendEvent(VirtualMachine* a_this, VMHandle a_handle, const BSFixedString& a_eventName, IFunctionArguments* a_args) {
		_SendEvent(a_this, a_handle, a_eventName, a_args);
		std::string event_name = a_eventName.c_str();
		if (event_name == "OnUpdate") {
			EventDispatcher::DoPapyrusUpdate();
		}
	}
}
