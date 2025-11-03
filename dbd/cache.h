#pragma once
#include <map>
#include <array>
#include <string>
#include <vector>
#include <mutex>
#include "GameState.h" // Precisa ser incluído ANTES de usar g_GameState
#include "offset.h"
#include "global.h"     // Para DBD, process_base, rendering
#include "struct.h"   // Para PointerArray, FName (usado em GetNameById indiretamente)
// #include "func.h"    // REMOVIDO - is_valid virá de outro lugar
#include "util.h" // Inclui is_valid


// Coloque GetNameById aqui ou em um header incluído ANTES daqui
inline std::string GetNameById(uint32_t actor_id)
{
    // Verifica se DBD e process_base são válidos antes de usar
    if (!DBD || process_base == 0 || actor_id == 0) return std::string("NULL");

    char pNameBuffer[128];
    uint64_t GNameTable = process_base + offsets::GNames;

    // Adiciona validação para GNameTable
    if (!is_valid(GNameTable)) return std::string("NULL");


    // Calcula TableLocation e RowLocation (como antes)
    int TableLocation = (unsigned int)(actor_id >> 0x10);
    uint16_t RowLocation = (unsigned __int16)actor_id;

    // Calcula TableLocationAddress (como antes)
    uint64_t TablePtrAddress = GNameTable + 0x10 + TableLocation * 0x8;
    if (!is_valid(TablePtrAddress)) return std::string("NULL"); // Valida endereço do ponteiro da tabela
    uint64_t TableAddress = DBD->rpm<uint64_t>(TablePtrAddress);
    if (!is_valid(TableAddress)) return std::string("NULL"); // Valida endereço da tabela

    uint64_t EntryAddress = TableAddress + (unsigned int)(4 * RowLocation);
    if (!is_valid(EntryAddress)) return std::string("NULL"); // Valida endereço da entrada


    // O tamanho está nos 2 bytes após os 4 bytes do índice (ajustado para UE4.2x+)
    // O bit mais baixo indica se é widechar, os outros 15 são o tamanho.
    uint16_t nameHeader = DBD->rpm<uint16_t>(EntryAddress + 4);
    uint16_t sLength = nameHeader >> 1; // Pega o tamanho
    bool isWide = nameHeader & 1; // Verifica se é wide


    // Validação extra de tamanho
    if (sLength > 0 && sLength < 128)
    {
        uint64_t NameAddress = EntryAddress + 6; // O nome começa após o header
        if (!is_valid(NameAddress)) return std::string("NULL");


        if (isWide) {
            // Se for widechar, precisamos ler wchar_t
            wchar_t wNameBuffer[128] = { 0 };
            DBD->ReadRaw(NameAddress, wNameBuffer, sLength * sizeof(wchar_t));
            std::wstring wstr(wNameBuffer, sLength);


            // Converte para std::string (UTF-8)
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                std::string strTo(size_needed, 0);
                WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
                return strTo;
            }
        }
        else {
            // Se for char normal (ANSI)
            DBD->ReadRaw(NameAddress, pNameBuffer, sLength);
            return std::string(pNameBuffer, sLength); // Cria string com tamanho exato
        }
    }
    return std::string("NULL");
}

// ========================================================================
// CACHE DE OBJETOS E NAMESPACE
// ========================================================================

inline uintptr_t FindOutlineComponentForCache(uintptr_t actor) {
    if (!DBD || !is_valid(actor)) return 0; // Validação

    uintptr_t compArrayAddr = actor + offsets::BlueprintCreatedComponents;
    uintptr_t compArray = DBD->rpm<uintptr_t>(compArrayAddr);
    int compCount = DBD->rpm<int>(compArrayAddr + 0x8);

    // Validação adicional
    if (!is_valid(compArray) || compCount <= 0 || compCount > 500) // Limite razoável
        return 0;


    for (int j = 0; j < compCount; j++) {
        uintptr_t compPtrAddr = compArray + (uintptr_t)j * sizeof(uintptr_t);
        if (!is_valid(compPtrAddr)) continue; // Valida endereço do ponteiro
        uintptr_t compPtr = DBD->rpm<uintptr_t>(compPtrAddr);
        if (!is_valid(compPtr)) continue;

        int compId = DBD->rpm<int>(compPtr + offsets::ActorID);
        if (compId <= 0) continue; // ID inválido
        std::string compName = GetNameById(compId);


        if (compName.find("DBDOutline") != std::string::npos) {
            return compPtr;
        }
    }
    return 0;
}

