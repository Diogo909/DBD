// In KERNEL.H

#pragma once
#ifndef KERNEL_H
#define KERNEL_H

#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <thread>
//#include "ntloadup.h" // Assumes this file contains the 'driver' namespace functions
#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)

typedef NTSTATUS(
    NTAPI*
    NtDeviceIoControlFile_t)(
        IN HANDLE FileHandle,
        IN HANDLE Event OPTIONAL,
        IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
        IN PVOID ApcContext OPTIONAL,
        OUT PIO_STATUS_BLOCK IoStatusBlock,
        IN ULONG IoControlCode,
        IN PVOID InputBuffer OPTIONAL,
        IN ULONG InputBufferLength,
        OUT PVOID OutputBuffer OPTIONAL,
        IN ULONG OutputBufferLength
        );

inline BOOLEAN DEBUG = false;

inline void Ulog(const char* const _Format, ...) {
    if (!DEBUG)
        return;

    va_list args;
    va_start(args, _Format);
    vprintf(_Format, args);
    va_end(args);
}

struct SystemRequest
{
    PVOID Address;
    PVOID Buffer;
    SIZE_T BufferSize;
    INT Process;
    wchar_t ModuleName[256];

    // --- Novos campos ---
    PVOID OutAddress;       // Para retornar o endereço alocado
    PVOID ThreadStart;      // Para especificar o endereço de início da thread

    enum _CALL
    {
        read,
        write,
        cache,
        getBase,
        alloc,
        createThread,
        // --- NOVA OPERAÇÃO ---
        free
    } CALL;
};

const ULONG DRIVER_CALL = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);

