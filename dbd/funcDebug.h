#pragma once
#include "global.h" 
#include "imgui.h"
#include "cache.h"  
#include <sstream>
#include <iomanip>
#include <vector>
#include <functional>
#include <mutex> 
#include <iostream>
#include <algorithm>

// --- Sistema de Registro de Funções de Debug ---
inline std::vector<std::function<void()>> g_debugDrawCalls;
inline std::mutex g_debugDrawMutex;


    // Inheritance: UCompetence > UGameplayModifierContainer > UBaseModifierContainer > UActorComponent > UObject
namespace UPerk {
constexpr auto _isUsable = 0x440; // bool
}

// Inheritance: UPerk > UCompetence > UGameplayModifierContainer > UBaseModifierContainer > UActorComponent > UObject
namespace UDecisiveStrike {
    constexpr auto _timeAfterUnhook = 0x460; // float
    constexpr auto _hasBeenAttempted = 0x508; // bool
    // --- NOVOS OFFSETS DESCOBERTOS ---
    constexpr auto _timerStateFlag = 0x4D8; // int? (Valor 3 quando ativo?)
    constexpr auto _isTimerActiveBool = 0x4E8; // bool? (Valor 1 quando ativo?)
}

// Struct ATUALIZADA para guardar os dados dos perks de forma segura
struct DebugPerkInfo {
    uintptr_t ptr;
    int id;
    std::string name;
    bool isUsable; // _isUsable (0x440)

    // DS specific
    float ds_timeAfterUnhook; // 0x460
    bool ds_hasBeenAttempted; // 0x508
    int ds_timerStateFlag;   // 0x4D8
    bool ds_isTimerActiveBool; // 0x4E8

    DebugPerkInfo() : ptr(0), id(0), isUsable(false), ds_timeAfterUnhook(0.f),
        ds_hasBeenAttempted(false), ds_timerStateFlag(0), ds_isTimerActiveBool(false) {
    }
};

// --- FUNÇÃO HELPER PARA DUMP DE MEMÓRIA ---
inline void PrintMemoryDump(const std::vector<BYTE>& data, uintptr_t baseAddr)
{
    std::cout << "--- RAW MEMORY DUMP (Base: 0x" << std::hex << baseAddr << std::dec << ") ---" << std::endl;
    std::cout << std::hex << std::setfill('0') << std::uppercase; // Formata para hex

    for (size_t i = 0; i < data.size(); ++i) {
        if (i % 16 == 0) { // Começa nova linha
            if (i > 0) std::cout << std::endl;
            // Mostra o offset relativo ao início do dump também
            std::cout << "  0x" << std::setw(4) << i << " (Abs: 0x" << (baseAddr + i) << "): ";
        }
        else if (i % 8 == 0) { // Espaço no meio
            std::cout << "  ";
        }
        // Imprime o byte
        std::cout << std::setw(2) << static_cast<int>(data[i]) << " ";
    }
    std::cout << std::dec << std::setfill(' ') << std::nouppercase; // Reseta a formatação
    std::cout << "\n--- END OF DUMP ---" << std::endl;
}

inline void RegisterDebugDraw(std::function<void()> drawFunction)
{
    std::lock_guard<std::mutex> lock(g_debugDrawMutex);
    g_debugDrawCalls.push_back(drawFunction);
}

inline void DrawCameraDebugInfo()
{
    if (ImGui::CollapsingHeader("Debug da Camera (Mira)"))
    {
        // Trava o mutex para ler os dados de forma segura
        std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);

        // Pega os valores que a LogicThread salvou
        // --- CORREÇÃO DE TIPO AQUI ---
        Rotator rotation = g_GameState.debugInfo.cameraRotation;

        // --- CORREÇÃO DE MEMBROS AQUI ---
        ImGui::Text("Pitch (Cima/Baixo):");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.2f", rotation.Pitch);

        ImGui::Text("Yaw (Lados):");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.2f", rotation.Yaw);

        ImGui::Text("Roll (Inclinacao):");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%.2f", rotation.Roll);
    }
}


