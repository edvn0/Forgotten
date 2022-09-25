#include "fg_pch.hpp"

#include "Reference.hpp"

#include <mutex>
#include <unordered_set>

namespace ForgottenEngine {

	static std::unordered_set<void*> live_references;
	static std::mutex live_reference_mutex;

	namespace RefUtils {

		void add_to_live_references(void* instance)
		{
			std::scoped_lock<std::mutex> lock(live_reference_mutex);
			core_assert(instance != nullptr, "No instance");
			live_references.insert(instance);
		}

		void remove_from_live_references(void* instance)
		{
			std::scoped_lock<std::mutex> lock(live_reference_mutex);
			core_assert(instance != nullptr, "No instance");
			core_assert(is_in_map(live_references, instance), "Could not find instance");
			live_references.erase(instance);
		}

		bool is_live(void* instance)
		{
			core_assert(instance != nullptr, "No instance");
			return live_references.find(instance) != live_references.end();
		}
	} // namespace RefUtils

} // namespace ForgottenEngine
