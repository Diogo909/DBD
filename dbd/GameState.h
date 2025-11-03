#pragma once
#include "global.h" // Adicionado para que o compilador conheça a struct DebugInfo
#include "util.h"   // Adicionado para que o compilador conheça a struct EntityList
#include <string>
#include <vector>
#include <mutex>
#include <set>

// NOVA STRUCT: Define as configurações de exibição para CADA tipo de ator
struct ActorSettings {
    bool bEnableESP = true;      // Controla o ESP de texto (Nome, Dist, Status)
    bool bShowName = true;
    bool bShowDistance = true;
    bool bShowStatus = true;     // Ex: Vida (Survivor), Progresso (Gen), Estado (Pallet/Totem)
    bool bDrawLine = false;      // Controla a linha (snapline)
    bool bEnableOutline = true;  // Controla o Outline (brilho)

    // Construtor para facilitar a inicialização
    ActorSettings(bool esp, bool name, bool dist, bool status, bool line, bool outline)
        : bEnableESP(esp), bShowName(name), bShowDistance(dist), bShowStatus(status), bDrawLine(line), bEnableOutline(outline) {
    }

    // Construtor padrão
    ActorSettings() = default;
};


// NOVA STRUCT MASTER: Substitui as antigas Settings_ESP e Settings_Outlines
struct Settings_Master {
    // --- Master Switches ---
    bool bEnableESP_Global = true;      // O "Master Switch" para todo o ESP de texto
    bool bEnableOutlines_Global = true; // O "Master Switch" para todos os Outlines

    // --- Configurações Individuais por Ator ---
    //             (ESP,  Name, Dist, Status, Line,  Outline)
    ActorSettings survivor{ true, true, true, true,  false, true };
    ActorSettings killer{ true, true, true, false, false, true };  // Killer não tem "status" no ESP
    ActorSettings generator{ true, true, true, true,  false, true };
    ActorSettings totem{ false,true, true, true,  false, true };
    ActorSettings escape{ false,true, true, false, false, true };  // Escape não tem "status"
    ActorSettings hatch{ true, true, true, false, false, true };  // Hatch não tem "status"
    ActorSettings pallet{ false,true, true, true,  false, true };
    ActorSettings window{ false,true, true, false, false, false }; // Window não tem "status"
    ActorSettings hook{ false,true, true, false, false, false }; // Hook não tem "status"
    ActorSettings traps{ true,true, false, false, false, true }; //
    ActorSettings chest{ false,true, false, false, false, false }; //
    ActorSettings killerItems{ false, true, false, false, false, false }; //
    ActorSettings allActors{ false,true, true, false, false, false }; // O "ShowAll"

    // --- Configurações Especiais que não são por ator ---
    bool bShowGeneratorHUD = true;           // Controla o painel de HUD dos geradores
    bool bGeneratorProgressColor = true;     // Específico do Outline de Gerador
    bool bTotemOnlyHex = true;               // Específico do Outline de Totem
    bool bTrapOnlyArmed = true;
    bool bHookOnlyScourge = false;

    bool bPatch_SSLBypass = true;
    bool bPatch_EnableButton = true;
};