inline void DrawAllActorAddresses()
{
    static char actorSearchBuffer[128] = "";

    if (ImGui::CollapsingHeader("Debug de Atores do Cache (Clique para Fixar ESP)"))
    {
        ImGui::InputTextWithHint("##search", "Pesquisar ator por nome...", actorSearchBuffer, sizeof(actorSearchBuffer));
        ImGui::Separator();

        std::string filterLower = actorSearchBuffer;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        const char* actorTypeNames[] = {
            "Survivor", "Killer", "Generator", "Pallet", "Window", "Hook",
            "Hatch", "Escape", "Totem", "Chest", "Trap", "KillerItem", "All"
        };

        bool bIsEmpty = true;
        std::lock_guard<std::mutex> cacheLock(Cache::cacheMutex);

        for (size_t i = 0; i < static_cast<size_t>(Cache::EActorType::COUNT); ++i)
        {
            auto type = static_cast<Cache::EActorType>(i);
            const auto& actorList = Cache::GetList(type);

            if (i >= (sizeof(actorTypeNames) / sizeof(actorTypeNames[0]))) continue;
            if (!actorList.empty()) bIsEmpty = false;

            std::string headerName = std::string(actorTypeNames[i]) + " (" + std::to_string(actorList.size()) + ")";

            if (ImGui::TreeNode(headerName.c_str()))
            {
                if (actorList.empty()) {
                    ImGui::TextDisabled("Nenhum ator encontrado.");
                }
                else {
                    for (const auto& obj : actorList) {
                        std::string nameLower = obj.rawName;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        if (filterLower.empty() || nameLower.find(filterLower) != std::string::npos) {
                            // --- Lógica de Pin ---
                            bool isPinned = false;
                            {
                                std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
                                isPinned = g_GameState.pinnedActors.count(obj.instance) > 0;
                            }
                            char label[512];
                            sprintf_s(label, "0x%p | %s", (void*)obj.instance, obj.rawName.c_str());
                            if (ImGui::Selectable(label, isPinned)) {
                                std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
                                if (isPinned) g_GameState.pinnedActors.erase(obj.instance);
                                else g_GameState.pinnedActors.insert(obj.instance);
                            }

                            // --- Debug de Survivor ---
                            if (type == Cache::EActorType::Survivor) {
                                ImGui::Indent(15.0f);

                                // --- Equipped Perks ---
                                ImGui::TextDisabled("Equipped Perks:");
                                uintptr_t playerState = 0;
                                FPlayerStateData playerData{};
                                std::vector<FName> perks;
                                bool readOk = false;
                                bool playerStateValid = false;
                                int perkCount = 0;
                                { // Lock para PlayerState
                                    std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
                                    playerState = DBD->rpm<uintptr_t>(obj.instance + offsets::PlayerState);
                                    if (is_valid(playerState)) {
                                        playerStateValid = true;
                                        playerData = DBD->rpm<FPlayerStateData>(playerState + offsets::PlayerData);
                                        perkCount = playerData.EquipedPerkIds.Count;
                                        if (is_valid(playerData.EquipedPerkIds.Data) && perkCount > 0 && perkCount <= 4) {
                                            perks.resize(perkCount);
                                            readOk = DBD->ReadRaw(playerData.EquipedPerkIds.Data, perks.data(), sizeof(FName) * perkCount);
                                        }
                                    }
                                } // Fim Lock PlayerState
                                if (playerStateValid) {
                                    if (readOk) {
                                        for (const auto& perk : perks) {
                                            std::string perkName;
                                            { // Lock para GetNameById
                                                std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
                                                perkName = GetNameById(perk.ComparisonIndex);
                                            }
                                            ImGui::Text("- %s", perkName.c_str());
                                        }
                                    }
                                    else if (perkCount == 0) ImGui::TextDisabled("- None.");
                                    else ImGui::TextDisabled("- Read Error.");
                                }
                                else ImGui::TextDisabled("Invalid PlayerState.");

                                // --- Active Perks ---
                                ImGui::TextDisabled("Active Perks:");
                                uintptr_t perkManager = 0;
                                uintptr_t perkCollection = 0;
                                PointerArray perkArray{};
                                std::vector<DebugPerkInfo> activePerkDetails;
                                bool perkManagerValid = false;
                                bool perkCollectionValid = false;
                                bool perkArrayValid = false;
                                bool readFailed = false;

                                { // Lock para PerkManager e Perks
                                    std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
                                    perkManager = DBD->rpm<uintptr_t>(obj.instance + offsets::PerkManager);
                                    if (is_valid(perkManager)) {
                                        perkManagerValid = true;
                                        perkCollection = DBD->rpm<uintptr_t>(perkManager + offsets::_perks);
                                        if (is_valid(perkCollection)) {
                                            perkCollectionValid = true;
                                            perkArray = DBD->rpm<PointerArray>(perkCollection + offsets::_array);
                                            if (is_valid(perkArray.data) && perkArray.count > 0 && perkArray.count <= 10) {
                                                perkArrayValid = true;
                                                std::vector<uintptr_t> perkPtrs(perkArray.count);
                                                if (DBD->ReadRaw(perkArray.data, perkPtrs.data(), sizeof(uintptr_t) * perkArray.count)) {
                                                    activePerkDetails.reserve(perkArray.count);
                                                    for (uintptr_t perkPtr : perkPtrs) {
                                                        DebugPerkInfo info; info.ptr = perkPtr;
                                                        if (is_valid(perkPtr)) {
                                                            info.id = DBD->rpm<int>(perkPtr + offsets::ActorID);
                                                            info.name = GetNameById(info.id);
                                                            info.isUsable = DBD->rpm<bool>(perkPtr + UPerk::_isUsable);
                                                            if (info.name.find("DecisiveStrike") != std::string::npos || info.name.find("K22P01") != std::string::npos) {
                                                                info.ds_timeAfterUnhook = DBD->rpm<float>(perkPtr + UDecisiveStrike::_timeAfterUnhook);
                                                                info.ds_hasBeenAttempted = DBD->rpm<bool>(perkPtr + UDecisiveStrike::_hasBeenAttempted);
                                                                info.ds_timerStateFlag = DBD->rpm<int>(perkPtr + UDecisiveStrike::_timerStateFlag);
                                                                info.ds_isTimerActiveBool = DBD->rpm<bool>(perkPtr + UDecisiveStrike::_isTimerActiveBool);
                                                            }
                                                        }
                                                        else info.name = "Invalid Ptr";
                                                        activePerkDetails.push_back(info);
                                                    }
                                                }
                                                else readFailed = true;
                                            }
                                        }
                                    }
                                } // Fim Lock PerkManager

                                if (!perkManagerValid) ImGui::TextColored(ImVec4(1.f, .5f, .5f, 1.f), "Invalid PerkMgr Ptr.");
                                else {
                                    if (!perkCollectionValid) ImGui::TextColored(ImVec4(1.f, .5f, .5f, 1.f), "Invalid PerkColl Ptr.");
                                    else {
                                        if (readFailed) ImGui::TextColored(ImVec4(1.f, .5f, .5f, 1.f), "Failed Perk Ptr Read.");
                                        else if (!perkArrayValid) {
                                            if (perkArray.count == 0) ImGui::TextDisabled("- None active.");
                                            else ImGui::TextDisabled("- Array invalid.");
                                        }
                                        else {
                                            bool foundDS = false, foundUNB = false;
                                            for (size_t k = 0; k < activePerkDetails.size(); ++k) {
                                                const auto& info = activePerkDetails[k];
                                                ImGui::Text(" [%zu] Name: '%s'", k, info.name.c_str());
                                                ImGui::Indent(15.0f);
                                                ImGui::Text("IsUsable (0x440): %s", info.isUsable ? "TRUE" : "FALSE");
                                                ImGui::TextDisabled("ID: %d | Ptr: 0x%p", info.id, (void*)info.ptr);

                                                if (info.name.find("SelfSufficient") != std::string::npos || info.name.find("S4P1") != std::string::npos) {
                                                    foundUNB = true; ImGui::TextColored(ImVec4(0.f, 1.f, 0.f, 1.f), "(Unbreakable)");
                                                }
                                                else if (info.name.find("DecisiveStrike") != std::string::npos || info.name.find("K22P01") != std::string::npos) {
                                                    foundDS = true;
                                                    ImGui::TextColored(ImVec4(1.f, .6f, 0.f, 1.f), "Attempted (0x508): %s", info.ds_hasBeenAttempted ? "SIM (GASTO)" : "NAO");
                                                    ImGui::Text("Duration (0x460): %.1fs", info.ds_timeAfterUnhook);
                                                    // --- MOSTRA AS FLAGS NO IMGUI ---
                                                    ImGui::Text("StateFlag (0x4D8): %d", info.ds_timerStateFlag);
                                                    ImGui::Text("ActiveBool (0x4E8): %s", info.ds_isTimerActiveBool ? "TRUE" : "FALSE");

                                                    // Determina o estado baseado nas flags
                                                    if (info.ds_hasBeenAttempted) {
                                                        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "ESTADO: GASTO");
                                                    }
                                                    else if (info.ds_isTimerActiveBool) { // Usa a flag booleana que descobrimos
                                                        ImGui::TextColored(ImVec4(0.f, 1.f, 1.f, 1.f), "ESTADO: ARMADO (Timer Rodando)");
                                                    }
                                                    else {
                                                        ImGui::TextDisabled("ESTADO: Inativo");
                                                    }
                                                }
                                                ImGui::Unindent(15.0f);
                                                ImGui::Spacing();
                                            }
                                            if (!foundDS) ImGui::TextDisabled("- DS not active.");
                                            if (!foundUNB) ImGui::TextDisabled("- UNB not active.");
                                        }
                                    }
                                }
                                ImGui::Unindent(15.0f);
                            } // Fim debug Survivor
                        }
                    }
                }
                ImGui::TreePop();
            }
        } // Fim loop Tipos de Ator

        if (bIsEmpty) {
            ImGui::Text("Cache de atores esta vazio.");
        }
    } // Fim CollapsingHeader
}

