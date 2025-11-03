#pragma once
#include "cache.h"
#include "struct.h"     // <-- ADICIONADO: Para FName, PointerArray, FSkillCheckDefinition, etc.
#include "offset.h"
#include <mutex>
#include <iostream>
#include <vector>      // Necessário para std::vector
#include <string>      // Necessário para std::string

// --- LÓGICA PRINCIPAL DO AUTO SKILL CHECK ---
inline void HandleSkillChecks(uintptr_t currentLocalPawn) { // Recebe o ponteiro como parâmetro

    // 1. Validação de ponteiros essenciais
    if (!is_valid(currentLocalPawn)) return;

    uintptr_t interactionHandlerPtr = DBD->rpm<uintptr_t>(currentLocalPawn + offsets::InteractionHandler);
    if (!is_valid(interactionHandlerPtr)) return;

    uintptr_t skillCheckPtr = DBD->rpm<uintptr_t>(interactionHandlerPtr + offsets::SkillCheck);
    if (!is_valid(skillCheckPtr)) return;

    // --- Offsets internos do objeto USkillCheck ---
    uintptr_t isDisplayedOffset = 0x178;
    uintptr_t progressOffset = 0x17C;
    uintptr_t typeOffset = 0x1A8;
    uintptr_t definitionOffset = 0x1E8;

    // 2. Verificar se o skill check está na tela
    bool b_isDisplayed = DBD->rpm<bool>(skillCheckPtr + isDisplayedOffset);

    if (b_isDisplayed) {
        // 3. Ler as informações do skill check
        float currentProgress = DBD->rpm<float>(skillCheckPtr + progressOffset);
        ESkillCheckCustomType type = DBD->rpm<ESkillCheckCustomType>(skillCheckPtr + typeOffset);
        FSkillCheckDefinition definition = DBD->rpm<FSkillCheckDefinition>(skillCheckPtr + definitionOffset);

        bool isNegativeProgressRate = definition.ProgressRate < 0.f;

        // 4. Lógica para Wiggle
        if (type == ESkillCheckCustomType::VE_Wiggle) {
            float wiggleProgress = isNegativeProgressRate ? definition.StartingTickerPosition + (1 - currentProgress) : currentProgress + definition.StartingTickerPosition;
            if (wiggleProgress > 1.f) wiggleProgress -= 1.f;

            float bonusZoneStart = definition.BonusZoneStart;
            float bonusZoneEnd = bonusZoneStart + definition.BonusZoneLength;

            if (wiggleProgress > bonusZoneStart && wiggleProgress < bonusZoneEnd) {
                sendSpaceCommand();
            }
        }
        // 5. Lógica para Skill Checks normais
        else {
            float skillCheckStartZone = definition.SuccessZoneStart - definition.StartingTickerPosition;
            float skillCheckEndZone = definition.SuccessZoneEnd - definition.StartingTickerPosition;

            // Tenta acertar a zona de bônus (great skill check) se ela existir
            if (definition.BonusZoneLength > 0) {
                skillCheckStartZone = definition.BonusZoneStart - definition.StartingTickerPosition;
            }

            // Normaliza os limites para a verificação, considerando a direção do ponteiro
            float startRange = isNegativeProgressRate ? 1 - skillCheckEndZone : skillCheckStartZone;
            float endRange = isNegativeProgressRate ? 1 - skillCheckStartZone : skillCheckEndZone;

            if (currentProgress > startRange && currentProgress < endRange) {
                sendSpaceCommand();
            }
        }
    }
}

inline void SkillCheckThread()
{
    while (rendering)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        bool shouldRun = false;
        {
            std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
            shouldRun = g_GameState.bAutoSkillCheck;
        }

        if (!shouldRun) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Validação inicial do DBD e process_base
        if (!DBD || process_base == 0) {
            Sleep(1000); // Espera se driver/base inválido
            continue;
        }

        // Lógica simplificada para obter o LocalPawn
        uintptr_t uWorld = DBD->rpm<uintptr_t>(process_base + offsets::GWorld);
        if (!is_valid(uWorld)) continue;

        uintptr_t gameInstance = DBD->rpm<uintptr_t>(uWorld + offsets::OwningGameInstance);
        if (!is_valid(gameInstance)) continue;

        uintptr_t localPlayer = DBD->rpm<uintptr_t>(DBD->rpm<uintptr_t>(gameInstance + offsets::LocalPlayers));
        if (!is_valid(localPlayer)) continue;

        uintptr_t playerController = DBD->rpm<uintptr_t>(localPlayer + offsets::PlayerController);
        if (!is_valid(playerController)) continue;

        uintptr_t localPawn = DBD->rpm<uintptr_t>(playerController + offsets::AcknowledgedPawn);

        // Se o localPawn for válido (estamos em partida), chama a função
        if (is_valid(localPawn))
        {
            HandleSkillChecks(localPawn);
        }
    }
}

