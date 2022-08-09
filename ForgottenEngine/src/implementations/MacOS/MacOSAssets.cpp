#include "fg_pch.hpp"

#include <sys/stat.h>

#include "Assets.hpp"

namespace ForgottenEngine {

bool Assets::exists(const Path& path)
{
	struct stat buffer { };
	return (stat(path.c_str(), &buffer) == 0);
}

}
