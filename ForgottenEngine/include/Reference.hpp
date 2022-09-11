#pragma once

#include <atomic>
#include <concepts>
#include <stdint.h>

namespace ForgottenEngine {

	namespace RefUtils {
		void add_to_live_references(void* instance);
		void remove_from_live_references(void* instance);
		bool is_live(void* instance);
	} // namespace RefUtils

	class ReferenceCounted {
	public:
		void inc_ref_count() const { ++ref_count; }
		void dec_ref_count() const { --ref_count; }

		uint32_t get_ref_count() const { return ref_count.load(); }

	private:
		mutable std::atomic<uint32_t> ref_count = 0;
	};

	template <typename T> class Reference {
	public:
		Reference()
			: instance(nullptr)
		{
		}

		Reference(std::nullptr_t n)
			: instance(nullptr)
		{
		}

		Reference(T* instance)
			: instance(instance)
		{
			inc_ref();
		}

		template <typename Other> Reference(const Reference<Other>& other)
		{
			instance = reinterpret_cast<T*>(other.instance);
			inc_ref();
		}

		template <typename Other> Reference(Reference<Other>&& other)
		{
			instance = reinterpret_cast<T*>(other.instance);
			other.instance = nullptr;
		}

		static Reference<T> copy_without_increment(const Reference<T>& other)
		{
			Reference<T> result = nullptr;
			result->instance = other.instance;
			return result;
		}

		~Reference() { dec_ref(); }

		Reference(const Reference& other)
			: instance(other.instance)
		{
			inc_ref();
		}

		Reference& operator=(std::nullptr_t)
		{
			dec_ref();
			instance = nullptr;
			return *this;
		}

		Reference& operator=(const Reference& other)
		{
			other.inc_ref();
			dec_ref();

			instance = other.instance;
			return *this;
		}

		template <typename T2> Reference& operator=(const Reference<T2>& other)
		{
			other.inc_ref();
			dec_ref();

			instance = other.instance;
			return *this;
		}

		template <typename T2> Reference& operator=(Reference<T2>&& other)
		{
			dec_ref();

			instance = other.instance;
			other.instance = nullptr;
			return *this;
		}

		operator bool() { return instance != nullptr; }
		operator bool() const { return instance != nullptr; }

		T* operator->() { return instance; }
		const T* operator->() const { return instance; }

		T& operator*() { return *instance; }
		const T& operator*() const { return *instance; }

		T* raw() { return instance; }
		const T* raw() const { return instance; }

		void reset(T* in_instance = nullptr)
		{
			dec_ref();
			instance = in_instance;
		}

		template <typename T2> Reference<T2> as() const { return Reference<T2>(*this); }

		template <typename... Args> static Reference<T> create(Args&&... args) { return Reference<T>(new T(std::forward<Args>(args)...)); }

		bool operator==(const Reference<T>& other) const { return instance == other.instance; }

		bool operator!=(const Reference<T>& other) const { return !(*this == other); }

		bool equals_object(const Reference<T>& other)
		{
			if (!instance || !other.instance)
				return false;

			return *instance == *other.instance;
		}

	private:
		void inc_ref() const
		{
			if (instance) {
				instance->inc_ref_count();
				RefUtils::add_to_live_references((void*)instance);
			}
		}

		void dec_ref() const
		{
			if (instance) {
				instance->dec_ref_count();
				if (instance->get_ref_count() == 0) {
					delete instance;
					RefUtils::remove_from_live_references((void*)instance);
					instance = nullptr;
				}
			}
		}

		template <typename OtherT> friend class Reference;
		mutable T* instance;
	};

	template <typename T, typename... Args> static Reference<T> make(Args&&... args) { return Reference<T>(new T(std::forward<Args>(args)...)); }

	template <typename T> class WeakReference {
	public:
		WeakReference() = default;

		WeakReference(Reference<T> reference) { instance = reference.raw(); }

		WeakReference(T* in_instance) { instance = in_instance; }

		T* operator->() { return instance; }
		const T* operator->() const { return instance; }

		T& operator*() { return *instance; }
		const T& operator*() const { return *instance; }

		[[nodiscard]] bool is_valid() const { return instance && RefUtils::is_live(instance); }
		operator bool() const { return is_valid(); }

	private:
		T* instance = nullptr;
	};

} // namespace ForgottenEngine
