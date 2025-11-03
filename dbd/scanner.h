#pragma once
#include "global.h" // Para DBD e process_base
#include "util.h"   // Para is_valid
#include <vector>
#include <string>
#include <sstream>

namespace SigScanner {

    // Função helper para os novos popups de debug
    inline void DebugMessageBox(bool& mensagebox, const char* text) {
        if (mensagebox) {
            MessageBoxA(NULL, text, "Debug Interno do Scanner", MB_OK | MB_ICONINFORMATION);
        }
    }

    /**
     * @brief Converte uma string de padrão (ex: "88 05 ?? 48") em um vetor de bytes.
     * Wildcards ("??") são representados por -1.
     */
    inline std::vector<int> PatternToBytes(const std::string& pattern) {
        std::vector<int> bytes;
        std::stringstream ss(pattern);
        std::string byteStr;

        while (ss >> byteStr) {
            if (byteStr == "??") {
                bytes.push_back(-1); // -1 representa um wildcard
            }
            else {
                bytes.push_back(std::stoi(byteStr, nullptr, 16));
            }
        }
        return bytes;
    }

    /**
     * @brief Procura um padrão de bytes (assinatura) na memória do processo.
     * Lê o módulo principal inteiro para um buffer local para velocidade.
     * @param pattern A string da assinatura (ex: "88 05 ?? ?? ?? ?? 48 8D").
     * @return O endereço (uintptr_t) onde o padrão foi encontrado, ou 0 se não for encontrado.
     */
    inline uintptr_t FindPattern(const std::string& pattern, uintptr_t readOffset, size_t scanSize, bool& mensagebox) {

        if (!DBD || process_base == 0) {
            DebugMessageBox(mensagebox, "FindPattern: FALHA! DBD ou process_base é 0.");
            return 0;
        }

        std::vector<int> patternBytes = PatternToBytes(pattern);
        const size_t sigSize = patternBytes.size();

        // Aloca um buffer para UMA ÚNICA PÁGINA
        const size_t pageSize = 4096; // 4KB
        std::vector<BYTE> buffer(pageSize);

        // Itera pela memória, pulando de página em página
        for (uintptr_t currentOffset = readOffset; currentOffset < (readOffset + scanSize); currentOffset += pageSize)
        {
            uintptr_t readAddress = process_base + currentOffset;

            // Tenta ler UMA página (4KB)
            if (!DBD->ReadRaw(readAddress, buffer.data(), pageSize)) {
                // Se ReadRaw falhar (página inválida), nós NÃO damos 'return 0'.
                // Nós apenas pulamos para a próxima página e continuamos a varredura.
                continue;
            }

            // Agora, escaneia dentro desta página que acabamos de ler
            for (size_t i = 0; i < (pageSize - sigSize); ++i)
            {
                bool found = true;
                for (size_t j = 0; j < sigSize; ++j) {
                    if (patternBytes[j] != -1 && buffer[i + j] != patternBytes[j]) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    // ENCONTRADO!
                    // Retorna o endereço base + o offset da página + o índice dentro da página
                    return readAddress + i;
                }
            }
        } // Fim do loop de páginas

        DebugMessageBox(mensagebox, "FindPattern: Varredura PAGINADA concluída. Padrão NÃO encontrado.");
        return 0;
    }

    // ========================================================================
    // A FUNÇÃO QUE VOCÊ QUERIA
    // ========================================================================

    /**
     * @brief Encontra o offset do SSLBypass dinamicamente usando a assinatura.
     */
    inline uintptr_t FindSSLBypassOffset(bool& mensagebox) {

        DebugMessageBox(mensagebox, "FindSSLBypassOffset: Iniciado.");

        std::string sig = "88 05 ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? C6 44 24";

        // Parâmetros: (pattern, offset_inicio, tamanho_scan, debug)
        // Vamos escanear os primeiros 32MB (0x2000000)
        const uintptr_t readOffset = 0x0;         // Começa do início
        const size_t scanSize = 0x2000000;
        uintptr_t patternAddress = FindPattern(sig, readOffset, scanSize, mensagebox);

        if (!is_valid(patternAddress)) {
            DebugMessageBox(mensagebox, "FindSSLBypassOffset: FindPattern falhou ou não encontrou. Retornando 0.");
            return 0; // Não encontrado
        }

        // ... (o resto da função é igual)
        DebugMessageBox(mensagebox, "FindSSLBypassOffset: Padrão encontrado. Lendo offset relativo...");
        int32_t relativeOffset = DBD->rpm<int32_t>(patternAddress + 2);
        uintptr_t ripAddress = patternAddress + 6;
        uintptr_t targetAddress = ripAddress + relativeOffset;
        uintptr_t finalOffset = targetAddress - process_base;
        DebugMessageBox(mensagebox, "FindSSLBypassOffset: Cálculo concluído. Retornando offset.");
        return finalOffset;
    }

    inline uintptr_t FindGamePatchOffset(bool& mensagebox) {

        DebugMessageBox(mensagebox, "FindGamePatchOffset: Iniciado.");

        std::string sigPatched = "0F 84 ?? ?? ?? ?? EB 08 90 0F 11 81 ?? ?? ?? ??";
        std::string sigOriginal = "0F 84 ?? ?? ?? ?? 0F 10 02 0F 11 81 ?? ?? ?? ??";

        uintptr_t patternAddress = 0;

        // Vamos escanear os primeiros 100MB (0x6400000) do processo.
        // E lemos 4MB (0x400000).
        const uintptr_t readOffset = 0x0; // Começa do início
        const size_t scanSize = 0x6400000; // Escaneia por 100 MB

        DebugMessageBox(mensagebox, "FindGamePatchOffset: Procurando pela assinatura JÁ CORRIGIDA...");
        patternAddress = FindPattern(sigPatched, readOffset, scanSize, mensagebox);

        if (!is_valid(patternAddress)) {
            DebugMessageBox(mensagebox, "FindGamePatchOffset: Assinatura corrigida não encontrada. Procurando pela ORIGINAL...");
            patternAddress = FindPattern(sigOriginal, readOffset, scanSize, mensagebox);
        }

        if (!is_valid(patternAddress)) {
            DebugMessageBox(mensagebox, "FindGamePatchOffset: FindPattern falhou para ambas as assinaturas. Retornando 0.");
            return 0;
        }

        DebugMessageBox(mensagebox, "FindGamePatchOffset: Padrão encontrado.");

        // O 'patternAddress' retornado já é o endereço absoluto
        uintptr_t targetAddress = patternAddress + 6;

        // Retorne o offset final em relação à base do processo
        return targetAddress - process_base;
    }
} // Fim do namespace SigScanner