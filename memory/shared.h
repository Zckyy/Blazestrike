// memory/shared.h
#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <Windows.h>
#include <winioctl.h>
#endif

// BlazeDriver definitions (Loaded via KDMapper)
#define DRIVER_DEVICE_NAME      L"\\Device\\BlazeDriver"
#define DRIVER_SYMBOLIC_LINK    L"\\??\\BlazeDriver"
#define DRIVER_USER_PATH        L"\\\\.\\BlazeDriver"
#define DRIVER_SERVICE_NAME     L"BlazeDriver"
#define DRIVER_FILE_NAME        L"BlazeDriver.sys"

// kdmapper aliases
#define KDMP_DRIVER_NAME        L"\\Driver\\BlazeDriver"
#define KDMP_DEVICE_NAME        L"\\Device\\BlazeDriver"
#define KDMP_SYMBOLIC_LINK      L"\\??\\BlazeDriver"
#define KDMP_USER_PATH          L"\\\\.\\BlazeDriver"
#define KDMP_FILE_NAME          L"BlazeDriver.sys"

#define IOCTL_BASE              0xC39

#define IOCTL_READ_MEMORY   CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 1, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_WRITE_MEMORY  CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 2, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_GET_MODULE    CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 3, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_UNLOAD        CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 4, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

// Map standard structure names from PooDriver code to BlazeDriver format
typedef struct _MEMORY_REQUEST {
    unsigned long    processId;   // Game PID
    unsigned long    cheatId;     // Cheat PID
    unsigned __int64 address;     // Source/destination address in game process
    unsigned __int64 buffer;      // Destination/source address in cheat process
    unsigned long    size;        // Bytes to copy
    unsigned __int64 response;    // OUT: bytes copied
} MEMORY_REQUEST, *PMEMORY_REQUEST;

typedef struct _MODULE_REQUEST {
    unsigned long    processId;
    unsigned __int64 baseAddress; // OUT: ImageBaseAddress
} MODULE_REQUEST, *PMODULE_REQUEST;