namespace Cache
{
    enum class EActorType {
        Survivor, Killer, Generator, Pallet, Window, Hook, Hatch, Escape, Totem, Chest, Trap, KillerItem,
        All, COUNT
    };

    struct CachedObject {
        uintptr_t instance = 0;
        uintptr_t outlineComponent = 0;
        std::string displayName; // Nome "limpo" para o ESP (ex: "Pallet")
        std::string rawName;     // Nome "cru" do GetNameById (ex: "BP_Pallet_01")
    };

    inline std::array<std::vector<CachedObject>, (size_t)EActorType::COUNT> actorCache;
    inline std::mutex cacheMutex;

    inline const std::vector<CachedObject>& GetList(EActorType type) {
        size_t index = static_cast<size_t>(type);
        if (index < actorCache.size()) {
            return actorCache[index];
        }
        static const std::vector<CachedObject> emptyList;
        return emptyList;
    }
}

// ========================================================================
// O VETOR DE BUSCA (ATUALIZADO)
// ========================================================================
const std::vector<std::pair<std::string, std::pair<Cache::EActorType, std::string>>> g_ActorLookupVector = {
    // Ordem: Mais específico para mais genérico
    {"GroundPortal",                 {Cache::EActorType::KillerItem, "Abyss Portal"}},
    {"DreamSnare",                   {Cache::EActorType::KillerItem, "Snare"}},
    {"DreamPallet_C",                {Cache::EActorType::KillerItem, "Dream Pallet"}},
    {"WakerObject",                  {Cache::EActorType::KillerItem, "Alarm Clock"}},
    {"PhantomTrap",                  {Cache::EActorType::KillerItem, "Phantom Trap"}},
    {"ReverseBearTrapRemover",       {Cache::EActorType::KillerItem, "Jigsaw Box"}},
    {"MagicFountain",                {Cache::EActorType::KillerItem, "Fountain"}},
    {"ZombieCharacter",              {Cache::EActorType::KillerItem, "Zombie"}},
    {"SupplyCrate_BP_C",             {Cache::EActorType::KillerItem, "Vaccine Crate"}},
    {"BP_K29SupplyCrate_C",          {Cache::EActorType::KillerItem, "Spray Crate"}},
    {"InfectionRemovalCollectable",  {Cache::EActorType::KillerItem, "Spray"}},
    {"LamentConfiguration",          {Cache::EActorType::KillerItem, "Lament Configuration"}},
    {"OnryoTelevision",              {Cache::EActorType::KillerItem, "Television"}},
    {"BP_K32ItemBox_C",              {Cache::EActorType::KillerItem, "EMP Printer"}},
    {"BP_K32KillerPod_C",            {Cache::EActorType::KillerItem, "Biopod"}},
    {"K33ControlStation",            {Cache::EActorType::KillerItem, "Control Station"}},
    {"K33Turret",                    {Cache::EActorType::KillerItem, "Flame Turret"}},
    {"BP_K35KillerTeleportPoint_C",  {Cache::EActorType::KillerItem, "Hallucination"}},
    {"BP_K36TreasureChest_C",        {Cache::EActorType::KillerItem, "Magic Chest"}},
    {"K31Drone",                     {Cache::EActorType::KillerItem, "Drone"}},
    //{"BP_BearTrap_001_C",            {Cache::EActorType::Trap,       "Bear Trap"}},
    {"BearTrap_C",                   {Cache::EActorType::Trap,       "Bear Trap"}},
    {"BP_CamperMale",                {Cache::EActorType::Survivor,   "Survivor"}},
    {"BP_CamperFemale",              {Cache::EActorType::Survivor,   "Survivor"}},
    {"BP_ConjoinedTwin_C",           {Cache::EActorType::Killer,     "Killer"}},
    {"Slasher_Character",            {Cache::EActorType::Killer,     "Killer"}},
    {"Hatch01_",                     {Cache::EActorType::Hatch,      "Hatch"}},
    {"BP_Pallet_",                   {Cache::EActorType::Pallet,     "Pallet"}},
    {"WindowStandard_",              {Cache::EActorType::Window,     "Window"}},
    {"Chest_",                       {Cache::EActorType::Chest,      "Chest"}},
    {"TotemBase_",                   {Cache::EActorType::Totem,      "Totem"}},
    {"BP_EscapeBlocker",             {Cache::EActorType::Escape,     "Escape"}},
    {"BP_EscapeBase_C",              {Cache::EActorType::Escape,     "Escape"}},
    {"SmallMeatLocker",              {Cache::EActorType::Hook,       "Hook"}},
    {"GeneratorStandard",            {Cache::EActorType::Generator,  "Generator"}},
    {"GeneratorNoPole",              {Cache::EActorType::Generator,  "Generator"}},
    {"GeneratorIndoor",              {Cache::EActorType::Generator,  "Generator"}},
    {"GeneratorShort",               {Cache::EActorType::Generator,  "Generator"}},
};

