#include "fg_pch.hpp"

#include "UUID.hpp"

#include <random>

namespace ForgottenEngine {

static std::random_device random_device;
static std::mt19937_64 eng(random_device());
static std::uniform_int_distribution<uint64_t> distribution;

static std::mt19937 eng32(random_device());
static std::uniform_int_distribution<uint32_t> distribution32;

UUID::UUID()
	: uuid(distribution(eng))
{
}

UUID::UUID(uint64_t uuid)
	: uuid(uuid)
{
}

UUID::UUID(const UUID& other) = default;


UUID32::UUID32()
	: uuid(distribution32(eng32))
{
}

UUID32::UUID32(uint32_t uuid)
	: uuid(uuid)
{
}

UUID32::UUID32(const UUID32& other) = default;

}