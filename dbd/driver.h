// In DRIVER.H

#pragma once
#include "kernel.h"
#include <string>
#include <vector>

// This class acts as a wrapper for the _kernel class
class DRV {
private:
    std::wstring processName_w;

public:
    static bool Init() {
        return true;
    }

    bool Attach(const char* processName) {
        // Safely convert process name from char* to std::wstring
        size_t size = strlen(processName) + 1;
        std::vector<wchar_t> wide_buffer(size);
        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, wide_buffer.data(), size, processName, _TRUNCATE);

        this->processName_w = wide_buffer.data();

        return kernel.Attach(this->processName_w.c_str());
    }

    uint64_t GetModuleBase() {
        if (processName_w.empty()) return 0;
        return kernel.GetModuleBase(processName_w.c_str());
    }

    template<typename T>
    static T rpm(uintptr_t address) {
        return kernel.read<T>(address);
    }

    template<typename T>
    bool wpm(uintptr_t address, T value) {
        // kernel.write<T>(address, value); // Linha antiga
        // return true; // Linha antiga

        // CORREÇÃO: Passe o 'bool' de retorno do kernel.write para cima
        return kernel.write<T>(address, value);
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) {
        return kernel.ReadVirtualMemory(address, buffer, size);
    }
};

// NOTE: The extra '}' and '#endif' have been removed from the end of this file.