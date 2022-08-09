#include "fg_pch.hpp"

#include "Assets.hpp"

#include <Windows.h>

namespace ForgottenEngine {

bool Assets::exists(const Path& p)
{
	const char* f = p.string().c_str();
	DWORD dwAttrib = GetFileAttributes(f);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

}
