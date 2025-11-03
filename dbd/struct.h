#pragma once
#include <cstdint>
#include <cmath>
#include <string>      // Para std::string e std::wstring

template<typename T>
struct TArray {
	uintptr_t Data;
	int32_t Count;  // Quantidade atual
	int32_t Max;    // Capacidade máxima

	// Métodos de acesso simples
	int size() const { return Count; }
	bool is_valid_index(int index) const { return index >= 0 && index < Count; }
	T& operator[](int index) { return Data[index]; }
	const T& operator[](int index) const { return Data[index]; }
};

struct FString : public TArray<wchar_t> {
	std::wstring ToWString() const {
		if (!Data) return L"";
		return std::wstring(Data, Count);
	}

	std::string ToString() const {
		std::wstring wstr = ToWString();
		return std::string(wstr.begin(), wstr.end());
	}
};

// Estrutura FName, usada pelo Unreal para referenciar nomes
struct FName {
	int32_t ComparisonIndex;  // Índice usado internamente para comparação
	int32_t DisplayIndex;     // Índice usado para exibição (GetNameById)
	int32_t Number;           // Suporte para nomes duplicados (Ex: Name_01)

	// Getter opcional para facilitar leitura
	int32_t Index() const { return DisplayIndex; }
};

struct FCharacterStateData
{
	int                                     pips;                                             // 0x0000(0x0004)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPrivate)
	class FName                             powerId;                                          // 0x0004(0x000C)(ZeroConstructor, IsPlainOldData, NoDestructor, HasGetValueTypeHash, NativeAccessSpecifierPrivate)
	TArray<class FName>                     addonIds;                                         // 0x0010(0x0010)(ZeroConstructor, NativeAccessSpecifierPrivate)
};

struct FPlayerStateData
{
	int CharacterLevel;                       // 0x0000
	FName EquipedFavorId;                     // 0x0004
	TArray<FName> EquipedPerkIds;             // 0x0010
	TArray<int> EquipedPerkLevels;            // 0x0020
	FString EquippedBannerId;                 // 0x0030
	FString EquippedBadgeId;                  // 0x0040
	FName EquippedCharacterClass;             // 0x0050
};

// Enum para os tipos de skill check
enum class ESkillCheckCustomType : uint8_t {
	VE_None = 0,
	VE_OnPickedUp = 1,
	VE_OnAttacked = 2,
	VE_DecisiveStrikeWhileWiggling = 3,
	VE_GeneratorOvercharge1 = 4,
	VE_GeneratorOvercharge2 = 5,
	VE_GeneratorOvercharge3 = 6,
	VE_BrandNewPart = 7,
	VE_Struggle = 8,
	VE_OppressionPerkGeneratorKicked = 9,
	VE_SoulChemical = 10,
	VE_Wiggle = 11,
	VE_YellowGlyph = 12,
	VE_K27P03Continuous = 13,
	VE_Continuous = 14,
	VE_S42P02 = 15,
	VE_K38P03Continuous = 16,
	VE_SnapOutOfIt = 17
};

enum class EHealthState : uint8_t {
	VE_Healthy = 0,
	VE_Injured = 1,
	VE_KO = 2,
	VE_Dead = 3,
	VE_MAX = 4,
};

enum class ETotemState : uint8_t
{
	Cleansed = 0,
	Dull = 1,
	Hex = 2,
	Boon = 3
};

enum class EPalletState : uint8_t
{
	Up = 0,
	Falling = 1,
	Fallen = 2,
	Destroyed = 3
};


struct FSkillCheckDefinition {
	float SuccessZoneStart;        // 0x0000
	float SuccessZoneEnd;          // 0x0004
	float BonusZoneLength;         // 0x0008
	float BonusZoneStart;          // 0x000C
	float ProgressRate;            // 0x0010
	float StartingTickerPosition;  // 0x0014
	bool IsDeactivatedAfterResponse; // 0x0018
	char pad_0019[3];              // 0x0019 -> Padding para alinhar o próximo float
	float WarningSoundDelay;       // 0x001C
	bool IsAudioMuted;             // 0x0020
	bool IsJittering;              // 0x0021
	bool IsOffCenter;              // 0x0022
	bool IsSuccessZoneMirrorred;   // 0x0023
	bool IsInsane;                 // 0x0024
	bool IsLocallyPredicted;       // 0x0025
};

struct vec2
{
	float x, y;
};

/*inline float ToMeters(float x)
{
	return x / 39.62f;
}*/

struct Rotator {
	double Pitch, Yaw, Roll;
};

struct Vector3 {
	double x, y, z;

	Vector3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}

	Vector3 operator-(Vector3 ape) { return { x - ape.x, y - ape.y, z - ape.z }; }
	Vector3 operator+(Vector3 ape) { return { x + ape.x, y + ape.y, z + ape.z }; }
	Vector3 operator*(double ape) { return { x * ape, y * ape, z * ape }; }
	Vector3 operator/(double ape) { return { x / ape, y / ape, z / ape }; }
	Vector3& operator/=(double ape) { x /= ape; y /= ape; z /= ape; return *this; }
	Vector3& operator+=(Vector3 ape) { x += ape.x; y += ape.y; z += ape.z; return *this; }
	Vector3& operator-=(Vector3 ape) { x -= ape.x; y -= ape.y; z -= ape.z; return *this; }
	double Length() { return sqrt((x * x) + (y * y) + (z * z)); }
	double Length2D() { return sqrt((x * x) + (y * y)); }
	double DistTo(Vector3 ape) { return (*this - ape).Length(); }
	double Dist2D(Vector3 ape) { return (*this - ape).Length2D(); }
	double Dot(Vector3& v) { return x * v.x + y * v.y + z * v.z; }
};

struct Vector4 {
	double w, x, y, z;
};

struct FMinimalViewInfo
{
	Vector3 Location;
	Rotator Rotation;
	// A linha de padding foi removida
	float FOV;
};

struct FCameraCacheEntry
{
	float TimeStamp;
	char pad_001[0xC];
	FMinimalViewInfo POV;
};

// --- NOVAS DEFINIÇÕES ADICIONADAS AQUI ---

// Estrutura para representar a TArray do Unreal Engine
struct PointerArray {
	uintptr_t data;
	int32_t count;
	int32_t max;
};