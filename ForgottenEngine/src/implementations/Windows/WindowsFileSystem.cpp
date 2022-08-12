#include "fg_pch.hpp"

#include "Application.hpp"
#include "utilities/FileSystem.hpp"

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <filesystem>

namespace ForgottenEngine {

	FileSystem::FileSystemChangedCallbackFn FileSystem::s_Callback;

	static bool s_Watching = false;
	static bool s_IgnoreNextChange = false;

	void FileSystem::set_change_callback(const FileSystemChangedCallbackFn& callback) { s_Callback = callback; }

	void FileSystem::start_watching() { s_Watching = true; }

	void FileSystem::stop_watching()
	{
		if (!s_Watching)
			return;

		s_Watching = false;
	}

	void FileSystem::skip_next_fs_change() { s_IgnoreNextChange = true; }

	unsigned long FileSystem::watch(void* param)
	{

		static const std::filesystem::path assetDirectory = "resources";
		std::string dirStr = assetDirectory.string();

		char buf[2048];
		DWORD bytesReturned;
		std::filesystem::path filepath;
		BOOL result = TRUE;

		HANDLE directoryHandle = CreateFile(dirStr.c_str(), GENERIC_READ | FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

		if (directoryHandle == INVALID_HANDLE_VALUE) {
			CORE_VERIFY(false, "Failed to open directory!");
			return 0;
		}

		OVERLAPPED pollingOverlap;
		pollingOverlap.OffsetHigh = 0;
		pollingOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		std::vector<FileSystemChangedEvent> eventBatch;
		eventBatch.reserve(10);

		while (s_Watching && result) {
			result = ReadDirectoryChangesW(directoryHandle, &buf, sizeof(buf), TRUE,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE, &bytesReturned,
				&pollingOverlap, NULL);

			WaitForSingleObject(pollingOverlap.hEvent, INFINITE);

			if (s_IgnoreNextChange) {
				s_IgnoreNextChange = false;
				eventBatch.clear();
				continue;
			}

			FILE_NOTIFY_INFORMATION* pNotify;
			int offset = 0;
			std::wstring oldName;

			do {
				pNotify = (FILE_NOTIFY_INFORMATION*)((char*)buf + offset);
				size_t filenameLength = pNotify->FileNameLength / sizeof(wchar_t);

				FileSystemChangedEvent e;
				e.FilePath = std::filesystem::path(std::wstring(pNotify->FileName, filenameLength));
				e.IsDirectory = false; // TODO

				switch (pNotify->Action) {
				case FILE_ACTION_ADDED: {
					e.Action = FileSystemAction::Added;
					break;
				}
				case FILE_ACTION_REMOVED: {
					e.Action = FileSystemAction::Delete;
					break;
				}
				case FILE_ACTION_MODIFIED: {
					e.Action = FileSystemAction::Modified;
					break;
				}
				case FILE_ACTION_RENAMED_OLD_NAME: {
					oldName = e.FilePath.filename();
					break;
				}
				case FILE_ACTION_RENAMED_NEW_NAME: {
					e.OldName = oldName;
					e.Action = FileSystemAction::Rename;
					break;
				}
				}

				// NOTE(Peter): Fix for https://gitlab.com/chernoprojects/Hazel-dev/-/issues/143
				bool hasAddedEvent = false;
				if (e.Action == FileSystemAction::Modified) {
					for (const auto& event : eventBatch) {
						if (event.FilePath == e.FilePath && event.Action == FileSystemAction::Added)
							hasAddedEvent = true;
					}
				}

				if (pNotify->Action != FILE_ACTION_RENAMED_OLD_NAME && !hasAddedEvent)
					eventBatch.push_back(e);

				offset += pNotify->NextEntryOffset;
			} while (pNotify->NextEntryOffset);

			if (eventBatch.size() > 0) {
				s_Callback(eventBatch);
				eventBatch.clear();
			}
		}

		CloseHandle(directoryHandle);
		return 0;

		return 0;
	}

	bool FileSystem::write_bytes(const std::filesystem::path& filepath, const Buffer& buffer)
	{
		std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);

		if (!stream) {
			stream.close();
			return false;
		}

		stream.write((char*)buffer.data, buffer.size);
		stream.close();

		return true;
	}

	Buffer FileSystem::read_bytes(const std::filesystem::path& filepath)
	{
		Buffer buffer;

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		CORE_ASSERT_BOOL(stream);

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint32_t size = end - stream.tellg();
		CORE_ASSERT(size != 0, "");

		buffer.allocate(size);
		stream.read((char*)buffer.data, buffer.size);
		stream.close();

		return buffer;
	}

	bool FileSystem::has_env_variable(const std::string& key) { return false; }

	bool FileSystem::set_env_variable(const std::string& key, const std::string& value) { return false; }

	std::string FileSystem::get_env_variable(const std::string& key) { return std::string {}; }

} // namespace ForgottenEngine