// ========================================================================
// CACHE THREAD (ATUALIZADA)
// ========================================================================
inline void CacheThread()
{
    while (rendering)
    {
        Sleep(16);

        std::array<std::vector<Cache::CachedObject>, (size_t)Cache::EActorType::COUNT> tempCache;

        int debug_levelCount = 0;
        int debug_validActorClusters = 0;
        int debug_totalActorsFound = 0;
        int debug_actorsIdentified = 0;

        if (!DBD || process_base == 0) {
            { std::lock_guard<std::mutex> lock(Cache::cacheMutex); for (auto& list : Cache::actorCache) list.clear(); }
            { std::lock_guard<std::mutex> debugLock(g_GameState.dataMutex); g_GameState.debugInfo.uWorld = "Driver Invalido"; g_GameState.debugInfo.isInMatch = false; }
            Sleep(1000); continue;
        }

        uintptr_t uWorld_ptr = DBD->rpm<uintptr_t>(process_base + offsets::GWorld);
        if (!is_valid(uWorld_ptr)) {
            { std::lock_guard<std::mutex> lock(Cache::cacheMutex); for (auto& list : Cache::actorCache) list.clear(); }
            { std::lock_guard<std::mutex> debugLock(g_GameState.dataMutex); g_GameState.debugInfo.uWorld = "Invalido"; g_GameState.debugInfo.isInMatch = false; }
            continue;
        }

        uintptr_t localPawn_ptr = 0;
        uintptr_t gameInstance_ptr = 0; // Inicializa
        uintptr_t playerController_ptr = 0; // Inicializa

        gameInstance_ptr = DBD->rpm<uintptr_t>(uWorld_ptr + offsets::OwningGameInstance);
        if (is_valid(gameInstance_ptr)) {
            uintptr_t localPlayer_ptr = DBD->rpm<uintptr_t>(DBD->rpm<uintptr_t>(gameInstance_ptr + offsets::LocalPlayers));
            if (is_valid(localPlayer_ptr)) {
                playerController_ptr = DBD->rpm<uintptr_t>(localPlayer_ptr + offsets::PlayerController);
                if (is_valid(playerController_ptr)) {
                    localPawn_ptr = DBD->rpm<uintptr_t>(playerController_ptr + offsets::AcknowledgedPawn);
                }
            }
        }

        PointerArray levelsArray = DBD->rpm<PointerArray>(uWorld_ptr + offsets::Levels);
        debug_levelCount = levelsArray.count;
        if (!is_valid(levelsArray.data) || levelsArray.count <= 0 || levelsArray.count > 10) // Validação do array de levels
            continue;


        for (int i = 0; i < levelsArray.count; ++i) {
            uintptr_t levelAddr = levelsArray.data + i * sizeof(uintptr_t);
            if (!is_valid(levelAddr)) continue; // Valida endereço do ponteiro do level
            uintptr_t level = DBD->rpm<uintptr_t>(levelAddr);
            if (!is_valid(level)) continue;

            PointerArray actorArrayData = DBD->rpm<PointerArray>(level + offsets::ActorArray);

            if (!is_valid(actorArrayData.data) || actorArrayData.count <= 0 || actorArrayData.count > 2048) // Validação do array de atores
                continue;


            for (int j = 0; j < actorArrayData.count; ++j) {
                uintptr_t actorAddr = actorArrayData.data + j * sizeof(uintptr_t);
                if (!is_valid(actorAddr)) continue; // Valida endereço do ponteiro do ator
                uintptr_t actor = DBD->rpm<uintptr_t>(actorAddr);
                if (!is_valid(actor)) continue;
                debug_totalActorsFound++;

                int actorId = DBD->rpm<int>(actor + offsets::ActorID);
                if (actorId <= 0) continue; // Valida ID
                std::string actorName = GetNameById(actorId);
                if (actorName == "NULL" || actorName.empty()) continue;

                Cache::EActorType foundType = Cache::EActorType::All;
                std::string displayName = actorName;

                for (const auto& pair : g_ActorLookupVector) {
                    if (actorName.find(pair.first) != std::string::npos) {
                        foundType = pair.second.first;
                        displayName = pair.second.second;
                        debug_actorsIdentified++;
                        break;
                    }
                }

                uintptr_t outline = FindOutlineComponentForCache(actor);

                size_t index = static_cast<size_t>(foundType);
                if (index < tempCache.size()) {
                    tempCache[index].emplace_back(Cache::CachedObject{ actor, outline, displayName, actorName }); // <-- POR ESTA
                }
            }
        }

        // 1. Verifica se o tempCache está realmente vazio (ou seja, a leitura falhou)
        bool isTempCacheEmpty = true;
        for (const auto& list : tempCache) {
            if (!list.empty()) {
                isTempCacheEmpty = false;
                break;
            }
        }

        // 2. Atualiza o cache principal de forma atômica E SEGURA
        {
            std::lock_guard<std::mutex> lock(Cache::cacheMutex);

            // SÓ ATUALIZA O CACHE SE:
            // 1. O tempCache NÃO ESTIVER VAZIO (leitura normal)
            // 2. OU, o tempCache ESTIVER VAZIO E o localPawn for INVÁLIDO (saímos da partida)
            if (!isTempCacheEmpty || !is_valid(localPawn_ptr))
            {
                Cache::actorCache = std::move(tempCache);
            }
            // SE O tempCache ESTIVER VAZIO, MAS O localPawn AINDA FOR VÁLIDO
            // (erro de leitura no meio da partida), O CACHE ANTIGO SERÁ PRESERVADO.
        }

        size_t survCount = 0;
        size_t killerCount = 0;
        size_t genCount = 0;
        {
            std::lock_guard<std::mutex> lock(Cache::cacheMutex);
            survCount = Cache::GetList(Cache::EActorType::Survivor).size();
            killerCount = Cache::GetList(Cache::EActorType::Killer).size();
            genCount = Cache::GetList(Cache::EActorType::Generator).size();
        }
        {
            std::lock_guard<std::mutex> debugLock(g_GameState.dataMutex);
            g_GameState.debugInfo.processBase = ToHexString(process_base);
            g_GameState.debugInfo.uWorld = ToHexString(uWorld_ptr);
            g_GameState.debugInfo.gameInstance = ToHexString(gameInstance_ptr);
            g_GameState.debugInfo.localPawn = ToHexString(localPawn_ptr);
            g_GameState.debugInfo.isInMatch = is_valid(localPawn_ptr);

            g_GameState.debugInfo.levelCount = debug_levelCount;
            g_GameState.debugInfo.validActorClusters = debug_validActorClusters;
            g_GameState.debugInfo.totalActorsFound = debug_totalActorsFound;
            g_GameState.debugInfo.actorsIdentified = debug_actorsIdentified;
            g_GameState.debugInfo.survivorCount = static_cast<int>(survCount);
            g_GameState.debugInfo.killerCount = static_cast<int>(killerCount);
            g_GameState.debugInfo.generatorCount = static_cast<int>(genCount);
        } // dataMutex é liberado aqui
    }
}