#pragma once
#include "global.h"
#include <Windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <d3d9.h>
#include "struct.h"
#include "offset.h"
#include <imgui.h>

namespace Cache {
	enum class EActorType;
}

struct DebugInfo {
	std::string processBase = "N/A";
	std::string uWorld = "N/A";
	std::string gameInstance = "N/A";
	std::string localPawn = "N/A";
	bool isInMatch = false;
	int survivorCount = 0;
	int killerCount = 0;
	int generatorCount = 0;
	float cameraFOV = 0.0f;
	int espFps = 0;
	int logicFps = 0;
	int levelCount = 0;
	int validActorClusters = 0;
	int totalActorsFound = 0;
	int actorsIdentified = 0;
	Rotator cameraRotation;
};

enum class GeneratorTrend {
	Stagnant,
	Increasing,
	Decreasing
};

typedef struct _EntityList
{
	int charge = 0; // progresso do gerador (0~100)
	uintptr_t instance;
	uintptr_t mesh;
	uintptr_t root_component;
	uintptr_t instigator;
	uintptr_t PlayerState;
	uintptr_t Pawn;
	Vector3 TopLocation;
	Vector3 bone_origin;
	EPalletState palletState;
	ETotemState totemState;
	std::string name;
	Cache::EActorType type;
	Vector3 origin;
	bool bIsPinned = false;
	float health;
	float dist;
	int objectId;
	int team;
	int healthState;
	float progress = -1.0f; // valor entre 0.0f e 100.0f
	uintptr_t debugOffset; // <-- ADICIONE ESTA LINHA
	bool isOvercharged = false;
	bool bIsScourgeActive;
	uintptr_t outlineComp;
	GeneratorTrend trend;
	// === ADICIONE ESTA LINHA ===
	float progressRate = 0.0f; // Para a estimativa de tempo
	float stableProgressRate = 0.0f;
	bool bIsUnbreakableActive = false;
	float dsTimer = 0.0f;
	bool bIsDsActive = false; // <--- Novo flag para indicar se o timer está rodando
	bool bHasDsBeenAttempted = false; // <--- Novo flag para DS GASTO
	// --- NOVOS MEMBROS ---
	std::vector<std::string> perks;
	std::vector<std::string> addons;
}EntityList;

// ========================================================================
// FUNÇÕES HELPER BÁSICAS
// ========================================================================

inline bool is_valid(uintptr_t address) {
	// Limites mais restritivos podem ser mais seguros, dependendo da arquitetura
	// return (address >= 0x10000 && address < 0x000F000000000000); // Exemplo x64
	return (address > 0x10000 && address < 0x7FFFFFFFFFFF); // Mantendo o seu limite
}

inline std::string ToHexString(uintptr_t ptr) {
	std::stringstream stream;
	stream << "0x" << std::hex << ptr;
	return stream.str();
}

inline float ToMeters(float unrealUnits) {
	// A conversão padrão é 1 unidade = 1 cm
	return unrealUnits / 100.0f; // Converte cm para metros
}

// ========================================================================
// FUNÇÕES DE INTERAÇÃO E LEITURA DE MEMÓRIA ESPECÍFICAS
// ========================================================================

