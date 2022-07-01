//
// Created by Edwin Carlsson on 2022-07-01.
//

#pragma once

#include <memory>

namespace ForgottenEngine {

template <typename T, typename Arg> std::shared_ptr<T> shared(Arg&& arg)
{
	return std::shared_ptr<T>(new T(std::forward<Arg>(arg)));
};

};