inline void DrawPatchStatusDebug()
{
    if (ImGui::CollapsingHeader("Debug do Patch de Memoria"))
    {
        if (process_base == 0) {
            ImGui::Text("Aguardando base do processo do jogo...");
            return;
        }
        uintptr_t patchOffset = offsets::GamePatch;

        // --- ADICIONE ESTA VERIFICAÇÃO ---
        if (patchOffset == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[AVISO] Offset do GamePatch não foi encontrado!");
            ImGui::Text("A assinatura (sig) pode ter mudado.");
            return;
        }

        const uintptr_t patchAddress = process_base + patchOffset;
        const BYTE expectedBytes[3] = { 0xEB, 0x08, 0x90 };
        BYTE readBytes[3] = { 0 };
        for (int i = 0; i < 3; ++i) readBytes[i] = DBD->rpm<BYTE>(patchAddress + i);

        ImGui::Text("Endereco Alvo: 0x%p", (void*)patchAddress);
        ImGui::Separator();

        std::stringstream ss_read;
        ss_read << std::hex << std::uppercase << std::setfill('0');
        for (int i = 0; i < 3; ++i) ss_read << "0x" << std::setw(2) << static_cast<int>(readBytes[i]) << (i < 2 ? " " : "");
        ImGui::Text("Bytes Atuais:   %s", ss_read.str().c_str());

        std::stringstream ss_expected;
        ss_expected << std::hex << std::uppercase << std::setfill('0');
        for (int i = 0; i < 3; ++i) ss_expected << "0x" << std::setw(2) << static_cast<int>(expectedBytes[i]) << (i < 2 ? " " : "");
        ImGui::Text("Bytes Esperados: %s", ss_expected.str().c_str());

        ImGui::Spacing();
        // Lógica de verificação e adição do botão
        if (memcmp(readBytes, expectedBytes, sizeof(expectedBytes)) == 0) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[SUCESSO] O patch esta aplicado corretamente!");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[FALHA] Os bytes na memoria nao correspondem ao patch.");

            // --- NOVO CÓDIGO ---
            // Adiciona um botão que só aparece em caso de falha
            if (ImGui::Button("Tentar Aplicar Patch Novamente"))
            {
                // Este código será executado quando o botão for clicado.
                // É o mesmo código que você forneceu, adaptado para esta função.
                constexpr int instructionSize = 3;
                BYTE patchBytes[instructionSize] = { 0xEB, 0x08, 0x90 };

                // Escreve os bytes do patch no endereço de memória
                for (int i = 0; i < instructionSize; ++i) {
                    DBD->wpm<BYTE>(patchAddress + i, patchBytes[i]);
                }

                // Uma pequena pausa para garantir que a escrita seja processada
                Sleep(50);
            }
            // --- FIM DO NOVO CÓDIGO ---
        }
    }
}

