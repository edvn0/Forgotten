#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

// "UUID" (universally unique identifier) or GUID is (usually) a 128-bit integer
// used to "uniquely" identify information. In ForgottenEngine, even though we use the term
// GUID and UUID, at the moment we're simply using a randomly generated 64-bit
// integer, as the possibility of a clash is low enough for now.
// This may change in the future.
class UUID
{
public:
	UUID();
	UUID(uint64_t uuid);
	UUID(const UUID& other);

	operator uint64_t () { return uuid; }
	operator const uint64_t () const { return uuid; }
private:
	uint64_t uuid;
};

class UUID32
{
public:
	UUID32();
	UUID32(uint32_t uuid);
	UUID32(const UUID32& other);

	operator uint32_t () { return uuid; }
	operator const uint32_t() const { return uuid; }
private:
	uint32_t uuid;
};
		
}

namespace std {

template <>
struct hash<ForgottenEngine::UUID>
{
	std::size_t operator()(const ForgottenEngine::UUID& uuid) const
	{
		// uuid is already a randomly generated number, and is suitable as a hash key as-is.
		// this may change in future, in which case return hash<uint64_t>{}(uuid); might be more appropriate
		return uuid;
	}
};

template <>
struct hash<ForgottenEngine::UUID32>
{
	std::size_t operator()(const ForgottenEngine::UUID32& uuid) const
	{
		return hash<uint32_t>()((uint32_t)uuid);
	}
};
}