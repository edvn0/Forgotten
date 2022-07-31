#pragma once

#include "Buffer.hpp"

#include <functional>
#include <filesystem>

namespace ForgottenEngine {

enum class FileSystemAction
{
	Added, Rename, Modified, Delete
};

struct FileSystemChangedEvent
{
	FileSystemAction Action;
	std::filesystem::path FilePath;
	bool IsDirectory;

	// If this is a rename event the new name will be in the FilePath
	std::wstring OldName = L"";
};

class FileSystem
{
public:
	static bool exists(const std::filesystem::path& filepath);
	static bool exists(const std::string& filepath);
	static bool is_directory(const std::filesystem::path& filepath);

	static bool write_bytes(const std::filesystem::path& filepath, const Buffer& buffer);
	static Buffer read_bytes(const std::filesystem::path& filepath);
public:
	using FileSystemChangedCallbackFn = std::function<void(const std::vector<FileSystemChangedEvent>&)>;

	static void set_change_callback(const FileSystemChangedCallbackFn& callback);
	static void start_watching();
	static void stop_watching();

	static std::filesystem::path open_file_dialog(const char* filter = "All\0*.*\0");
	static std::filesystem::path open_folder_dialog(const char* initialFolder = "");
	static std::filesystem::path save_file_dialog(const char* filter = "All\0*.*\0");

	static void skip_next_fs_change();

public:
	static bool has_env_variable(const std::string& key);
	static bool set_env_variable(const std::string& key, const std::string& value);
	static std::string get_env_variable(const std::string& key);

private:
	static unsigned long watch(void* param);

private:
	static FileSystemChangedCallbackFn s_Callback;
};
}
