#include "GameState.h"
#include "util.h"
#include "offset.h"
#include "func.h"
#include <cmath>
#include "characteroffset.h"

#include <sstream>
#include <chrono>
#include <iostream>

#include "cache.h"
#include <vector>
#include <string>
#include <mutex>
#include <set>    
#include <fstream>  
#include <cmath>
#include "imgui.h"
#include "funcDebug.h"


// ###################################################### Some useful stuff (I was lazy) ###################################################### \\

HWND hwnd = NULL;
HWND hwnd_active = NULL;
HWND OverlayWindow = NULL;
HANDLE hProcess = NULL;

ImU32 CrosshairColor = IM_COL32(0, 100, 255, 255);
ImU32 Color = IM_COL32(255, 0, 0, 255);

#define M_PI 3.14159265358979323846264338327950288419716939937510


// --- VARIÁVEIS PARA O DEBUG DA LISTA DE ATORES ---
bool g_showActorList = false;
std::vector<std::string> g_actorNameList;
std::mutex g_actorNameMutex;

std::mutex g_allActorsMutex;
std::set<std::string> g_allActorNamesThisFrame;

// ###################################################### ESP Features ###################################################### \\

inline PVOID LogicThread()
{   
    static auto lastOutlineUpdateTime = std::chrono::steady_clock::now();

    while (rendering)
    {   

        // =========================================================================
        // AQUI VOCÊ CONTROLA SEU FPS!
        // 32 -> ~30 FPS
        // 16ms -> ~60 FPS
        // 8ms  -> ~120 FPS
        // 4ms  -> ~250 FPS (Pode usar mais CPU)
        std::this_thread::sleep_for(std::chrono::milliseconds(8));

        // ==============================================================================
        // ======================= LÓGICA DO TIMER DE OTIMIZAÇÃO ========================
        // ==============================================================================
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastOutlineUpdateTime).count();

        bool shouldUpdateOutlines = false;
        // Atualiza as outlines 10x por segundo (a cada 100ms). É rápido para o olho, leve para a CPU.
        if (elapsedMs > 100) {
            shouldUpdateOutlines = true;
            lastOutlineUpdateTime = currentTime;
        }
        // ==============================================================================
        // 
        // 1. OBTÉM PONTEIROS ESSENCIAIS PARA O JOGADOR LOCAL
        uintptr_t uWorld = DBD->rpm<uintptr_t>(process_base + offsets::GWorld);
        if (!is_valid(uWorld)) {
            g_GameState.ClearMatchData();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        float currentServerTime = 0.0f;
        uintptr_t gameState = DBD->rpm<uintptr_t>(uWorld + offsets::GameState);
        if (is_valid(gameState)) {
            currentServerTime = (float)DBD->rpm<int32_t>(gameState + offsets::ElapsedTime);
        }

        uintptr_t gameInstance = DBD->rpm<uintptr_t>(uWorld + offsets::OwningGameInstance);
        uintptr_t localPlayer = DBD->rpm<uintptr_t>(DBD->rpm<uintptr_t>(gameInstance + offsets::LocalPlayers));
        uintptr_t playerController = DBD->rpm<uintptr_t>(localPlayer + offsets::PlayerController);
        uintptr_t localPawn = DBD->rpm<uintptr_t>(playerController + offsets::AcknowledgedPawn);

        g_GameState.bIsInMatch = is_valid(localPawn);

        // ATUALIZADO: Checa os switches master globais
        if ((!g_GameState.settings.bEnableESP_Global && !g_GameState.settings.bEnableOutlines_Global) || !g_GameState.bIsInMatch) {
            g_GameState.ClearVisibleEntities(); // Limpa se o ESP/Outline estiverem desligados ou se não estiver na partida
            continue;
        }

        // 2. LÊ A CÂMERA (Substituindo a UpdateCameraCacheThread)
        FCameraCacheEntry tempCameraCache;
        uintptr_t cameraManager = DBD->rpm<uintptr_t>(playerController + offsets::PlayerCameraManager);
        if (is_valid(cameraManager)) {
            tempCameraCache = DBD->rpm<FCameraCacheEntry>(cameraManager + offsets::CameraCachePrivate);
            tempCameraCache.POV.FOV = g_GameState.customFOV;
            DBD->wpm<float>(cameraManager + 0x2D0, g_GameState.customFOV);
        }

        uintptr_t PlayerStateLocalPlayer = DBD->rpm<uintptr_t>(playerController + offsets::PlayerStateLocalPlayer);
        std::vector<EntityList> tempVisibleEntities;

        std::lock_guard<std::mutex> lock(Cache::cacheMutex);

        // --- CÓDIGO NOVO ---
        // Pega uma cópia da lista de atores fixados para este frame
        std::set<uintptr_t> pinnedActorsCopy;
        {
            // Usa o dataMutex para ler g_GameState de forma segura
            std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
            pinnedActorsCopy = g_GameState.pinnedActors;
        }
        // --- FIM DO CÓDIGO NOVO ---

        // ==============================================================================
        //                                PROCESSA SURVIVORS
        // ==============================================================================
        for (const auto& surv : Cache::GetList(Cache::EActorType::Survivor))
        {
            // --- DADOS BÁSICOS (Lidos uma vez para ambas as funcionalidades) ---
            uintptr_t actor = surv.instance;
            uintptr_t outlineComponent = surv.outlineComponent;
            uintptr_t playerState = DBD->rpm<uintptr_t>(actor + offsets::PlayerState);

            // Pula o jogador local ou inválidos
            if (playerState == PlayerStateLocalPlayer || !is_valid(playerState)) continue;

            // ----------------------------------------------------------------------
            // Bloco 1: Lógica dos OUTLINES (totalmente independente)
            // ----------------------------------------------------------------------
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.survivor.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    // Lê o estado de saúde APENAS se necessário para o outline
                    int healthState = -1;
                    uintptr_t healthComponent = DBD->rpm<uintptr_t>(actor + offsets::HealthComponent);
                    if (is_valid(healthComponent)) {
                        uint8_t currentHealthState = DBD->rpm<uint8_t>(healthComponent + offsets::HealthState);
                        if (currentHealthState >= 0 && currentHealthState <= 2) healthState = currentHealthState;
                    }

                    if (healthState != -1) {
                        float* colorToApply = (healthState == 2) ? g_GameState.colors.outlineSurvivorHealthyColor : g_GameState.colors.outlineSurvivorInjuredColor;
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, colorToApply[0]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, colorToApply[1]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, colorToApply[2]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, colorToApply[3]);
                    }
                }
            }

            bool isPinned = pinnedActorsCopy.count(surv.instance) > 0;
            // ----------------------------------------------------------------------
            // Bloco 2: Lógica do ESP (totalmente independente)
            // ----------------------------------------------------------------------
            if (isPinned || (g_GameState.settings.bEnableESP_Global && g_GameState.settings.survivor.bEnableESP))
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                // 1. Pega o nome do personagem PRIMEIRO
                int32_t survivorIndex = DBD->rpm<int32_t>(playerState + offsets::SelectedSurvivorIndex);
                std::string characterName;
                auto it = DBDSurvivors::Data.find(survivorIndex);
                if (it != DBDSurvivors::Data.end()) {
                    characterName = it->second;
                }
                else {
                    std::stringstream ss;
                    ss << "Unknown Survivor [" << survivorIndex << "]";
                    characterName = ss.str();
                }

                // 2. Monta a entidade
                EntityList Entity{};
                Entity.instance = actor;
                Entity.origin = origin;
                Entity.dist = DistM;
                Entity.name = characterName;
                Entity.bIsPinned = isPinned;
                Entity.type = Cache::EActorType::Survivor;
                Entity.bIsUnbreakableActive = false; // Inicializa
                Entity.dsTimer = 0.0f;             // Inicializa

                // 3. TENTA ler o estado de saúde, mas sem pular se falhar
                int healthState = -1; // Padrão é -1 (Desconhecido)
                uintptr_t healthComponent = DBD->rpm<uintptr_t>(actor + offsets::HealthComponent);
                if (is_valid(healthComponent)) {
                    uint8_t currentHealthState = DBD->rpm<uint8_t>(healthComponent + offsets::HealthState);
                    if (currentHealthState >= 0 && currentHealthState <= 2) {
                        healthState = currentHealthState;
                    }
                    // Se o estado não for 0-2, healthState continua -1, o que é ótimo.
                }
                // Se healthComponent for inválido, healthState continua -1.

                Entity.healthState = healthState; // Atribui o estado (válido ou -1)
                tempVisibleEntities.push_back(Entity); // Adiciona na lista DE QUALQUER FORMA

                // ================================================================
                // === INÍCIO DA LEITURA DE PERK/TIMER (UNBREAKABLE + DS) ===
                // ================================================================
                float decisiveStrikeDuration = 0.0f;
                float decisiveStrikeTimestamp = 0.0f;
                uintptr_t perkManager = DBD->rpm<uintptr_t>(actor + offsets::PerkManager);

                if (is_valid(perkManager))
                {
                    uintptr_t perkCollection = DBD->rpm<uintptr_t>(perkManager + offsets::_perks);
                    if (is_valid(perkCollection))
                    {
                        PointerArray perkArray = DBD->rpm<PointerArray>(perkCollection + offsets::_array);
                        if (is_valid(perkArray.data) && perkArray.count > 0)
                        {
                            std::vector<uintptr_t> perks(perkArray.count);
                            if (DBD->ReadRaw(perkArray.data, perks.data(), sizeof(uintptr_t) * perkArray.count))
                            {
                                for (uintptr_t perkPtr : perks)
                                {
                                    if (!is_valid(perkPtr)) continue;
                                    int perkId = DBD->rpm<int>(perkPtr + offsets::ActorID);
                                    std::string perkName = GetNameById(perkId);
                                    bool isUsable = DBD->rpm<bool>(perkPtr + offsets::_isUsable);

                                    // 1. Unbreakable
                                    if (healthState == 0 && isUsable && (perkName.find("Unbreakable") != std::string::npos || perkName.find("S4P1") != std::string::npos))
                                    {
                                        Entity.bIsUnbreakableActive = true;
                                    }

                                    // 2. Decisive Strike
                                    if (perkName.find("DecisiveStrike") != std::string::npos || perkName.find("K22P01") != std::string::npos)
                                    {
                                        decisiveStrikeDuration = DBD->rpm<float>(perkPtr + offsets::_timeAfterUnhook);
                                        decisiveStrikeTimestamp = DBD->rpm<float>(perkPtr + offsets::_unhookTimestamp);

                                        // O mais importante: a flag de USO e a flag de ATIVO
                                        Entity.bHasDsBeenAttempted = DBD->rpm<bool>(perkPtr + UDecisiveStrike::_hasBeenAttempted);
                                        Entity.bIsDsActive = DBD->rpm<bool>(perkPtr + UDecisiveStrike::_isTimerActiveBool);

                                        // Se o perk foi gasto, não precisamos de mais nada
                                        if (Entity.bHasDsBeenAttempted) break;
                                    }
                                }
                            }
                        }
                    }
                }
                // --- CÁLCULO FINAL DO TEMPO (USA currentServerTime) ---
                if (Entity.bIsDsActive && !Entity.bHasDsBeenAttempted && decisiveStrikeDuration > 0.0f)
                {
                    float expirationTime = decisiveStrikeTimestamp + decisiveStrikeDuration;
                    float remaining = expirationTime - currentServerTime;

                    if (remaining > 0.0f) {
                        Entity.dsTimer = remaining;
                    }
                    else {
                        // O tempo expirou, mesmo que a booleana ainda esteja TRUE (bug do jogo)
                        Entity.bIsDsActive = false;
                    }
                }
                // ================================================================
                // === FIM CÁLCULO E ATRIBUIÇÃO DO DS ===
                // ================================================================

                // ADICIONA A ENTIDADE À LISTA SÓ DEPOIS QUE TODAS AS INFOS SÃO LIDA
                tempVisibleEntities.push_back(Entity);
            }
        }

        // ==============================================================================
        //                                PROCESSA KILLER
        // ==============================================================================
        for (const auto& killer : Cache::GetList(Cache::EActorType::Killer))
        {
            uintptr_t actor = killer.instance;
            uintptr_t outlineComponent = killer.outlineComponent;
            uintptr_t playerState = DBD->rpm<uintptr_t>(actor + offsets::PlayerState);
            if (playerState == PlayerStateLocalPlayer || !is_valid(playerState)) continue;

            // --- Bloco 1: OUTLINES ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.killer.bEnableOutline)
            {
                if (is_valid(outlineComponent)) {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineKillerColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineKillerColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineKillerColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineKillerColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(killer.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.killer.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    int32_t killerIndex = DBD->rpm<int32_t>(playerState + offsets::SelectedKillerIndex);
                    std::string characterName;
                    auto it = DBDKillers::Data.find(killerIndex);
                    if (it != DBDKillers::Data.end()) {
                        characterName = it->second;
                    }
                    else {
                        std::stringstream ss;
                        ss << "Unknown Killer [" << killerIndex << "]";
                        characterName = ss.str();
                    }

                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.name = characterName;
                    Entity.bIsPinned = isPinned;
                    Entity.type = Cache::EActorType::Killer;
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                             PROCESSA GERADORES
        // ==============================================================================
        int generatorCounter = 1;
        std::vector<EntityList> tempGeneratorStatusList;
        auto nowForGenerators = std::chrono::steady_clock::now();
        for (const auto& gen : Cache::GetList(Cache::EActorType::Generator))
        {
            uintptr_t actor = gen.instance;

            uintptr_t rootComponent = DBD->rpm<uintptr_t>(actor + offsets::RootComponent);
            if (!is_valid(rootComponent)) {
                g_GameState.generatorStateMap.erase(actor);
                g_GameState.generatorNames.erase(actor);
                continue;
            }

            bool bIsRepaired = DBD->rpm<bool>(actor + offsets::_activated);
            bool bIsAutoCompleted = DBD->rpm<bool>(actor + offsets::_isAutoCompleted);

            if (bIsRepaired || bIsAutoCompleted)
            {
                g_GameState.generatorStateMap.erase(actor);
                g_GameState.generatorNames.erase(actor);

                uintptr_t outlineComponent = gen.outlineComponent;
                if (shouldUpdateOutlines && is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, 0.0f);
                }

                continue;
            }

            float percentage = 0.0f;
            float normalizedChargeValue = DBD->rpm<float>(actor + offsets::_realCharge);

            if (std::isnan(normalizedChargeValue) || normalizedChargeValue < 0.0f) {
                g_GameState.generatorStateMap.erase(actor);
                g_GameState.generatorNames.erase(actor);
                continue;
            }
            percentage = normalizedChargeValue * 100.0f;

            // --- LÓGICA DE TENDÊNCIA E VELOCIDADE (VERSÃO 2.0 - ESTÁVEL E RÁPIDA) ---
            auto& state = g_GameState.generatorStateMap[actor];

            // Inicialização (se for a primeira vez que vemos o gerador)
            if (state.lastProgress < 0.0f) {
                state.lastProgress = percentage;
                state.lastChangeTime = nowForGenerators; // Usado para 'Stagnant'
                state.lastCheckTime = nowForGenerators;  // Usado para 'Rate'
                state.progressPerSecond = 0.0f;
                state.stableProgressPerSecond = 0.0f;
            }

            // 1. CALCULAR A VELOCIDADE ATUAL (INSTANTÂNEA / "RUIM")
            float elapsedRateSeconds = std::chrono::duration_cast<std::chrono::duration<float>>(nowForGenerators - state.lastCheckTime).count();
            float currentRate = 0.0f;

            // Evita divisão por zero e picos de lag (se o frame demorar > 1s)
            if (elapsedRateSeconds > 0.001f && elapsedRateSeconds < 1.0f) {
                float progressDiff = percentage - state.lastProgress;
                currentRate = progressDiff / elapsedRateSeconds;

                // ==========================================================
                // === INÍCIO DO FILTRO DE "GREAT SKILL CHECK" (A SOLUÇÃO) ===
                // ==========================================================
                // 
                // Um great skill check (1%) em um frame (0.008s) gera um 
                // 'currentRate' de ~125.0/s.
                // Um reparo de 4 pessoas com "Prove Thyself" talvez chegue a 6.0/s ou 7.0/s.
                // 
                // Vamos definir um "teto razoável" (ex: 10.0/s). Qualquer valor
                // instantâneo acima disso é 99.9% de chance de ser um pico 
                // de skill check, e não um reparo real.

                const float MAX_SANE_INSTANT_RATE = 10.0f; // Teto de 10.0%/s

                if (currentRate > MAX_SANE_INSTANT_RATE)
                {
                    // Se a taxa for absurda (ex: +126.0/s), nós a ignoramos.
                    // Em vez de usar o pico, alimentamos a média com o 
                    // valor estável que já tínhamos (ex: +1.1/s).
                    // Isso "anula" o pico do skill check.
                    currentRate = state.progressPerSecond;
                }
                // ==========================================================
                // === FIM DO FILTRO DE "GREAT SKILL CHECK" ===
                // ==========================================================
            }

            // 2. ESTABILIZAR A VELOCIDADE (DUAL-SPEED)

            // --- VELOCIDADE RÁPIDA (Para o texto "+X.XXX/s") ---
            // Aumentamos o fator para 10% (0.10) para ser MAIS RESPONSIVO.
            // Isso mostrará a flutuação (0.9/s -> 1.1/s) que você vê.
            float fastSmoothingFactor = 0.10f;
            state.progressPerSecond = (state.progressPerSecond * (1.0f - fastSmoothingFactor)) + (currentRate * fastSmoothingFactor);

            // --- VELOCIDADE LENTA (Para o tempo "XXs") ---
            // Esta é a "média de ~3 segundos" que você sugeriu.
            // Usamos um fator BEM PEQUENO (1.5%) para ser SUPER ESTÁVEL.
            // Ela é alimentada pela 'currentRate' limpa.
            float slowSmoothingFactor = 0.01f;
            state.stableProgressPerSecond = (state.stableProgressPerSecond * (1.0f - slowSmoothingFactor)) + (currentRate * slowSmoothingFactor);

            // 3. DEFINIR A TENDÊNCIA EM TEMPO REAL (Resolve a "Demora")
            // Define a tendência com base na velocidade *suavizada*
            float rateThreshold = 0.2f; // (0.2%/s - um limite pequeno para ignorar "ruído")

            if (state.progressPerSecond > rateThreshold) {
                state.currentTrend = GeneratorTrend::Increasing;
                state.lastChangeTime = nowForGenerators; // Atualiza o timer do 'Stagnant'
            }
            else if (state.progressPerSecond < -rateThreshold) {
                state.currentTrend = GeneratorTrend::Decreasing;
                state.lastChangeTime = nowForGenerators; // Atualiza o timer do 'Stagnant'
            }
            else {
                // Fica 'Stagnant' se a velocidade for quase zero.
                // A lógica de tempo é um *fallback* caso a velocidade fique "presa" perto de 0.
                auto elapsedSinceChange = std::chrono::duration_cast<std::chrono::seconds>(nowForGenerators - state.lastChangeTime).count();
                if (elapsedSinceChange >= 5) { // Reduzido de 20s para 5s (mais responsivo)
                    state.currentTrend = GeneratorTrend::Stagnant;
                }
            }

            // 4. ATUALIZAR VALORES PARA O PRÓXIMO FRAME
            state.lastProgress = percentage;
            state.lastCheckTime = nowForGenerators; // Sempre atualiza o timer do 'Rate'
            // --- FIM DA NOVA LÓGICA ---

            std::string generatorName;
            auto it = g_GameState.generatorNames.find(actor);
            if (it == g_GameState.generatorNames.end()) {
                // Se o gerador não tem um nome, atribui um novo
                int nextId = g_GameState.generatorNames.size() + 1;
                generatorName = "Gen " + std::to_string(nextId);
                g_GameState.generatorNames[actor] = generatorName;
            }
            else {
                // Se já tem um nome, usa ele
                generatorName = it->second;
            }
            // --- FIM DA NOVA LÓGICA ---

            EntityList genStatusEntity{};
            genStatusEntity.name = generatorName;
            genStatusEntity.progress = percentage;
            genStatusEntity.trend = state.currentTrend;
            genStatusEntity.progressRate = state.progressPerSecond;
            genStatusEntity.stableProgressRate = state.stableProgressPerSecond; // <-- ADICIONE ESTA LINHA
            tempGeneratorStatusList.push_back(genStatusEntity);

            uintptr_t outlineComponent = gen.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.generator.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    float r, g, b, a; // Variáveis para armazenar a cor final

                    // Verifica se a opção de cor por progresso está ativa
                    if (g_GameState.settings.bGeneratorProgressColor) {
                        // Lógica de gradiente (Verde -> Vermelho)
                        float normalizedProgress = percentage / 100.0f;
                        r = normalizedProgress; // Vermelho aumenta com o progresso
                        g = 1.0f - normalizedProgress; // Verde diminui com o progresso
                        b = 0.0f;
                        a = 1.0f;
                    }
                    else {
                        // Lógica de cor estática (a que você já tinha)
                        r = g_GameState.colors.outlineGeneratorColor[0];
                        g = g_GameState.colors.outlineGeneratorColor[1];
                        b = g_GameState.colors.outlineGeneratorColor[2];
                        a = g_GameState.colors.outlineGeneratorColor[3];
                    }

                    // Aplica a cor calculada
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, r);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, b);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, a);
                }
            }

            bool isPinned = pinnedActorsCopy.count(gen.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.generator.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                if (origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f) continue;
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.name = generatorName;
                    Entity.bIsPinned = isPinned;
                    Entity.type = Cache::EActorType::Generator;
                    Entity.progress = percentage;
                    tempVisibleEntities.push_back(Entity);
                }
            }
            generatorCounter++;
        }

        // ==============================================================================
        //                              PROCESSA PALLETS
        // ==============================================================================
        for (const auto& pallet : Cache::GetList(Cache::EActorType::Pallet))
        {
            uintptr_t actor = pallet.instance;
            uintptr_t outlineComponent = pallet.outlineComponent;

            uint8_t stateRaw = DBD->rpm<uint8_t>(actor + offsets::PalletState);
            EPalletState palletState = static_cast<EPalletState>(stateRaw);

            // --- Bloco 1: Lógica dos OUTLINES ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.pallet.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    if (palletState == EPalletState::Destroyed)
                    {
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, 0.0f);
                    }
                    else
                    {
                        // Pallet está Ativo (Up ou Fallen), aplica a cor normal
                        float* colorToApply = (palletState == EPalletState::Up)
                            ? g_GameState.colors.outlinePalletUpColor
                            : g_GameState.colors.outlinePalletDownColor;

                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, colorToApply[0]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, colorToApply[1]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, colorToApply[2]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, colorToApply[3]); // (Garante que o Alpha é 1.0)
                    }
                }
            }

            bool isPinned = pinnedActorsCopy.count(pallet.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.pallet.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.type = Cache::EActorType::Pallet;
                    Entity.palletState = palletState;
                    Entity.bIsPinned = isPinned;

                    switch (palletState) {
                    case EPalletState::Up:      Entity.name = "Pallet (Up)"; break;
                    case EPalletState::Falling: Entity.name = "Pallet (Falling)"; break;
                    case EPalletState::Fallen:  Entity.name = "Pallet (Fallen)"; break;
                    default:                    Entity.name = "Pallet (Unknown)"; break;
                    }
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                             PROCESSA JANELAS
        // ==============================================================================
        for (const auto& window : Cache::GetList(Cache::EActorType::Window))
        {
            uintptr_t actor = window.instance;
            uintptr_t outlineComponent = window.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.window.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineWindowColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineWindowColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineWindowColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineWindowColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(window.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.window.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.bIsPinned = isPinned;
                    Entity.name = window.displayName.empty() ? "Window" : window.displayName;
                    Entity.type = Cache::EActorType::Window;
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                              PROCESSA GANCHOS
        // ==============================================================================
        for (const auto& hook : Cache::GetList(Cache::EActorType::Hook))
        {
            uintptr_t actor = hook.instance;
            uintptr_t outlineComponent = hook.outlineComponent;

            bool bIsScourgeActive = false; // Começa como Normal (false)

            uintptr_t niagaraPtr = DBD->rpm<uintptr_t>(actor + offsets::SpawnedScourgeHookNiagara);
            if (is_valid(niagaraPtr))
            {
                // bRenderingEnabled é o 2º bit (bit 1) no byte do offset
                uint8_t renderBits = DBD->rpm<uint8_t>(niagaraPtr + offsets::bRenderingEnabled);
                bool isRendering = (renderBits & (1 << 1)); // Checa o 2º bit
                if (isRendering)
                {
                    bIsScourgeActive = true; // É um Scourge Ativo!
                }
            }

            if (g_GameState.settings.bHookOnlyScourge && !bIsScourgeActive)
            {
                // Desliga ativamente o outline se for filtrado
                if (shouldUpdateOutlines && is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, 0.0f);
                }
                continue; // Pula este gancho
            }

            // --- Bloco 1: Lógica dos OUTLINES (ATUALIZADO) ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.hook.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    // Decide qual cor aplicar
                    float* colorToApply = bIsScourgeActive
                        ? g_GameState.colors.outlineHookScourgeColor
                        : g_GameState.colors.outlineHookNormalColor;

                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, colorToApply[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, colorToApply[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, colorToApply[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, colorToApply[3]); // Garante que está visível
                }
            }

            bool isPinned = pinnedActorsCopy.count(hook.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.hook.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.bIsPinned = isPinned;
                    Entity.name = "Hook"; // Nome base
                    Entity.type = Cache::EActorType::Hook;
                    Entity.bIsScourgeActive = bIsScourgeActive; // Passa o bool para a RenderThread
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                             PROCESSA ESCAPE (HATCH)
        // ==============================================================================
        for (const auto& hatch : Cache::GetList(Cache::EActorType::Hatch))
        {
            uintptr_t actor = hatch.instance;
            uintptr_t outlineComponent = hatch.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.hatch.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineHatchColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineHatchColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineHatchColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineHatchColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(hatch.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.hatch.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.bIsPinned = isPinned;
                    Entity.name = "!!! Hatch !!!";
                    Entity.type = Cache::EActorType::Hatch;
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                           PROCESSA PORTÕES DE SAÍDA
        // ==============================================================================
        for (const auto& escape : Cache::GetList(Cache::EActorType::Escape))
        {
            uintptr_t actor = escape.instance;
            uintptr_t outlineComponent = escape.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.escape.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineEscapeColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineEscapeColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineEscapeColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineEscapeColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(escape.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.escape.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.bIsPinned = isPinned;
                    Entity.name = escape.displayName.empty() ? "Escape" : escape.displayName;
                    Entity.type = Cache::EActorType::Escape;
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                               PROCESSA TOTENS
        // ==============================================================================
        for (const auto& totem : Cache::GetList(Cache::EActorType::Totem))
        {
            uintptr_t actor = totem.instance;
            uintptr_t outlineComponent = totem.outlineComponent;

            uint8_t rawState = DBD->rpm<uint8_t>(actor + offsets::TotemState);
            ETotemState totemState = static_cast<ETotemState>(rawState);

            // --- Bloco 1: Lógica dos OUTLINES (AGORA VEM PRIMEIRO) ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.totem.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    // ==================================================
                    // INÍCIO DA CORREÇÃO
                    // ==================================================

                    // Verifica se o outline deve ser pulado (se for Dull e "Hex Only" estiver ativo)
                    bool skipOutline = g_GameState.settings.bTotemOnlyHex && (totemState != ETotemState::Hex && totemState != ETotemState::Boon);

                    // CONDIÇÃO DE DESLIGAR:
                    // O totem está limpo OU é um totem "Dull" que o usuário não quer ver.
                    if (totemState == ETotemState::Cleansed || skipOutline)
                    {
                        // Força o desligamento do outline
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, 0.0f);
                    }
                    // CONDIÇÃO DE LIGAR:
                    // O totem NÃO está limpo E (é Hex/Boon OU o usuário quer ver todos)
                    else
                    {
                        // Aplica a cor apropriada (Hex/Boon ou Normal)
                        float* colorToApply = (totemState == ETotemState::Hex || totemState == ETotemState::Boon)
                            ? g_GameState.colors.outlineTotemHexColor
                            : g_GameState.colors.outlineTotemNormalColor;

                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, colorToApply[0]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, colorToApply[1]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, colorToApply[2]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, colorToApply[3]); // Garante que o Alpha está 1.0
                    }
                    // ==================================================
                    // FIM DA CORREÇÃO
                    // ==================================================
                }
            }

            // --- AGORA NÓS CHECAMOS SE O TOTEM ESTÁ LIMPO ---
            // Ignora totens já limpos para o ESP (Bloco 2)
            if (totemState == ETotemState::Cleansed) {
                continue; // Pula o Bloco 2 (ESP de texto)
            }

            bool isPinned = pinnedActorsCopy.count(totem.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.totem.bEnableESP)
            {
                Vector3 origin = DBD->rpm<Vector3>(DBD->rpm<uintptr_t>(actor + offsets::RootComponent) + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.type = Cache::EActorType::Totem;
                    Entity.totemState = totemState;
                    Entity.bIsPinned = isPinned;

                    switch (totemState) {
                    case ETotemState::Hex:   Entity.name = "Totem (Hex)"; break;
                    case ETotemState::Boon:  Entity.name = "Totem (Boon)"; break;
                    case ETotemState::Dull:  Entity.name = "Totem (Dull)"; break;
                    default:                 Entity.name = "Totem (Unknown)"; break;
                    }
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                         PROCESSA TRAPS
        // ==============================================================================
        for (const auto& traps : Cache::GetList(Cache::EActorType::Trap))
            {
                uintptr_t actor = traps.instance;
                uintptr_t outlineComponent = traps.outlineComponent;

                bool bIsTrapSet = DBD->rpm<bool>(actor + offsets::_isTrapSet);

                // --- Bloco 1: Lógica dos OUTLINES (Corrigido) ---
                if (g_GameState.settings.bTrapOnlyArmed && !bIsTrapSet)
                {
                    // ...temos que DESLIGAR ativamente o outline (caso ele estivesse ligado antes).
                    if (shouldUpdateOutlines && is_valid(outlineComponent))
                    {
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, 0.0f);
                    }

                    // Pula o resto da lógica (não vai mostrar ESP nem ligar o outline).
                    continue;
                }

                if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.traps.bEnableOutline)
                {
                    if (is_valid(outlineComponent))
                    {
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineTrapsColor[0]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineTrapsColor[1]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineTrapsColor[2]);
                        DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineTrapsColor[3]);
                    }
                }

                bool isPinned = pinnedActorsCopy.count(traps.instance) > 0;
                // --- Bloco 2: ESP ---
                if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.traps.bEnableESP)
                {
                    // Verificação de segurança CRÍTICA: Pula atores sem posição física.
                    uintptr_t rootComponent = DBD->rpm<uintptr_t>(actor + offsets::RootComponent);
                    if (!is_valid(rootComponent)) {
                        continue;
                    }

                    Vector3 origin = DBD->rpm<Vector3>(rootComponent + offsets::RelativeLocation);
                    float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                    if (DistM <= g_GameState.maxDistance)
                    {
                        EntityList Entity{};
                        Entity.instance = actor;
                        Entity.origin = origin;
                        Entity.dist = DistM;
                        Entity.bIsPinned = isPinned;
                        if (bIsTrapSet) {
                            Entity.name = "Bear Trap (Armed)";
                        }
                        else {
                            Entity.name = "Bear Trap (Disarmed)";
                        }
                        Entity.type = Cache::EActorType::Trap; // Define o tipo para a função de cor
                        tempVisibleEntities.push_back(Entity);
                    }
                }
            }

        // ==============================================================================
        //                         PROCESSA CHEST
        // ==============================================================================
        for (const auto& chests : Cache::GetList(Cache::EActorType::Chest))
        {
            uintptr_t actor = chests.instance;
            uintptr_t outlineComponent = chests.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES (Corrigido) ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.chest.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineChestColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineChestColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineChestColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineChestColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(chests.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.chest.bEnableESP)
            {
                // Verificação de segurança CRÍTICA: Pula atores sem posição física.
                uintptr_t rootComponent = DBD->rpm<uintptr_t>(actor + offsets::RootComponent);
                if (!is_valid(rootComponent)) {
                    continue;
                }

                Vector3 origin = DBD->rpm<Vector3>(rootComponent + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.name = chests.displayName;
                    Entity.bIsPinned = isPinned;
                    Entity.type = Cache::EActorType::Chest; // Define o tipo para a função de cor
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                         PROCESSA KILLER ITEMS
        // ==============================================================================
        for (const auto& killerItems : Cache::GetList(Cache::EActorType::KillerItem))
        {
            uintptr_t actor = killerItems.instance;
            uintptr_t outlineComponent = killerItems.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES (Corrigido) ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.killerItems.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineKillerItemsColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineKillerItemsColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineKillerItemsColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineKillerItemsColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(killerItems.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.killerItems.bEnableESP)
            {
                // Verificação de segurança CRÍTICA: Pula atores sem posição física.
                uintptr_t rootComponent = DBD->rpm<uintptr_t>(actor + offsets::RootComponent);
                if (!is_valid(rootComponent)) {
                    continue;
                }

                Vector3 origin = DBD->rpm<Vector3>(rootComponent + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.name = killerItems.displayName;
                    Entity.bIsPinned = isPinned;
                    Entity.type = Cache::EActorType::KillerItem; // Define o tipo para a função de cor
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        // ==============================================================================
        //                         PROCESSA TODOS OS OUTROS ACTORS (CORRIGIDO)
        // ==============================================================================
        for (const auto& all : Cache::GetList(Cache::EActorType::All))
        {
            uintptr_t actor = all.instance;
            uintptr_t outlineComponent = all.outlineComponent;

            // --- Bloco 1: Lógica dos OUTLINES (Corrigido) ---
            if (shouldUpdateOutlines && g_GameState.settings.bEnableOutlines_Global && g_GameState.settings.allActors.bEnableOutline)
            {
                if (is_valid(outlineComponent))
                {
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::InterpolationSpeed, 1.0f);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorR, g_GameState.colors.outlineAllActorsColor[0]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorG, g_GameState.colors.outlineAllActorsColor[1]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorB, g_GameState.colors.outlineAllActorsColor[2]);
                    DBD->wpm<float>(outlineComponent + offsets::OutlineComponent::ColorA, g_GameState.colors.outlineAllActorsColor[3]);
                }
            }

            bool isPinned = pinnedActorsCopy.count(all.instance) > 0;
            // --- Bloco 2: ESP ---
            if (isPinned || g_GameState.settings.bEnableESP_Global && g_GameState.settings.allActors.bEnableESP)
            {
                // Verificação de segurança CRÍTICA: Pula atores sem posição física.
                uintptr_t rootComponent = DBD->rpm<uintptr_t>(actor + offsets::RootComponent);
                if (!is_valid(rootComponent)) {
                    continue;
                }

                Vector3 origin = DBD->rpm<Vector3>(rootComponent + offsets::RelativeLocation);
                float DistM = ToMeters(g_GameState.cameraCache.POV.Location.DistTo(origin));

                if (DistM <= g_GameState.maxDistance)
                {
                    EntityList Entity{};
                    Entity.instance = actor;
                    Entity.origin = origin;
                    Entity.dist = DistM;
                    Entity.bIsPinned = isPinned;
                    Entity.name = all.displayName;
                    Entity.type = Cache::EActorType::All; // Define o tipo para a função de cor
                    tempVisibleEntities.push_back(Entity);
                }
            }
        }

        {
            std::lock_guard<std::mutex> dataLock(g_GameState.dataMutex);
            g_GameState.visibleEntities = std::move(tempVisibleEntities);
            g_GameState.generatorStatusList = std::move(tempGeneratorStatusList);
            g_GameState.cameraCache = tempCameraCache;
            g_GameState.serverTime = currentServerTime;
            g_GameState.debugInfo.cameraFOV = tempCameraCache.POV.FOV;
            g_GameState.debugInfo.cameraRotation = tempCameraCache.POV.Rotation;

        }
    }
    return nullptr;
}

inline ImU32 GetColorForEntityType(const EntityList& entity) // A assinatura não muda
{
    float* colorFloats = nullptr;

    // ========== MUDANÇA AQUI: USA SWITCH ==========
    switch (entity.type) { // Usa o enum 'type'
    case Cache::EActorType::Survivor:   colorFloats = g_GameState.colors.playerEspColor; break;
    case Cache::EActorType::Killer:     colorFloats = g_GameState.colors.killerEspColor; break;
    case Cache::EActorType::Generator:  colorFloats = g_GameState.colors.generatorEspColor; break;
    case Cache::EActorType::Totem:      colorFloats = g_GameState.colors.totemEspColor; break;
    case Cache::EActorType::Escape:     colorFloats = g_GameState.colors.escapeEspColor; break;
    case Cache::EActorType::Hatch:      colorFloats = g_GameState.colors.hatchEspColor; break;
    case Cache::EActorType::Pallet:     colorFloats = g_GameState.colors.palletEspColor; break;
    case Cache::EActorType::Window:     colorFloats = g_GameState.colors.windowEspColor; break;
    case Cache::EActorType::Hook:
        // Lógica de cor baseada no bool
        colorFloats = entity.bIsScourgeActive
            ? g_GameState.colors.hookEspScourgeColor
            : g_GameState.colors.hookEspNormalColor;
        break;
    case Cache::EActorType::Chest:      colorFloats = g_GameState.colors.chestEspColor; break;
    case Cache::EActorType::KillerItem: colorFloats = g_GameState.colors.killerItemsEspColor; break;
    case Cache::EActorType::Trap:       colorFloats = g_GameState.colors.trapEspColor; break;
    case Cache::EActorType::All:        // Fallthrough intencional
    default:                            // Trata 'All' e qualquer tipo desconhecido
        colorFloats = g_GameState.colors.allEspColor; break;
    }
    // ========== FIM DA MUDANÇA =========

    if (colorFloats) {
        return IM_COL32(
            (int)(colorFloats[0] * 255), (int)(colorFloats[1] * 255),
            (int)(colorFloats[2] * 255), (int)(colorFloats[3] * 255)
        );
    }

    // Fallback caso algo dê muito errado (não deveria acontecer com o default no switch)
    return IM_COL32(255, 255, 255, 255);
}

void CheckActiveWindow() {
    HWND game_hwnd = FindWindowA("UnrealWindow", NULL);
    // Faça essa verificação a cada 500ms, por exemplo
    if (GetForegroundWindow() == game_hwnd) {
        ShowWindow(OverlayWindow, SW_SHOW);
    }
    else {
        ShowWindow(OverlayWindow, SW_HIDE);
    }
}

inline void DrawGeneratorHUD()
{
    // 1. Pega os dados de forma segura
    std::vector<EntityList> generatorsForHUD;
    bool showHUD = false;
    {
        std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
        if (!g_GameState.bIsInMatch) return; // Não desenha se não estiver em partida
        generatorsForHUD = g_GameState.generatorStatusList;
        showHUD = g_GameState.settings.bShowGeneratorHUD;
    }

    // 2. Sai se o HUD estiver desligado ou não houver geradores
    if (!showHUD || generatorsForHUD.empty()) {
        return;
    }

    // 3. Ordena a lista do maior progresso para o menor
    std::sort(generatorsForHUD.begin(), generatorsForHUD.end(), [](const EntityList& a, const EntityList& b) {
        return a.progress > b.progress;
        });

    // 4. Configurações visuais do HUD
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    float xPos = 20.0f;
    float yPos = 30.0f;
    float panelWidth = 220.0f;
    float panelHeight = 30.0f + (generatorsForHUD.size() * 20.0f);

    ImU32 bgColor = IM_COL32(20, 20, 20, 200);
    ImU32 titleColor = IM_COL32(0, 200, 255, 255); // Ciano
    ImU32 borderColor = IM_COL32(0, 200, 255, 255);

    // 5. Desenha o fundo do painel com borda
    drawList->AddRectFilled(ImVec2(xPos, yPos), ImVec2(xPos + panelWidth, yPos + panelHeight), bgColor, 8.0f);
    drawList->AddRect(ImVec2(xPos, yPos), ImVec2(xPos + panelWidth, yPos + panelHeight), borderColor, 8.0f, 0, 1.5f);

    // 6. Título do painel
    const char* title = "Generators Progress";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(ImVec2(xPos + (panelWidth - titleSize.x) / 2, yPos + 7), titleColor, title);
    yPos += 30.0f; // Espaço após o título

    // 7. Lista os geradores
    for (const auto& gen : generatorsForHUD)
    {
        // Define o símbolo da tendência
        std::string trendSymbol = " "; // Um espaço para alinhamento quando estagnado
        switch (gen.trend) {
        case GeneratorTrend::Increasing: trendSymbol = ">"; break;
        case GeneratorTrend::Decreasing: trendSymbol = "<"; break;
        case GeneratorTrend::Stagnant:   trendSymbol = " "; break;
        }

        // Monta o texto final
        char buffer[128];

        // --- Parte 1: Texto de VELOCIDADE (ex: "(+1.100/s)") ---
        char rateBuffer[32];
        // Formata a velocidade com 3 casas decimais e sinal forçado
        sprintf_s(rateBuffer, " (%+.3f/s)", gen.progressRate);
        std::string rateText = rateBuffer;

        // --- Parte 2: Texto de TEMPO (ex: " (80s)") ---
        std::string timeEstimate = ""; // Começa vazia

        // A estimativa de tempo só faz sentido se o gerador estiver progredindo
        // (Usamos o threshold de 0.8/s que definimos anteriormente)
        if (gen.trend == GeneratorTrend::Increasing && gen.stableProgressRate > 0.8f)
        {
            float progressRemaining = 100.0f - gen.progress;
            // Calcula os segundos brutos (ex: 83.4s)
            float secondsRemaining = progressRemaining / gen.stableProgressRate;

            int roundedSeconds = 0;

            // Se faltar mais de 60s, arredonda PARA BAIXO para os 10s.
            // Ex: 89.9s -> (int)8.99 * 10 = 8 * 10 = 80s.
            // Ex: 80.1s -> (int)8.01 * 10 = 8 * 10 = 80s.
            if (secondsRemaining > 60.0f) {
                roundedSeconds = static_cast<int>(secondsRemaining / 10.0) * 10;
            }
            // Se faltar entre 20s e 60s, arredonda PARA BAIXO para os 5s.
            // Ex: 49.9s -> (int)9.98 * 5 = 9 * 5 = 45s.
            // Ex: 45.1s -> (int)9.02 * 5 = 9 * 5 = 45s.
            else if (secondsRemaining > 20.0f) {
                roundedSeconds = static_cast<int>(secondsRemaining / 5.0) * 5;
            }
            // Se faltar menos de 20s, arredondamos para o segundo MAIS PRÓXIMO (como antes).
            // A variação aqui é boa e esperada.
            else {
                roundedSeconds = static_cast<int>(std::round(secondsRemaining));
            }
            // === FIM DA LÓGICA DE TRUNCAMENTO ===

            // Formata o tempo (ex: " (80s)")
            char timeBuffer[16];
            sprintf_s(timeBuffer, " (%ds)", roundedSeconds);
            timeEstimate = timeBuffer; // Agora timeEstimate é algo como " (80s)"
        }

        // --- Parte 3: Montagem Final ---
        // Combina tudo: Nome - Trend - % - Velocidade - TempoEstimado
        sprintf_s(buffer, "%s - %s %.0f%%%s%s",
            gen.name.c_str(),
            trendSymbol.c_str(),
            gen.progress,
            rateText.c_str(),     // (ex: " (+1.100/s)")
            timeEstimate.c_str()  // (ex: " (80s)")
        );
        // 1. Normaliza o progresso para uma escala de 0.0 a 1.0
        float normalizedProgress = gen.progress / 100.0f;
        // Garante que o valor não saia do intervalo, por segurança
        normalizedProgress = std::max(0.0f, std::min(1.0f, normalizedProgress));

        // 2. Calcula os componentes de cor Vermelho e Verde
        // O Vermelho aumenta de 0 para 255 conforme o progresso aumenta.
        int red = static_cast<int>(255.0f * normalizedProgress);
        // O Verde diminui de 255 para 0 conforme o progresso aumenta.
        int green = static_cast<int>(255.0f * (1.0f - normalizedProgress));

        // 3. Monta a cor final. O Azul (B) é sempre 0.
        ImU32 textColor = IM_COL32(red, green, 0, 255);

        drawList->AddText(ImVec2(xPos + 15, yPos), textColor, buffer);
        yPos += 20.0f;
    }
}

// --- FUNÇÃO DE RENDERIZAÇÃO PRINCIPAL ---
void RenderESP()
{
    // =======================================================================
    // ====================== INÍCIO DO CÓDIGO DE FPS ========================
    // =======================================================================
    static int frameCount = 0;
    static auto lastTime = std::chrono::steady_clock::now();

    frameCount++;

    auto currentTime = std::chrono::steady_clock::now();
    auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime).count();

    if (elapsedTime >= 1)
    {
        // Trava o mutex para escrever o valor de forma segura
        std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
        g_GameState.debugInfo.espFps = frameCount;

        // Reseta para o próximo segundo
        frameCount = 0;
        lastTime = currentTime;
    }
    // =======================================================================
    // ======================== FIM DO CÓDIGO DE FPS =========================
    // =======================================================================
    // 
    // Controla a visibilidade do overlay

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // 1. FAZ UMA CÓPIA SEGURA DOS DADOS NECESSÁRIOS PARA RENDERIZAR
    // Esta é a única interação desta thread com a g_GameState.
    std::vector<EntityList> entitiesToRender;
    FCameraCacheEntry cameraForRender;
    bool bIsEspGlobalOn = false; // Precisamos do switch global
    {
        std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
        entitiesToRender = g_GameState.visibleEntities; // Copia a lista de entidades
        cameraForRender = g_GameState.cameraCache;     // Copia a câmera usada para o cálculo de W2S
        bIsEspGlobalOn = g_GameState.settings.bEnableESP_Global; // Copia o switch global
    }

    // Se o ESP global estiver desligado, não desenha nada
    if (!bIsEspGlobalOn) {
        DrawGeneratorHUD(); // O HUD do gerador é separado, então ainda o chamamos
        return;
    }

    // 2. DESENHA TODAS AS ENTIDADES USANDO OS DADOS COPIADOS
    // Este loop opera em dados locais, garantindo que não haverá crashes
    // se a LogicThread modificar a lista original.
    for (const auto& Entity : entitiesToRender)
    {

        // ==================================================================
        // INÍCIO DA LÓGICA DE RENDERIZAÇÃO ATUALIZADA
        // ==================================================================

        // 1. Pega as configurações corretas para este tipo de entidade
        //    (Usamos um ponteiro para evitar copiar a struct inteira)
        const ActorSettings* pSettings = nullptr;
        {
            // Trava rápida para acessar o g_GameState.settings
            // Isso é rápido, pois é apenas leitura
            std::lock_guard<std::mutex> lock(g_GameState.dataMutex);
            switch (Entity.type) { // Usa o enum 'type'
            case Cache::EActorType::Survivor:   pSettings = &g_GameState.settings.survivor; break;
            case Cache::EActorType::Killer:     pSettings = &g_GameState.settings.killer; break;
            case Cache::EActorType::Generator:  pSettings = &g_GameState.settings.generator; break;
            case Cache::EActorType::Totem:      pSettings = &g_GameState.settings.totem; break;
            case Cache::EActorType::Escape:     pSettings = &g_GameState.settings.escape; break;
            case Cache::EActorType::Hatch:      pSettings = &g_GameState.settings.hatch; break;
            case Cache::EActorType::Pallet:     pSettings = &g_GameState.settings.pallet; break;
            case Cache::EActorType::Window:     pSettings = &g_GameState.settings.window; break;
            case Cache::EActorType::Hook:       pSettings = &g_GameState.settings.hook; break;
            case Cache::EActorType::Chest:      pSettings = &g_GameState.settings.chest; break;
            case Cache::EActorType::KillerItem: pSettings = &g_GameState.settings.killerItems; break;
            case Cache::EActorType::Trap:       pSettings = &g_GameState.settings.traps; break;
            case Cache::EActorType::All:        // Fallthrough intencional
            default:                            // Trata 'All' e qualquer tipo desconhecido
                pSettings = &g_GameState.settings.allActors; break;
            }

            // Verificação de segurança
            if (pSettings == nullptr) {
                continue;
            }

            // --- LÓGICA DE RENDERIZAÇÃO CORRIGIDA ---
            // Vamos pular o desenho SOMENTE SE:
            // 1. O ESP para este ator estiver DESLIGADO nas settings
            // E
            // 2. O ator NÃO estiver "fixado" (pinned)
            if (!pSettings->bEnableESP && !Entity.bIsPinned) {
                continue;
            }

            Vector3 renderPosition = Entity.origin;

            // --- NOVA LÓGICA DE POSICIONAMENTO ---
            if (Entity.type == Cache::EActorType::Survivor || Entity.type == Cache::EActorType::Killer) {
                renderPosition.z += 10.0f;
            }

            renderPosition.z -= 50.0f;

            // Usa a câmera copiada para garantir que a posição na tela corresponda aos dados da entidade
            Vector3 Screen = WorldToScreen(cameraForRender.POV, renderPosition);

            // Pula entidades que estão fora da tela
            if (Screen.x <= 0 || Screen.y <= 0) continue;

            // Pega a cor apropriada para o tipo de entidade
            ImU32 entityColor = GetColorForEntityType(Entity);

            // Monta o texto a ser exibido
            std::string name_text = Entity.name;
            if (pSettings->bShowDistance) {
                name_text += " [" + std::to_string(static_cast<int>(Entity.dist)) + "m]";
            }

            float status_y_offset = 0.0f;

            // --- USA A FONTE g_espFontName ---
            if (pSettings->bShowName && g_espFontName != nullptr) {
                ImGui::PushFont(g_espFontName); // Ativa a fonte de nome
                ImVec2 textSize = ImGui::CalcTextSize(name_text.c_str());
                drawList->AddText(ImVec2(Screen.x - (textSize.x / 2.0f), Screen.y), entityColor, name_text.c_str());
                status_y_offset = g_espFontName->FontSize + 1.0f;
                ImGui::PopFont();
            }

            // Desenha informações de status (vida, progresso, etc.)
            if (pSettings->bShowStatus) {
                std::string status_text;
                ImU32 statusColor = IM_COL32(255, 255, 255, 255); // Cor padrão

                // 1. Prepara o texto e a cor do status baseado no tipo da entidade
                if (Entity.type == Cache::EActorType::Survivor) {
                    switch (Entity.healthState) {
                    case 0: status_text = "Downed";  statusColor = IM_COL32(255, 0, 0, 255); break;
                    case 1: status_text = "Injured"; statusColor = IM_COL32(255, 255, 0, 255); break;
                    case 2: status_text = "Healthy"; statusColor = IM_COL32(0, 255, 0, 255); break;
                    }
                }
                else if (Entity.type == Cache::EActorType::Generator && Entity.progress >= 0.0f) {
                    // Usando a lógica de gradiente de cor que fizemos
                    float normalizedProgress = Entity.progress / 100.0f;
                    int red = static_cast<int>(255.0f * normalizedProgress);
                    int green = static_cast<int>(255.0f * (1.0f - normalizedProgress));
                    statusColor = IM_COL32(red, green, 0, 255);
                    status_text = "[" + std::to_string((int)Entity.progress) + "%]";
                }
                else if (Entity.type == Cache::EActorType::Pallet) {
                    EPalletState palletState = static_cast<EPalletState>(Entity.palletState);
                    switch (palletState) {
                    case EPalletState::Up:        status_text = "Up";        statusColor = IM_COL32(0, 255, 0, 255); break;
                    case EPalletState::Falling:   status_text = "Falling";   statusColor = IM_COL32(0, 200, 255, 255); break;
                    case EPalletState::Fallen:    status_text = "Fallen";    statusColor = IM_COL32(255, 255, 0, 255); break;
                    case EPalletState::Destroyed: status_text = "Destroyed"; statusColor = IM_COL32(255, 0, 0, 255); break;
                    default:                      status_text = "Unknown";   break;
                    }
                }
                else if (Entity.type == Cache::EActorType::Hook) {
                    if (Entity.bIsScourgeActive) { // Simplesmente checa o bool
                        status_text = "(SCOURGE)";
                        statusColor = IM_COL32(255, 220, 50, 255); // Dourado
                    }
                }
                float current_status_y = status_y_offset;

                // 2. Desenha o Status Principal (se houver)
                if (!status_text.empty() && g_espFontStatus != nullptr) {
                    ImGui::PushFont(g_espFontStatus);
                    ImVec2 textSize = ImGui::CalcTextSize(status_text.c_str());
                    drawList->AddText(ImVec2(Screen.x - (textSize.x / 2.0f), Screen.y + current_status_y), statusColor, status_text.c_str());
                    ImGui::PopFont();

                    // IMPORTANTE: Incrementa o Y para a *próxima* linha
                    current_status_y += g_espFontStatus->FontSize + 1.0f;
                }

                // 3. Desenha o status "Unbreakable" (se ativo)
                if (Entity.bIsUnbreakableActive) //
                {
                    ImU32 unbColor = IM_COL32(0, 255, 0, 255);
                    std::string unbText = "(UNBREAKABLE)";

                    if (g_espFontStatus != nullptr) {
                        ImGui::PushFont(g_espFontStatus);
                        ImVec2 textSize = ImGui::CalcTextSize(unbText.c_str());
                        // Desenha na posição Y atual
                        drawList->AddText(ImVec2(Screen.x - (textSize.x / 2.0f), Screen.y + current_status_y), unbColor, unbText.c_str());
                        ImGui::PopFont();

                        // IMPORTANTE: Incrementa o Y para a *próxima* linha
                        current_status_y += g_espFontStatus->FontSize + 1.0f;
                    }
                }

                // 3. Desenha o timer "DS" (SE ATIVO)
                // Checa APENAS a flag que indica que o timer está rodando E se ainda não foi gasto.
                if (Entity.bIsDsActive && !Entity.bHasDsBeenAttempted)
                {
                    ImU32 dsColor = IM_COL32(255, 100, 100, 255); // Vermelho Chamativo (Ativo)
                    char dsBuffer[48];
                    std::string dsText;

                    if (Entity.dsTimer > 0.0f) {
                        // Se houver tempo restante, mostre
                        sprintf_s(dsBuffer, "DS ATIVO: %.1fs restantes", Entity.dsTimer);
                        dsText = dsBuffer;
                    }
                    else {
                        // Se a flag estiver TRUE, mas o cálculo deu 0 (ou seja, está quase acabando)
                        dsText = "DS ATIVO: EXPIRANDO!";
                        dsColor = IM_COL32(255, 255, 0, 255); // Amarelo (Alerta)
                    }

                    if (g_espFontStatus != nullptr) {
                        ImGui::PushFont(g_espFontStatus);
                        ImVec2 textSize = ImGui::CalcTextSize(dsText.c_str());
                        drawList->AddText(ImVec2(Screen.x - (textSize.x / 2.0f), Screen.y + current_status_y), dsColor, dsText.c_str());
                        ImGui::PopFont();
                        current_status_y += g_espFontStatus->FontSize + 1.0f; // Move a linha
                    }
                }
                // 4. Desenha o DS GASTO
                else if (Entity.bHasDsBeenAttempted) // <--- Se não está ativo, mas foi gasto
                {
                    ImU32 dsColor = IM_COL32(100, 100, 100, 255); // Cinza (Gasto)
                    std::string dsText = "DS: GASTO";

                    if (g_espFontStatus != nullptr) {
                        ImGui::PushFont(g_espFontStatus);
                        ImVec2 textSize = ImGui::CalcTextSize(dsText.c_str());
                        drawList->AddText(ImVec2(Screen.x - (textSize.x / 2.0f), Screen.y + current_status_y), dsColor, dsText.c_str());
                        ImGui::PopFont();
                        current_status_y += g_espFontStatus->FontSize + 1.0f; // Move a linha
                    }
                }
            }

            // Desenha as linhas que apontam para as entidades
            if (pSettings->bDrawLine) {
                float screenCenterX = ImGui::GetIO().DisplaySize.x / 2.0f;
                float screenBottomY = ImGui::GetIO().DisplaySize.y;
                drawList->AddLine(ImVec2(screenCenterX, screenBottomY), ImVec2(Screen.x, Screen.y), entityColor);
            }
        }
        DrawGeneratorHUD();
    }
}