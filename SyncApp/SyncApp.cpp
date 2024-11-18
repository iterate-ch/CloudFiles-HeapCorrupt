#include <Windows.h>
#include <cfapi.h>
#include <winrt/windows.storage.h>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

struct cf_protected_handle_traits
{
	using type = HANDLE;

	static void close(type value) noexcept
	{
		CfCloseHandle(value);
	}

	static type invalid() noexcept
	{
		return INVALID_HANDLE_VALUE;
	}
};

using cf_protected_handle = winrt::handle_type<cf_protected_handle_traits>;

using namespace winrt;

int WINAPI main()
{
	std::filesystem::path path(winrt::Windows::Storage::ApplicationData::Current().LocalCacheFolder().Path().data());
	auto mount = path / "Mount";
	std::filesystem::create_directories(mount);

	{
		std::string identity("CA2-Identity");
		CF_SYNC_REGISTRATION registration =
		{
			sizeof(CF_SYNC_REGISTRATION),
			L"Sample Provider",
			L"1.0.0",
			identity.c_str(),
			identity.length(),
			nullptr,
			0,
			GUID_NULL
		};
		CF_SYNC_POLICIES policies =
		{
			sizeof(CF_SYNC_POLICIES),
			{ CF_HYDRATION_POLICY_ALWAYS_FULL, CF_HYDRATION_POLICY_MODIFIER_NONE },
			{ CF_POPULATION_POLICY_ALWAYS_FULL, CF_POPULATION_POLICY_MODIFIER_NONE },
			CF_INSYNC_POLICY_NONE,
			CF_HARDLINK_POLICY_NONE,
			CF_PLACEHOLDER_MANAGEMENT_POLICY_DEFAULT
		};
		check_hresult(CfRegisterSyncRoot(
			mount.wstring().c_str(),
			&registration,
			&policies,
			CF_REGISTER_FLAG_MARK_IN_SYNC_ON_ROOT | CF_REGISTER_FLAG_DISABLE_ON_DEMAND_POPULATION_ON_ROOT
		));
	}

	SYSTEMTIME time;
	GetSystemTime(&time);
	LARGE_INTEGER filetime;
	SystemTimeToFileTime(&time, (FILETIME*)&filetime);
	auto file = mount / "__TEST";

	if (!std::filesystem::exists(file))
	{
		std::string placeholderIdentity("CA2-Identity");
		auto relativeFileName{ file.filename().wstring() };
		CF_PLACEHOLDER_CREATE_INFO testFile =
		{
			relativeFileName.c_str(),
			{
				{
					filetime,
					filetime,
					filetime,
					filetime,
					FILE_ATTRIBUTE_NORMAL
				},
				0
			},
			placeholderIdentity.c_str(),
			placeholderIdentity.length(),
			CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC,
			0,
			0
		};
		DWORD processed;
		check_hresult(
			CfCreatePlaceholders(mount.wstring().c_str(), &testFile, 1, CF_CREATE_FLAG_NONE, &processed)
		);
	}
	for (auto i = 0; i < 300; i++) {
		wprintf_s(L"%d", i);

		cf_protected_handle handle;
		check_hresult(
			CfOpenFileWithOplock(file.wstring().c_str(), CF_OPEN_FILE_FLAG_NONE, handle.put())
		);
		if (!CfReferenceProtectedHandle(handle.get())) {
			throw_last_error();
		}

		auto result = CfUpdatePlaceholder(
			CfGetWin32HandleFromProtectedHandle(handle.get()),
			nullptr,
			nullptr,
			0,
			nullptr,
			0,
			CF_UPDATE_FLAG_NONE,
			nullptr,
			nullptr
		);
		CfReleaseProtectedHandle(handle.get());
		check_hresult(result);

		std::this_thread::sleep_for(std::chrono::milliseconds(150));
	}
}
