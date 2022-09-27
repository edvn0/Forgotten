//
// Created by Edwin Carlsson on 2022-09-27.
//

#pragma once

#include "vendor/magic-enum/include/magic_enum.hpp"

namespace ForgottenEngine::Enumeration {

	template <typename Enum> decltype(auto) constexpr inline enum_name(const Enum& enum_field) { return magic_enum::enum_name(enum_field); }

} // namespace ForgottenEngine::Enumeration