inline void sendSpaceCommand() {
	INPUT inputs[2] = {};
	ZeroMemory(inputs, sizeof(inputs));

	// Pressiona a tecla espaço
	inputs[0].type = INPUT_KEYBOARD;
	inputs[0].ki.wVk = VK_SPACE;

	// Solta a tecla espaço
	inputs[1].type = INPUT_KEYBOARD;
	inputs[1].ki.wVk = VK_SPACE;
	inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

inline std::string ReadFString(uintptr_t address)
{
	if (!DBD || !is_valid(address)) return ""; // Validação inicial

	wchar_t buffer[128] = { 0 }; // Buffer maior pode ser mais seguro
	uintptr_t string_ptr = DBD->rpm<uintptr_t>(address);
	int string_len = DBD->rpm<int>(address + 0x8); // Offset padrão para TArray Count

	// Validações mais robustas
	if (string_len > 0 && string_len < 128 && is_valid(string_ptr)) {
		// Tenta ler a memória
		if (!DBD->ReadRaw(string_ptr, buffer, string_len * sizeof(wchar_t))) {
			return ""; // Falha na leitura
		}

		// --- Conversão Segura ---
		std::wstring wstr(buffer, string_len);
		if (wstr.empty()) return "";

		int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
		if (size_needed <= 0) return ""; // Erro na conversão

		std::string strTo(size_needed, 0);
		int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

		if (result > 0) {
			return strTo;
		}
	}
	return "";
}

// ========================================================================
// FUNÇÕES DE MATEMÁTICA E W2S
// ========================================================================

inline D3DMATRIX MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2)
{
	D3DMATRIX pOut;
	pOut._11 = pM1._11 * pM2._11 + pM1._12 * pM2._21 + pM1._13 * pM2._31 + pM1._14 * pM2._41;
	pOut._12 = pM1._11 * pM2._12 + pM1._12 * pM2._22 + pM1._13 * pM2._32 + pM1._14 * pM2._42;
	pOut._13 = pM1._11 * pM2._13 + pM1._12 * pM2._23 + pM1._13 * pM2._33 + pM1._14 * pM2._43;
	pOut._14 = pM1._11 * pM2._14 + pM1._12 * pM2._24 + pM1._13 * pM2._34 + pM1._14 * pM2._44;
	pOut._21 = pM1._21 * pM2._11 + pM1._22 * pM2._21 + pM1._23 * pM2._31 + pM1._24 * pM2._41;
	pOut._22 = pM1._21 * pM2._12 + pM1._22 * pM2._22 + pM1._23 * pM2._32 + pM1._24 * pM2._42;
	pOut._23 = pM1._21 * pM2._13 + pM1._22 * pM2._23 + pM1._23 * pM2._33 + pM1._24 * pM2._43;
	pOut._24 = pM1._21 * pM2._14 + pM1._22 * pM2._24 + pM1._23 * pM2._34 + pM1._24 * pM2._44;
	pOut._31 = pM1._31 * pM2._11 + pM1._32 * pM2._21 + pM1._33 * pM2._31 + pM1._34 * pM2._41;
	pOut._32 = pM1._31 * pM2._12 + pM1._32 * pM2._22 + pM1._33 * pM2._32 + pM1._34 * pM2._42;
	pOut._33 = pM1._31 * pM2._13 + pM1._32 * pM2._23 + pM1._33 * pM2._33 + pM1._34 * pM2._43;
	pOut._34 = pM1._31 * pM2._14 + pM1._32 * pM2._24 + pM1._33 * pM2._34 + pM1._34 * pM2._44;
	pOut._41 = pM1._41 * pM2._11 + pM1._42 * pM2._21 + pM1._43 * pM2._31 + pM1._44 * pM2._41;
	pOut._42 = pM1._41 * pM2._12 + pM1._42 * pM2._22 + pM1._43 * pM2._32 + pM1._44 * pM2._42;
	pOut._43 = pM1._41 * pM2._13 + pM1._42 * pM2._23 + pM1._43 * pM2._33 + pM1._44 * pM2._43;
	pOut._44 = pM1._41 * pM2._14 + pM1._42 * pM2._24 + pM1._43 * pM2._34 + pM1._44 * pM2._44;

	return pOut;
}

struct FTransform
{
	Vector4 rot;
	Vector3 translation;
	char pad[4];
	Vector3 scale;
	char pad1[4];