// =======================================================
// THREAD DE INFORMAÇÕES DO KILLER
// =======================================================
inline void UpdateKillerInfoThread() {
    while (rendering) // A thread rodará enquanto o programa estiver ativo
    {
        // A cada 500ms, a thread tentará encontrar as informações do killer
        Sleep(1000);

        if (!DBD || process_base == 0) { // Validação
            Sleep(1000); continue;
        }

        uintptr_t uWorld = DBD->rpm<uintptr_t>(process_base + offsets::GWorld);
        if (!is_valid(uWorld)) continue;

        uintptr_t gamestate = DBD->rpm<uintptr_t>(uWorld + offsets::GameState);
        if (!is_valid(gamestate)) continue;

        bool killerFound = false;
        std::string foundKillerName = "N/A";
        std::vector<std::string> foundPerks;
        std::vector<std::string> foundAddons;

        // --- MÉTODO 1: GameState->PlayerArray (Em partida) ---
        PointerArray playerArray = DBD->rpm<PointerArray>(gamestate + offsets::PlayerArray); // Usa PointerArray (struct.h)

        if (is_valid(playerArray.data) && playerArray.count > 0 && playerArray.count <= 5) // Validação
        {
            for (int k = 0; k < playerArray.count; k++) {
                uintptr_t playerStateAddr = playerArray.data + k * sizeof(uintptr_t);
                if (!is_valid(playerStateAddr)) continue; // Valida endereço do ponteiro
                uintptr_t playerState = DBD->rpm<uintptr_t>(playerStateAddr);
                if (!is_valid(playerState)) continue;

                uint8_t role = DBD->rpm<uint8_t>(playerState + offsets::GameRole);
                if (role == 1) { // 1 = Slasher/Killer
                    killerFound = true;

                    // Leitura segura de perks e addons
                    FCharacterStateData slasher = DBD->rpm<FCharacterStateData>(playerState + offsets::SlasherData); // Usa FCharacterStateData (struct.h)
                    FPlayerStateData playerData = DBD->rpm<FPlayerStateData>(playerState + offsets::PlayerData); // Usa FPlayerStateData (struct.h)

                    // Nome do Killer (Power ID)
                    if (slasher.powerId.ComparisonIndex > 0) { // Valida FName
                        foundKillerName = GetNameById(slasher.powerId.ComparisonIndex);
                        if (foundKillerName == "NULL") foundKillerName = "N/A"; // Trata NULL
                    }
                    else {
                        foundKillerName = "N/A";
                    }


                    // Perks
                    if (is_valid(playerData.EquipedPerkIds.Data) && playerData.EquipedPerkIds.Count > 0 && playerData.EquipedPerkIds.Count <= 4) {
                        std::vector<FName> perks(playerData.EquipedPerkIds.Count); // Usa FName (struct.h)
                        if (DBD->ReadRaw(playerData.EquipedPerkIds.Data, perks.data(), sizeof(FName) * playerData.EquipedPerkIds.Count)) {
                            for (const auto& perk : perks) {
                                if (perk.ComparisonIndex > 0) { // Valida FName
                                    std::string perkName = GetNameById(perk.ComparisonIndex);
                                    if (perkName != "NULL") foundPerks.push_back(perkName);
                                }
                            }
                        }
                    }

                    // Addons
                    if (is_valid(slasher.addonIds.Data) && slasher.addonIds.Count > 0 && slasher.addonIds.Count <= 2) {
                        std::vector<FName> addons(slasher.addonIds.Count); // Usa FName (struct.h)
                        if (DBD->ReadRaw(slasher.addonIds.Data, addons.data(), sizeof(FName) * slasher.addonIds.Count)) {
                            for (const auto& addon : addons) {
                                if (addon.ComparisonIndex > 0) { // Valida FName
                                    std::string addonName = GetNameById(addon.ComparisonIndex);
                                    if (addonName != "NULL") foundAddons.push_back(addonName);
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        // --- MÉTODO 2: Se não encontrou, tenta via varredura de atores (Funciona NO LOBBY) ---
        if (!killerFound) {
            PointerArray levelsArray = DBD->rpm<PointerArray>(uWorld + offsets::Levels);
            for (int i = 0; i < levelsArray.count; ++i) {
                uintptr_t level = DBD->rpm<uintptr_t>(levelsArray.data + i * sizeof(uintptr_t));
                if (!is_valid(level)) continue;

                PointerArray actorArrayData = DBD->rpm<PointerArray>(level + offsets::ActorArray);

                for (int j = 0; j < actorArrayData.count; ++j) {
                    uintptr_t actor = DBD->rpm<uintptr_t>(actorArrayData.data + j * sizeof(uintptr_t));
                    if (!is_valid(actor)) continue;

                    std::string actorName = GetNameById(DBD->rpm<int>(actor + offsets::ActorID));
                    if (actorName == "DBDPlayerState_Lobby") {
                        uintptr_t playerState = actor;
                        uint8_t role = DBD->rpm<uint8_t>(playerState + offsets::GameRole);

                        if (role == 1) { // Se for o Killer
                            killerFound = true;
                            // Reutiliza a mesma lógica de leitura de perks/addons
                            FCharacterStateData slasher = DBD->rpm<FCharacterStateData>(playerState + offsets::SlasherData);
                            uintptr_t playerData = playerState + offsets::PlayerData;
                            int perkCount = DBD->rpm<int>(playerData + 0x10 + 0x8);
                            std::vector<FName> perks(perkCount);
                            if (perkCount > 0)
                                DBD->ReadRaw(DBD->rpm<uintptr_t>(playerData + 0x10), perks.data(), sizeof(FName) * perkCount);

                            int addonCount = slasher.addonIds.Count;
                            std::vector<FName> addons(addonCount);
                            if (addonCount > 0)
                                DBD->ReadRaw(slasher.addonIds.Data, addons.data(), sizeof(FName) * addonCount);

                            std::string killerName = GetNameById(slasher.powerId.ComparisonIndex);
                            std::vector<std::string> perksStr;
                            for (const auto& perk : perks) perksStr.push_back(GetNameById(perk.ComparisonIndex));
                            std::vector<std::string> addonsStr;
                            for (const auto& addon : addons) addonsStr.push_back(GetNameById(addon.ComparisonIndex));

                            {
                                std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
                                if (killerFound) {
                                    g_GameState.killerName = "Power: " + (foundKillerName.empty() ? "N/A" : foundKillerName);
                                    g_GameState.killerPerks = foundPerks;
                                    g_GameState.killerAddons = foundAddons;
                                }
                                else {
                                    // Limpa se não encontrou killer (útil ao sair da partida/lobby)
                                    if (g_GameState.killerName != "Killer: N/A") {
                                        g_GameState.killerName = "Killer: N/A";
                                        g_GameState.killerPerks.clear();
                                        g_GameState.killerAddons.clear();
                                    }
                                }
                                break; // Sai do loop de atores
                            }
                        }
                    }
                    if (killerFound) break; // Sai do loop de levels
                }
            }
        }
    }
}

/*inline void UpdateCameraCacheThread()
{
    while (rendering)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Pode ser mais rápido

        if (!is_valid(playerController))
            continue;

        uintptr_t cameraManager = DBD->rpm<uintptr_t>(playerController + offsets::PlayerCameraManager);
        if (!is_valid(cameraManager))
            continue;

        // 1. Você continua lendo a cache da câmera do jogo para pegar a posição e rotação corretas.
        cameraCache = DBD->rpm<FCameraCacheEntry>(cameraManager + offsets::CameraCachePrivate);

        // ---  A CORREÇÃO ESTÁ AQUI ---
        // 2. Agora, você sobrescreve APENAS o valor do FOV na sua cópia local (cameraCache)
        //    com o valor do slider. Isso sincroniza o ESP com o jogo.
        cameraCache.POV.FOV = customFOV;
        // -----------------------------

        // Verificação de segurança opcional (pode remover se a linha acima funcionar bem)
        if (cameraCache.POV.FOV <= 0.f || cameraCache.POV.FOV > 180.f)
        {
            cameraCache.POV.FOV = 90.f; // Um valor seguro caso algo dê errado
        }

        // O resto do seu código de debug continua igual
        {
            std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
            uintptr_t cameraCacheAddress = cameraManager + offsets::CameraCachePrivate;
            const uintptr_t fovTotalOffset = 0x10 + 0x30;
            float fovPlus4Value = DBD->rpm<float>(cameraCacheAddress + fovTotalOffset + 0x04);
            g_GameState.debugInfo.cameraFOV = cameraCache.POV.FOV;
        }
    }
}

// Em func.h (pode adicionar no final do arquivo)

inline void FovChangerThread()
{
    while (rendering)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (!is_valid(playerController))
            continue;

        uintptr_t cameraManager = DBD->rpm<uintptr_t>(playerController + offsets::PlayerCameraManager);
        if (!is_valid(cameraManager))
            continue;     

        // Calcula o endereço base da CameraCache
        uintptr_t cameraCacheAddress = cameraManager + offsets::CameraCachePrivate;

        // O offset do FOV dentro da CameraCache é 0x40 (0x30 do POV + 0x10 do FOV)
        const uintptr_t fovTotalOffset = 0x40;

        DBD->wpm<float>(cameraManager + 0x2c0, customFOV);
        //DBD->wpm<float>(cameraCacheAddress + fovTotalOffset, customFOV);
    }
}*/