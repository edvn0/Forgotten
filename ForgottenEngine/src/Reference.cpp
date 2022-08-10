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
			CORE_ASSERT(instance, "No instance");
			live_references.insert(instance);
		}

		void remove_from_live_references(void* instance)
		{
			std::scoped_lock<std::mutex> lock(live_reference_mutex);
			CORE_ASSERT(instance, "No instance");
			CORE_ASSERT(live_references.find(instance) != live_references.end(), "Could not find instance");
			live_references.erase(instance);
		}

		bool is_live(void* instance)
		{
			CORE_ASSERT(instance, "No instance");
			return live_references.find(instance) != live_references.end();
		}
	} // namespace RefUtils

} // namespace ForgottenEngine