	D3DMATRIX ToMatrixWithScale()
	{
		D3DMATRIX m;
		m._41 = translation.x;
		m._42 = translation.y;
		m._43 = translation.z;

		float x2 = rot.x + rot.x;
		float y2 = rot.y + rot.y;
		float z2 = rot.z + rot.z;

		float xx2 = rot.x * x2;
		float yy2 = rot.y * y2;
		float zz2 = rot.z * z2;
		m._11 = (1.0f - (yy2 + zz2)) * scale.x;
		m._22 = (1.0f - (xx2 + zz2)) * scale.y;
		m._33 = (1.0f - (xx2 + yy2)) * scale.z;

		float yz2 = rot.y * z2;
		float wx2 = rot.w * x2;
		m._32 = (yz2 - wx2) * scale.z;
		m._23 = (yz2 + wx2) * scale.y;

		float xy2 = rot.x * y2;
		float wz2 = rot.w * z2;
		m._21 = (xy2 - wz2) * scale.y;
		m._12 = (xy2 + wz2) * scale.x;

		float xz2 = rot.x * z2;
		float wy2 = rot.w * y2;
		m._31 = (xz2 + wy2) * scale.z;
		m._13 = (xz2 - wy2) * scale.x;

		m._14 = 0.0f;
		m._24 = 0.0f;
		m._34 = 0.0f;
		m._44 = 1.0f;

		return m;
	}
};

inline D3DMATRIX Matrix(Rotator rotation, Vector3 origin = Vector3(0, 0, 0)) {
	float radPitch = (rotation.Pitch * M_PI / 180.f);
	float radYaw = (rotation.Yaw * M_PI / 180.f);
	float radRoll = (rotation.Roll * M_PI / 180.f);
	float SP = sinf(radPitch);
	float CP = cosf(radPitch);
	float SY = sinf(radYaw);
	float CY = cosf(radYaw);
	float SR = sinf(radRoll);
	float CR = cosf(radRoll);
	D3DMATRIX matrix;
	matrix._11 = CP * CY;
	matrix._12 = CP * SY;
	matrix._13 = SP;
	matrix._14 = 0.f;
	matrix._21 = SR * SP * CY - CR * SY;
	matrix._22 = SR * SP * SY + CR * CY;
	matrix._23 = -SR * CP;
	matrix._24 = 0.f;
	matrix._31 = -(CR * SP * CY + SR * SY);
	matrix._32 = CY * SR - CR * SP * SY;
	matrix._33 = CR * CP;
	matrix._34 = 0.f;
	matrix._41 = origin.x;
	matrix._42 = origin.y;
	matrix._43 = origin.z;
	matrix._44 = 1.f;
	return matrix;
}

inline Vector3 WorldToScreen(FMinimalViewInfo camera, Vector3 WorldLocation)
{
	Vector3 Screenlocation = Vector3(0, 0, 0);
	D3DMATRIX tempMatrix = Matrix(camera.Rotation);

	Vector3 vAxisX, vAxisY, vAxisZ;
	vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
	vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
	vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

	Vector3 vDelta = WorldLocation - camera.Location;
	Vector3 vTransformed = Vector3(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

	if (vTransformed.z < 1.f)
		vTransformed.z = 1.f;

	float FovAngle = camera.FOV;
	float ScreenCenterX = ImGui::GetIO().DisplaySize.x / 2.0f;
	float ScreenCenterY = ImGui::GetIO().DisplaySize.y / 2.0f;

	Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(FovAngle * M_PI / 360.f)) / vTransformed.z;
	Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(FovAngle * M_PI / 360.f)) / vTransformed.z;

	return Screenlocation;
}


namespace FColor {
	constexpr auto B = 0x0; // char
	constexpr auto G = 0x1; // char
	constexpr auto R = 0x2; // char
	constexpr auto A = 0x3; // char
}


struct SurvivorOutlineInfo {
	std::string name;
	float interpolationSpeed;
	float r, g, b, a;
};

class UPlayer {
private:
public:
	uintptr_t instance;
	uintptr_t mesh;
	uintptr_t root_component;
	uintptr_t OutlineComponent;
	uintptr_t instigator;
	uintptr_t PlayerState;
	uintptr_t Pawn;
	Vector3 TopLocation;
	std::string name;
	int objectId;
	Vector3 origin;
	float health;
	float dist;
	int team;
};

