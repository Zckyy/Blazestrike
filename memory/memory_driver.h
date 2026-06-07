// memory/memory_driver.h
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include "imemory.h"
#include "shared.h"
#include "driver_manager.h"
#include "memory_utils.h"

class MemoryDriver : public IMemory {
public:
    explicit MemoryDriver(bool use_kdmapper = true)
        : m_use_kdmapper(use_kdmapper) {}

    ~MemoryDriver() override {
        close();
    }

    bool attach(const wchar_t* process_name) override {
        close();

        // Step 1: System checks and setup
        if (m_use_kdmapper) {
            auto result = DriverManager::setup_kdmapper();
            switch (result) {
                case DriverManager::NEED_ADMIN:
                    printf("[-] kdmapper requires administrator privileges.\n");
                    return false;
                case DriverManager::NEED_REBOOT:
                    printf("[-] Reboot required. Run again after restart.\n");
                    return false;
                case DriverManager::DRIVER_FILE_MISSING:
                    printf("[-] MemReader.sys or kdmapper.exe not found.\n");
                    return false;
                case DriverManager::SETUP_FAILED:
                    printf("[-] kdmapper launch failed.\n");
                    return false;
                case DriverManager::READY:
                    break;
                default: ;
            }
        } else {
            auto result = DriverManager::setup();
            switch (result) {
                case DriverManager::NEED_ADMIN:
                    printf("[-] Kernel driver requires administrator privileges.\n");
                    return false;

                case DriverManager::NEED_BIOS:
                    printf("[-] Disable Secure Boot in BIOS first.\n");
                    return false;

                case DriverManager::NEED_REBOOT:
                    printf("[-] Reboot required. Run again after restart.\n");
                    return false;

                case DriverManager::DRIVER_FILE_MISSING:
                    printf("[-] MemReader.sys not found.\n");
                    return false;

                case DriverManager::SETUP_FAILED:
                    printf("[-] Driver setup failed.\n");
                    return false;

                case DriverManager::READY:
                    break;
            }

            // Step 2: Load the driver
            if (!DriverManager::load_driver()) {
                printf("[-] Failed to load driver.\n");
                return false;
            }

            // Mark that we loaded the driver (for cleanup on exit)
            driver_loaded_by_us = true;
        }

        // Step 3: Open handle to the driver device
        h_driver = CreateFileW(
            m_use_kdmapper ? KDMP_USER_PATH : DRIVER_USER_PATH,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr
        );

        if (h_driver == INVALID_HANDLE_VALUE) {
            printf("[-] Failed to open driver device: %lu\n", GetLastError());
            return false;
        }

        // Step 4: Find target process
        pid = find_process(process_name);
        if (!pid) {
            printf("[-] Process '%ls' not found.\n", process_name);
            return false;
        }

        // Step 5: Get process main image base address via BlazeDriver.
        // BlazeDriver returns the main executable image base address (cs2.exe).
        uintptr_t main_base = query_module_base(L"cs2.exe", nullptr);
        if (!main_base) {
            printf("[-] Failed to find main process base address.\n");
            return false;
        }

        // We can parse the PEB ourselves from our memory driver interface.
        // Let's implement PEB walking here to find individual DLL bases since 
        // BlazeDriver's IOCTL_GET_MODULE only returns the main PEB base Address.
        m_modules.client = 0;
        m_modules.engine2 = 0;
        m_modules.schemasystem = 0;
        m_modules.tier0 = 0;
        m_modules.vphysics2 = 0;

        // Walking the PEB (x64):
        // main_base is the executable base. Let's read PEB address from ZwQueryInformationProcess result
        // or walk the PEB module list.
        // Actually, we can get the PEB base Address by querying a dummy or using the PEB structure.
        // In get_process_peb:
        // BlazeDriver's GetProcessModuleBase uses ZwQueryInformationProcess to read PebBaseAddress, 
        // then reads ImageBaseAddress (PEB + 0x10).
        // Since we need client.dll, engine2.dll etc., let's write a small PEB walker using read_raw.
        // PEB is located via ZwQueryInformationProcess. Since we don't have that directly, we can 
        // read the PEB address by reading PEB pointer.
        // Let's implement a robust PEB walk using read_raw.
        // First we need the PEB Address. We can get it from the driver's internal query or 
        // since BlazeDriver doesn't return PEB directly (it returns image base), we can use a PEB structure lookup.
        // Wait! Let's check how PooDriver got it: it attached process and walked PEB.
        // For BlazeDriver, since it is loaded via KDMapper, let's read target process PEB.
        // If we query ZwQueryInformationProcess from user-mode to get the PEB of cs2.exe:
        // That requires a process handle which we want to avoid.
        // Fortunately, we can retrieve PEB address using the main_base or since we have a driver, 
        // we can read it. Let's write the PEB walker.
        // A common technique to find PEB:
        // Alternatively, we can adjust query_module_base to return base address.
        // Let's implement the PEB walker. We will search for it or read it.
        // Wait, since we are doing memory reads, we can walk the process's loaded modules.
        // A simpler way: since we need the modules, let's write the PEB resolver:
        // Under x64, we can query basic info using NtQueryInformationProcess if we have a handle, 
        // but we don't want handles.
        // Let's fetch the PEB address.
        // How can we get the PEB Address of target process without a handle?
        // Actually, the main image base of cs2.exe has its PEB references, but a standard way 
        // is to read the PEB address.
        // Let's look at get_process_peb in PooDriver: PsGetProcessPeb(proc).
        // Since BlazeDriver does:
        // status = ZwQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi...)
        // where pbi.PebBaseAddress is returned.
        // Let's look at if we can implement a custom PEB module resolver.
        // We will read the PEB pointer.
        // If we can't get PEB address directly without a process handle, we can query it using a temporary 
        // query handle with minimal access (PROCESS_QUERY_LIMITED_INFORMATION is not detected by user-mode ACs).
        // Let's obtain the PEB address using PROCESS_QUERY_LIMITED_INFORMATION handle.
        uintptr_t peb_address = 0;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            typedef LONG(WINAPI* NtQueryInformationProcess_t)(HANDLE, int, PVOID, ULONG, PULONG);
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            if (ntdll) {
                auto NtQueryInformationProcess = (NtQueryInformationProcess_t)GetProcAddress(ntdll, "NtQueryInformationProcess");
                if (NtQueryInformationProcess) {
                    struct PROCESS_BASIC_INFORMATION {
                        PVOID Reserved1;
                        PVOID PebBaseAddress;
                        PVOID Reserved2[2];
                        ULONG_PTR UniqueProcessId;
                        PVOID Reserved3;
                    } pbi{};
                    ULONG len = 0;
                    if (NtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), &len) >= 0) {
                        peb_address = reinterpret_cast<uintptr_t>(pbi.PebBaseAddress);
                    }
                }
            }
            CloseHandle(hProcess);
        }

        if (!peb_address) {
            printf("[-] Failed to get PEB address.\n");
            return false;
        }

        printf("[+] PEB Address: 0x%llX\n", (unsigned long long)peb_address);

        // Walk PEB Ldr (with retry loop in case process is initializing)
        uintptr_t ldr = 0;
        int ldr_retries = 10;
        while (ldr_retries-- > 0) {
            if (read_raw(peb_address + 0x18, &ldr, sizeof(ldr)) && ldr) {
                break;
            }
            Sleep(200);
        }

        if (!ldr) {
            printf("[-] Failed to read PEB Ldr from 0x%llX (error %lu).\n", (unsigned long long)(peb_address + 0x18), GetLastError());
            return false;
        }

        printf("[+] PEB Ldr: 0x%llX\n", (unsigned long long)ldr);

        uintptr_t list_head = ldr + 0x10;
        uintptr_t current_link = 0;
        if (!read_raw(list_head, &current_link, sizeof(current_link)) || !current_link) {
            printf("[-] Failed to read module list head.\n");
            return false;
        }

        int count = 0;
        while (current_link != list_head && count < 500) {
            // LDR_DATA_TABLE_ENTRY (x64 InLoadOrderLinks is at offset 0x0)
            // DllBase is at offset 0x30
            // SizeOfImage is at 0x40
            // BaseDllName (UNICODE_STRING) is at offset 0x58
            // BaseDllName.Buffer is at offset 0x60
            // BaseDllName.Length is at offset 0x58 (USHORT)
            uintptr_t dll_base = 0;
            uint32_t size_of_image = 0;
            USHORT name_len = 0;
            uintptr_t name_buf_ptr = 0;

            read_raw(current_link + 0x30, &dll_base, sizeof(dll_base));
            read_raw(current_link + 0x40, &size_of_image, sizeof(size_of_image));
            read_raw(current_link + 0x58, &name_len, sizeof(name_len));
            read_raw(current_link + 0x60, &name_buf_ptr, sizeof(name_buf_ptr));

            if (dll_base && name_len > 0 && name_buf_ptr) {
                std::wstring name(name_len / 2, L'\0');
                if (read_raw(name_buf_ptr, name.data(), name_len)) {
                    if (_wcsicmp(name.c_str(), L"client.dll") == 0) {
                        m_modules.client = dll_base;
                        m_modules.client_size = size_of_image;
                    } else if (_wcsicmp(name.c_str(), L"engine2.dll") == 0) {
                        m_modules.engine2 = dll_base;
                        m_modules.engine2_size = size_of_image;
                    } else if (_wcsicmp(name.c_str(), L"schemasystem.dll") == 0) {
                        m_modules.schemasystem = dll_base;
                        m_modules.schemasystem_size = size_of_image;
                    } else if (_wcsicmp(name.c_str(), L"tier0.dll") == 0) {
                        m_modules.tier0 = dll_base;
                        m_modules.tier0_size = size_of_image;
                    } else if (_wcsicmp(name.c_str(), L"vphysics2.dll") == 0) {
                        m_modules.vphysics2 = dll_base;
                        m_modules.vphysics2_size = size_of_image;
                    }
                }
            }

            // Move to next link (Flink)
            if (!read_raw(current_link, &current_link, sizeof(current_link)) || !current_link) {
                break;
            }
            count++;
        }

        if (!m_modules.client || !m_modules.engine2 || !m_modules.schemasystem || !m_modules.tier0 || !m_modules.vphysics2) {
            printf("[-] Failed to find all required game modules in PEB module list.\n");
            return false;
        }

        return true;
    }

    void close() override {
        if (h_driver != INVALID_HANDLE_VALUE) {
            CloseHandle(h_driver);
            h_driver = INVALID_HANDLE_VALUE;
        }

        if (driver_loaded_by_us) {
            DriverManager::full_cleanup();
            driver_loaded_by_us = false;
        }

        pid = 0;
        m_modules = {};
    }

    bool read_raw(uintptr_t address, void* buffer, size_t size) const override {
        if (h_driver == INVALID_HANDLE_VALUE || !pid || !buffer || size == 0) {
            return false;
        }

        const size_t MAX_CHUNK = 0x10000; // 64KB per IOCTL
        BYTE* dst = static_cast<BYTE*>(buffer);
        size_t remaining = size;
        uintptr_t current_addr = address;
        DWORD current_pid = GetCurrentProcessId();

        while (remaining > 0) {
            size_t chunk = (remaining > MAX_CHUNK) ? MAX_CHUNK : remaining;

            MEMORY_REQUEST request{};
            request.processId = pid;          // Target game process PID
            request.cheatId   = current_pid;  // Our process PID
            request.address   = static_cast<UINT64>(current_addr);
            request.buffer    = reinterpret_cast<UINT64>(dst);
            request.size      = static_cast<ULONG>(chunk);
            request.response  = 0;

            DWORD returned = 0;
            BOOL ok = DeviceIoControl(
                h_driver,
                IOCTL_READ_MEMORY,
                &request, sizeof(request),
                &request, sizeof(request),
                &returned,
                nullptr
            );

            if (!ok || returned != sizeof(request) || request.response != chunk) {
                return false;
            }

            dst += chunk;
            current_addr += chunk;
            remaining -= chunk;
        }

        return true;
    }

    uintptr_t get_client_base() const override { return m_modules.client; }

    DWORD get_pid() const override { return pid; }

private:
    HANDLE    h_driver = INVALID_HANDLE_VALUE;
    DWORD     pid = 0;
    bool      driver_loaded_by_us = false;
    bool      m_use_kdmapper = false;

    uintptr_t query_module_base(const wchar_t* module_name, size_t* out_size) const {
        if (h_driver == INVALID_HANDLE_VALUE || !pid) return 0;

        MODULE_REQUEST request{};
        request.processId = pid;
        request.baseAddress = 0;

        DWORD returned = 0;

        BOOL ok = DeviceIoControl(
            h_driver,
            IOCTL_GET_MODULE,
            &request, sizeof(request),
            &request, sizeof(request),
            &returned,
            nullptr
        );

        if (ok && returned == sizeof(request)) {
            if (out_size) {
                // BlazeDriver walks the PEB to find the ImageBaseAddress, 
                // but it doesn't return the module size, we can default it or parse it later
                *out_size = 0x10000000; 
            }
            return static_cast<uintptr_t>(request.baseAddress);
        }

        return 0;
    }
};