struct Settings_Colors {
    float playerEspColor[4] = { 0.4f, 0.8f, 1.0f, 1.0f };           // Azul claro, fácil de ver
    float killerEspColor[4] = { 1.0f, 0.1f, 0.1f, 1.0f };           // Vermelho forte, perigo
    float generatorEspColor[4] = { 1.0f, 0.85f, 0.2f, 1.0f };       // Amarelo dourado, objetivo importante
    float totemEspColor[4] = { 0.2f, 0.6f, 0.8f, 1.0f };            // Azul-petróleo, mais sóbrio para totems
    float escapeEspColor[4] = { 0.2f, 0.9f, 0.4f, 1.0f };           // Verde vivo, para saídas
    float hatchEspColor[4] = { 0.4f, 0.0f, 1.0f, 1.0f };            // Roxo mais escuro, escape especial
    float palletEspColor[4] = { 1.0f, 0.6f, 0.0f, 1.0f };           // Laranja queimado, recurso importante
    float windowEspColor[4] = { 0.9f, 0.9f, 0.9f, 1.0f };           // Cinza claro, neutro para janelas
    float hookEspNormalColor[4] = { 1.0f, 0.1f, 0.5f, 1.0f };         // Rosa (Padrão)
    float hookEspScourgeColor[4] = { 1.0f, 0.85f, 0.2f, 1.0f };  // Amarelo/Dourado
    float trapEspColor[4] = { 0.9f, 0.1f, 0.1f, 1.0f };           // Vermelho forte, perigo
    float chestEspColor[4] = { 1.0f, 0.5f, 0.8f, 1.0f };            // Rosa
    float killerItemsEspColor[4] = { 1.0f, 0.5f, 0.0f, 1.0f };    // Laranja
    float allEspColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };           // Cor do ESP (Branco)
    

    // --- Outline ---
    float outlineSurvivorHealthyColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };   // Cinza claro, menos agressivo que branco
    float outlineSurvivorInjuredColor[4] = { 1.0f, 0.45f, 0.0f, 1.0f };  // Laranja mais escuro para feridos
    float outlineKillerColor[4] = { 1.0f, 0.1f, 0.1f, 1.0f };            // Vermelho vivo para killer
    float outlineGeneratorColor[4] = { 1.0f, 0.9f, 0.1f, 1.0f };         // Amarelo forte, destaque
    float outlineEscapeColor[4] = { 0.1f, 1.0f, 0.5f, 1.0f };            // Verde brilhante para escapes
    float outlineHatchColor[4] = { 0.3f, 0.0f, 0.9f, 1.0f };             // Azul escuro para hatch
    float outlineTotemNormalColor[4] = { 0.6f, 0.6f, 0.6f, 1.0f };       // Cinza médio, neutro para totem desligado
    float outlineTotemHexColor[4] = { 1.0f, 0.3f, 0.0f, 1.0f };          // Vermelho alaranjado para totem ativo
    float outlineHookNormalColor[4] = { 0.85f, 0.15f, 0.15f, 1.0f };   // Vermelho (Padrão)
    float outlineHookScourgeColor[4] = { 1.0f, 0.9f, 0.1f, 1.0f };      // Cor do Outline(Branco)
    float outlinePalletUpColor[4] = { 0.0f, 1.0f, 0.1f, 1.0f };          // Verde forte para pallet disponível
    float outlinePalletDownColor[4] = { 0.55f, 0.55f, 0.55f, 1.0f };     // Cinza para pallet usado
    float outlineWindowColor[4] = { 0.6f, 0.0f, 0.8f, 1.0f };            // Roxo médio para janelas
    float outlineTrapsColor[4] = { 1.0f, 0.1f, 0.1f, 1.0f };            // Vermelho vivo para traps
    float outlineChestColor[4] = { 1.0f, 0.5f, 0.8f, 1.0f };
    float outlineKillerItemsColor[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
    float outlineAllActorsColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // Cor do Outline (Branco)
};


struct GeneratorState {
    float lastProgress = -1.0f;
    GeneratorTrend currentTrend = GeneratorTrend::Stagnant;
    std::chrono::steady_clock::time_point lastChangeTime;
    // === ADICIONE ESTAS DUAS LINHAS ===
    float progressPerSecond = 0.0f;
    float stableProgressPerSecond = 0.0f;
    std::chrono::steady_clock::time_point lastCheckTime;
};

class GameState {
public:
    // --- Configurações do Usuário ---
    // AS DUAS STRUCTS ANTIGAS FORAM COMBINADAS NESTA:
    Settings_Master settings;
    Settings_Colors colors;

    bool bEnableAuthAndDisableDebug = false;

    bool bAutoSkillCheck = true;
    float customFOV = 90.0f;
    float maxDistance = 250.0f;
    bool bShowMenu = true;

    float espNameFontSize = 12.0f;
    float espStatusFontSize = 10.0f;

    // --- NOVAS VARIÁVEIS PARA AS TECLAS ---
    int keyMenu = VK_INSERT; // Tecla para abrir/fechar o menu (Padrão: Insert)
    int keyExit = VK_END; // Tecla para fechar o programa (Padrão: Delete)

    // --- Dados do Jogo (Atualizados pela Thread de Lógica) ---
    std::mutex dataMutex; // Um único mutex para proteger os dados abaixo

    // Ponteiros e informações essenciais
    bool bIsInMatch = false;
    float serverTime = 0.0f; // <-- ADICIONE ESTA LINHA
    FCameraCacheEntry cameraCache;

    // Lista de entidades para renderizar
    std::vector<EntityList> visibleEntities;
    std::vector<EntityList> generatorStatusList;
    std::map<uintptr_t, GeneratorState> generatorStateMap;
    std::map<uintptr_t, std::string> generatorNames;

    // Informações do Killer
    std::string killerName = "Killer: N/A";
    std::vector<std::string> killerPerks;
    std::vector<std::string> killerAddons;

    // Informações de Debug
    DebugInfo debugInfo;

    // --- NOVO RECURSO: ATORES FIXADOS ---
    std::set<uintptr_t> pinnedActors;
    // ------------------------------------

    // NOVA FUNÇÃO: Limpa APENAS as entidades visuais para o ESP.
    void ClearVisibleEntities() {
        std::lock_guard<std::mutex> lock(dataMutex);
        visibleEntities.clear();
    }

    // --- Métodos (Opcional, mas boa prática) ---
    // Limpa os dados ao sair de uma partida
    void ClearMatchData() {
        std::lock_guard<std::mutex> lock(dataMutex);
        visibleEntities.clear();
        generatorStatusList.clear();
        generatorStateMap.clear();
        generatorNames.clear();
        killerName = "Killer: N/A";
        killerPerks.clear();
        killerAddons.clear();
        pinnedActors.clear();
        bIsInMatch = false;
    }
};

extern GameState g_GameState;