inline void DrawSSLBypassStatus()
{
    if (ImGui::CollapsingHeader("Debug do SSL Bypass"))
    {
        // 1. Valida se temos a base do jogo
        if (process_base == 0) {
            ImGui::Text("Aguardando base do processo do jogo...");
            return;
        }

        // 2. Valida se o offset foi encontrado pelo scanner no main.cpp
        if (offsets::SSLBypass == 0) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[AVISO] Offset do SSL Bypass nao foi encontrado!");
            ImGui::Text("A assinatura (sig) pode ter mudado.");
            return;
        }

        // 3. Se tudo estiver OK, calcula o endereço final
        const uintptr_t patchAddress = process_base + offsets::SSLBypass;

        // 4. Lê o valor booleano atual do endereço
        bool bCurrentValue = DBD->rpm<bool>(patchAddress);

        // 5. Exibe as informações
        ImGui::Text("Endereco Alvo (SSL): 0x%p", (void*)patchAddress);
        ImGui::Text("Valor Desejado:      FALSE (0)");
        ImGui::Separator();
        ImGui::Text("Valor Atual:");
        ImGui::SameLine();

        // 6. Mostra o status com cor (Verde = Bom, Vermelho = Ruim)
        if (bCurrentValue == false) // O valor é 0 (FALSO), que é o que queremos
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "FALSE (0)");
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[SUCESSO] O bypass esta aplicado!");
        }
        else // O valor é 1 (VERDADEIRO), precisamos corrigir
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "TRUE (1)");
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[PERIGO] O bypass esta DESATIVADO!");
        }

        ImGui::Spacing();

        // 7. Adiciona o botão para forçar o valor para FALSE (0)
        if (ImGui::Button("Forcar SSL Bypass para FALSE (0)"))
        {
            DBD->wpm<bool>(patchAddress, false); // Escreve 0 (false) no endereço
            Sleep(50); // Pausa para garantir a escrita
        }
    }
}

