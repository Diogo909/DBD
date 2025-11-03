#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace offsets
{
    // =======================================================
    // == Ponteiros Globais (GWorld / GNames) ==
    // =======================================================
    extern uintptr_t GWorld; // UWorld* (Ponteiro global para o mundo)
    extern uintptr_t GNames; // FNamePool* (Ponteiro global para a tabela de nomes)

    // =======================================================
    // == SSL Bypass & Unlock All ==
    // =======================================================
    extern uintptr_t SSLBypass; // steam = 0xB35E5D0 - 9.2.3
    extern uintptr_t GamePatch;

    // =======================================================
    // == Cadeia de Ponteiros (Mundo -> Jogador Local) ==
    // =======================================================
    constexpr uintptr_t OwningGameInstance = 0x1e8;    // UWorld -> OwningGameInstance (UGameInstance*)
    constexpr uintptr_t LocalPlayers = 0x40;        // UGameInstance -> LocalPlayers (TArray<ULocalPlayer*>)
    constexpr uintptr_t PlayerController = 0x38;    // ULocalPlayer -> PlayerController (APlayerController*)
    constexpr uintptr_t AcknowledgedPawn = 0x360;   // APlayerController -> AcknowledgedPawn (APawn* / ADBDPlayer*)
    constexpr uintptr_t PlayerCameraManager = 0x370; // APlayerController -> PlayerCameraManager (APlayerCameraManager*)
    constexpr uintptr_t PlayerState = 0x2d0;        // APawn -> PlayerState (APlayerState*)
    constexpr uintptr_t PlayerStateLocalPlayer = 0x2b8; // APlayerController -> PlayerState (APlayerState*)
    constexpr uintptr_t RootComponent = 0x1B0;      // AActor -> RootComponent (USceneComponent*)
    constexpr uintptr_t RelativeLocation = 0x148; // USceneComponent -> RelativeLocation (FVector)

    // =======================================================
    // == Estruturas do Mundo e Níveis (World / Level) ==
    // =======================================================
    constexpr uintptr_t PersistentLevel = 0x38;     // UWorld -> PersistentLevel (ULevel*)
    constexpr uintptr_t Levels = 0x188;       // UWorld -> Levels (TArray<ULevel*>)
    constexpr uintptr_t ActorArray = 0xA8;        // ULevel -> ActorArray (TArray<AActor*>)
    constexpr uintptr_t ActorID = 0x18;         // AActor -> ActorID (int32 / FName)
    constexpr uintptr_t GameState = 0x170;        // UWorld -> GameState (AGameStateBase*)
    constexpr uintptr_t ElapsedTime = 0x320;        // AGameState -> ElapsedTime (int32)
    constexpr uintptr_t ServerWorldTimeSeconds = 0x2dc; // AGameStateBase -> ReplicatedWorldTimeSeconds (float) [OBSOLETO, use ElapsedTime]
    constexpr uintptr_t PlayerArray = 0x2c8;        // AGameStateBase -> PlayerArray (TArray<APlayerState*>)
    constexpr uintptr_t BlueprintCreatedComponents = 0x298; // AActor -> BlueprintCreatedComponents (TArray<UActorComponent*>)
    constexpr bool      levelReadyToPlay = 0x6eb; // ULevel -> bReadyToPlay (bool)

    // =======================================================
    // == Câmera e Interação ==
    // =======================================================
    constexpr uintptr_t CameraCachePrivate = 0x13c0; // APlayerCameraManager -> CameraCachePrivate (FCameraCacheEntry)
    constexpr uintptr_t POV = 0x30;               // FCameraCacheEntry -> POV (FMinimalViewInfo)
    constexpr uintptr_t FOV = 0x30;               // FMinimalViewInfo -> FOV (float)
    constexpr uintptr_t DefaultFOV = 0x2bc;     // APlayerCameraManager -> DefaultFOV (float)
    constexpr uintptr_t InteractionHandler = 0xc10; // ADBDPlayer -> _interactionHandler (UPlayerInteractionHandler*)
    constexpr uintptr_t SkillCheck = 0x330;       // UPlayerInteractionHandler -> _skillCheck (USkillCheck*)

    // =======================================================
    // == Estado do Jogador (PlayerState) ==
    // =======================================================
    constexpr uintptr_t PlayerNamePrivate = 0x350; // APlayerState -> PlayerNamePrivate (FString)
    constexpr uintptr_t GameRole = 0x39a;          // ADBDPlayerState -> GameRole (EPlayerRole)
    constexpr uintptr_t CamperData = 0x3b0;        // ADBDPlayerState -> CamperData (FCharacterStateData)
    constexpr uintptr_t SlasherData = 0x3d0;       // ADBDPlayerState -> SlasherData (FCharacterStateData)
    constexpr uintptr_t PlayerData = 0x3f0;        // ADBDPlayerState -> PlayerData (FPlayerStateData)
    constexpr uintptr_t SelectedSurvivorIndex = 0x5e8; // ADBDPlayerState -> _selectedSurvivorIndex (int32)
    constexpr uintptr_t SelectedKillerIndex = 0x5ec; // ADBDPlayerState -> _selectedKillerIndex (int32)

    // =======================================================
    // == Componentes de Atores Específicos ==
    // =======================================================
    // --- Saúde (Health) ---
    constexpr uintptr_t HealthComponent = 0x1900; // ACamperPlayer -> _healthComponent (UHealthComponent*)
    constexpr uintptr_t HealthState = 0x268;      // UHealthComponent -> _healthState (EHealthState)

    // --- Gerador (Generator) ---
    constexpr uintptr_t _generatorCharge = 0x670; // AGenerator -> (Offset antigo/não usado)
    constexpr uintptr_t _realCharge = 0x6AC;      // AGenerator -> _realCharge (float - 0.0 a 1.0)
    constexpr uintptr_t _activated = 0x6A8;       // AGenerator -> _activated (bool)
    constexpr uintptr_t _isAutoCompleted = 0x698; // AGenerator -> _isAutoCompleted (bool)

    // --- Pallet ---
    constexpr uintptr_t PalletState = 0x3F0;      // APallet -> _palletState (EPalletState)

    // --- Totem ---
    constexpr uintptr_t TotemState = 0x408;       // ATotem -> _totemState (ETotemState)

    // --- Armadilha (Trap) ---
    constexpr uintptr_t _isTrapSet = 0x560;       // ABearTrap -> _isTrapSet (bool)

    // --- Gancho (Hook) ---
    constexpr uintptr_t SpawnedScourgeHookNiagara = 0x918; // ABP_SmallMeatLocker_C -> SpawnedScourgeHookNiagara (UNiagaraComponent*)
    constexpr uintptr_t bRenderingEnabled = 0x668;       // UNiagaraComponent -> bRenderingEnabled (uint8)

    // =======================================================
    // == Perks & Status Effects (Usados em esp.h) ==
    // =======================================================
    // --- UPerk (Classe Base) ---
    constexpr uintptr_t _isUsable = 0x440;        // UPerk -> _isUsable (bool)

    // --- UPerkManager ---
    constexpr uintptr_t PerkManager = 0xC18;      // ADBDPlayer -> PerkManager (UPerkManager*)
    constexpr uintptr_t _perks = 0xc8;          // UPerkManager -> _perks (UPerkCollectionComponent*)
    constexpr uintptr_t _statusEffects = 0xd0;  // UPerkManager -> _statusEffects (UStatusEffectCollectionComponent*)

    // --- UPerkCollectionComponent / UStatusEffectCollectionComponent ---
    constexpr uintptr_t _array = 0xb8;          // UPerkCollectionComponent -> _array (TArray<UPerk*>)

    // --- UDecisiveStrike ---
    constexpr uintptr_t _timeAfterUnhook = 0x460; // UDecisiveStrike -> _timeAfterUnhook (float - Duração total)
    constexpr uintptr_t _unhookTimestamp = 0x4B4;

    // --- UGameplayModifierContainer (Pai do UStatusEffect) ---
    constexpr uintptr_t _activationTimer = 0x238; // UGameplayModifierContainer -> _activationTimer (UTimerObject*)

    // --- UTimerObject ---
    constexpr uintptr_t _replicationData_Timestamp = 0x120; // UTimerObject -> _replicationData.Timestamp (float)
    constexpr uintptr_t _replicationData_Duration = 0x124;  // UTimerObject -> _replicationData.Duration (float)

    // =======================================================
    // == Offsets de Outline (Antigos/Hardcoded) ==
    // =NOTE: Mantidos como solicitado, mas `cache.h` usa um método dinâmico.==
    // =======================================================
    constexpr uintptr_t SurvivorDBDOutline = 0x1D68;
    constexpr uintptr_t KillerDBDOutline = 0x1D10;
    constexpr uintptr_t PalletDBDOutline = 0x05E8;
    constexpr uintptr_t GeneratorDBDOutline = 0x08C0;
    constexpr uintptr_t WindowDBDOutline = 0x04A8;
    constexpr uintptr_t MeatHookDBDOutline = 0x07B8;

    // =======================================================
    // == Offsets DENTRO do UDBDOutlineComponent ==
    // =======================================================
    namespace OutlineComponent {
        inline constexpr uintptr_t InterpolationSpeed = 0x2E0; // UDBDOutlineComponent -> InterpolationSpeed (float)
        inline constexpr uintptr_t ColorR = 0x344;            // UDBDOutlineComponent -> OutlineColor.R (float)
        inline constexpr uintptr_t ColorG = 0x348;            // UDBDOutlineComponent -> OutlineColor.G (float)
        inline constexpr uintptr_t ColorB = 0x34C;            // UDBDOutlineComponent -> OutlineColor.B (float)
        inline constexpr uintptr_t ColorA = 0x350;            // UDBDOutlineComponent -> OutlineColor.A (float)
    }

    // (Não utilizado pelo código, mas mantido)
    struct DBDKillers
    {
        static const std::map<int, std::wstring> Data;
    };
}