inline class _kernel
{
private:
    //std::thread caching;
    //bool isCaching = false;
    NtDeviceIoControlFile_t NtDeviceIoControlFileImport;
public:
    HANDLE kernelHandle = INVALID_HANDLE_VALUE;
    INT processHandle = 0;

    INT GetProcessId() const
    {
        return processHandle;
    }

    bool CacheProcessDirectoryTableBase()
    {
        if (kernelHandle == INVALID_HANDLE_VALUE)
            return false;

        SystemRequest Request{};
        Request.Process = processHandle;
        Request.CALL = SystemRequest::_CALL::cache;

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle,
            NULL,
            NULL,
            NULL,
            &ioStatus,
            DRIVER_CALL,
            &Request,
            sizeof(Request),
            &Request,
            sizeof(Request)
        );

        return NT_SUCCESS(ioStatus.Status);
    }

    /*
    void CacheThread()
    {
        while (isCaching)
        {
            CacheProcessDirectoryTableBase();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }*/

    bool Attach(const wchar_t* ProcessName)
    {
        HMODULE NTDLL = GetModuleHandleA("ntdll.dll");
        if (!NTDLL) return false;

        NtDeviceIoControlFileImport = (NtDeviceIoControlFile_t)GetProcAddress(NTDLL, "NtDeviceIoControlFile");
        if (!NtDeviceIoControlFileImport)
            return false;

        /*if (!driver::isloaded())
        {
            driver::load(driver::rawData, sizeof(driver::rawData));
        }*/

        HANDLE SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        if (SnapShot == INVALID_HANDLE_VALUE)
            return false;

        PROCESSENTRY32W Entry{};
        Entry.dwSize = sizeof(PROCESSENTRY32W);

        BOOL success = Process32FirstW(SnapShot, &Entry);
        while (success)
        {
            if (_wcsicmp(Entry.szExeFile, ProcessName) == 0)
            {
                processHandle = Entry.th32ProcessID;
                break;
            }
            success = Process32NextW(SnapShot, &Entry);
        }
        CloseHandle(SnapShot);

        if (processHandle == 0)
        {
            Ulog("failed to find process\n");
            return false;
        }

        kernelHandle = CreateFileW(
            L"\\\\.\\netodrv",
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (kernelHandle == INVALID_HANDLE_VALUE)
        {
            Ulog("failed to get driver handle\n");
            return false;
        }

        if (!CacheProcessDirectoryTableBase())
        {
            Ulog("failed to perform initial cache\n");
            CloseHandle(kernelHandle);
            kernelHandle = INVALID_HANDLE_VALUE;
            return false;
        }

        /*
        isCaching = true;
        caching = std::thread(&_kernel::CacheThread, this);
        caching.detach();
        */

        return true;
    }

    void Detach()
    {
        if (kernelHandle != INVALID_HANDLE_VALUE)
        {
            //isCaching = false;
            CloseHandle(kernelHandle);
            kernelHandle = INVALID_HANDLE_VALUE;
        }
        processHandle = 0;
    }

    bool ReadVirtualMemory(uintptr_t Address, void* Buffer, SIZE_T Size)
    {
        if (kernelHandle == INVALID_HANDLE_VALUE)
            return false;

        SystemRequest Request{};
        Request.Address = (PVOID)Address;
        Request.Buffer = Buffer;
        Request.BufferSize = Size;
        Request.Process = processHandle;
        Request.CALL = SystemRequest::_CALL::read;

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle,
            NULL,
            NULL,
            NULL,
            &ioStatus,
            DRIVER_CALL,
            &Request,
            sizeof(Request),
            NULL,         // <-- CORRIGIDO: Não há buffer de saída
            0             // <-- CORRIGIDO: Tamanho do buffer de saída é 0
        );

        return NT_SUCCESS(ioStatus.Status);
    }

    bool WriteVirtualMemory(uintptr_t Address, void* Buffer, SIZE_T Size)
    {
        if (kernelHandle == INVALID_HANDLE_VALUE)
            return false;

        SystemRequest Request{};
        Request.Address = (PVOID)Address;
        Request.Buffer = Buffer;
        Request.BufferSize = Size;
        Request.Process = processHandle;
        Request.CALL = SystemRequest::_CALL::write;

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle,
            NULL,
            NULL,
            NULL,
            &ioStatus,
            DRIVER_CALL,
            &Request,
            sizeof(Request),
            NULL,         // <-- CORRIGIDO: Não há buffer de saída
            0             // <-- CORRIGIDO: Tamanho do buffer de saída é 0
        );

        return NT_SUCCESS(ioStatus.Status);
    }

    uintptr_t GetModuleBase(const wchar_t* ModuleName)
    {
        if (kernelHandle == INVALID_HANDLE_VALUE || !ModuleName)
            return 0;

        SystemRequest Request{};
        Request.Process = processHandle;
        Request.CALL = SystemRequest::_CALL::getBase;
        wcscpy_s(Request.ModuleName, sizeof(Request.ModuleName) / sizeof(wchar_t), ModuleName);

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle,
            NULL,
            NULL,
            NULL,
            &ioStatus,
            DRIVER_CALL,
            &Request,
            sizeof(Request),
            &Request,
            sizeof(Request)
        );

        if (NT_SUCCESS(ioStatus.Status))
        {
            return (uintptr_t)Request.Address;
        }
        return 0;
    }

    PVOID AllocateMemory(SIZE_T Size)
    {
        if (kernelHandle == INVALID_HANDLE_VALUE)
            return nullptr;

        SystemRequest Request{};
        Request.Process = processHandle;
        Request.BufferSize = Size;
        Request.CALL = SystemRequest::_CALL::alloc;

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle, NULL, NULL, NULL, &ioStatus,
            DRIVER_CALL, &Request, sizeof(Request), &Request, sizeof(Request)
        );

        if (NT_SUCCESS(ioStatus.Status)) {
            return Request.OutAddress;
        }
        return nullptr;
    }

    bool CreateRemoteThread(PVOID StartAddress)
    {
        if (kernelHandle == INVALID_HANDLE_VALUE)
            return false;

        SystemRequest Request{};
        Request.Process = processHandle;
        Request.ThreadStart = StartAddress;
        Request.CALL = SystemRequest::_CALL::createThread;

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle, NULL, NULL, NULL, &ioStatus,
            DRIVER_CALL, &Request, sizeof(Request), &Request, sizeof(Request)
        );

        return NT_SUCCESS(ioStatus.Status);
    }

    bool FreeMemory(PVOID Address)
    {
        if (kernelHandle == INVALID_HANDLE_VALUE)
            return false;

        SystemRequest Request{};
        Request.Process = processHandle;
        Request.Address = Address;
        Request.BufferSize = 0; // Para MEM_RELEASE, o tamanho deve ser 0
        Request.CALL = SystemRequest::_CALL::free;

        IO_STATUS_BLOCK ioStatus;
        NTSTATUS status = NtDeviceIoControlFileImport(
            kernelHandle, NULL, NULL, NULL, &ioStatus,
            DRIVER_CALL, &Request, sizeof(Request), &Request, sizeof(Request)
        );

        return NT_SUCCESS(ioStatus.Status);
    }

    template<typename T>
    T read(uintptr_t Address) {
        T Buffer{};
        this->ReadVirtualMemory(Address, &Buffer, sizeof(T));
        return Buffer;
    }

    template<typename T>
    bool write(uintptr_t Address, T Buffer) // Mude de 'void' para 'bool'
    {
        // Retorne o resultado
        return this->WriteVirtualMemory(Address, &Buffer, sizeof(T));
    }

} kernel;

//extern _kernel kernel;

#endif // !KERNEL_H