// ========================================================================
// ========= REGISTRADOR CENTRAL INTELIGENTE (HEADER-ONLY) ================
// ========================================================================
// Esta classe garante que o registro só aconteça UMA VEZ.
class DebugModuleRegistrar
{
public:
    // Método público para "acordar" o registrador
    static void EnsureInitialized() {
        // Apenas chamar a função GetInstance garante que o construtor
        // será executado na primeira vez.
        GetInstance();
    }

private:
    // Construtor privado: aqui é onde a mágica acontece.
    // Este código só vai rodar UMA VEZ em todo o programa.
    DebugModuleRegistrar()
    {
        // --- ADICIONE NOVOS PAINÉIS DE DEBUG AQUI ---
        // Basta adicionar a linha RegisterDebugDraw com sua nova função.

        RegisterDebugDraw(DrawPatchStatusDebug);

        RegisterDebugDraw(DrawAllActorAddresses);

        RegisterDebugDraw(DrawSSLBypassStatus);

        RegisterDebugDraw(DrawCameraDebugInfo);
    }

    // Método que cria e retorna a instância única
    static DebugModuleRegistrar& GetInstance()
    {
        // O C++ garante que 'instance' só é criada na primeira chamada.
        static DebugModuleRegistrar instance;
        return instance;
    }

    // Impede que a classe seja copiada
    DebugModuleRegistrar(const DebugModuleRegistrar&) = delete;
    void operator=(const DebugModuleRegistrar&) = delete;
};