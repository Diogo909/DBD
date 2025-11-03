// keybinds.h
#pragma once
#include <map>
#include <string>
#include "imgui.h"
#include <Windows.h>

// Mapa para converter códigos de tecla (Virtual Keys) em texto legível
inline std::map<int, std::string> keyNames;

inline void InitializeKeyNames() {
    if (keyNames.empty()) {
        keyNames[VK_INSERT] = "Insert";
        keyNames[VK_DELETE] = "Delete";
        keyNames[VK_HOME] = "Home";
        keyNames[VK_END] = "End";
        keyNames[VK_PRIOR] = "Page Up";
        keyNames[VK_NEXT] = "Page Down";
        keyNames[VK_LEFT] = "Left Arrow";
        keyNames[VK_RIGHT] = "Right Arrow";
        keyNames[VK_UP] = "Up Arrow";
        keyNames[VK_DOWN] = "Down Arrow";
        // Adicione outras teclas que você quiser aqui...
        for (int i = 'A'; i <= 'Z'; i++) {
            keyNames[i] = std::string(1, (char)i);
        }
        for (int i = '0'; i <= '9'; i++) {
            keyNames[i] = std::string(1, (char)i);
        }
        for (int i = VK_F1; i <= VK_F12; i++) {
            keyNames[i] = "F" + std::to_string(i - VK_F1 + 1);
        }
    }
}

// Função que cria o botão de keybind interativo
inline void KeyBindButton(const char* label, int* key) {
    InitializeKeyNames(); // Garante que o mapa de nomes foi inicializado

    // Variável estática para saber em qual keybind estamos esperando input
    static int* currently_binding = nullptr;

    bool is_binding_this_one = (currently_binding == key);

    std::string button_text;
    if (is_binding_this_one) {
        button_text = "[ Pressione uma tecla ]";
    }
    else {
        if (keyNames.count(*key)) {
            button_text = keyNames[*key];
        }
        else {
            button_text = "Key " + std::to_string(*key);
        }
    }

    ImGui::Text("%s", label);
    ImGui::SameLine();
    if (ImGui::Button(button_text.c_str(), ImVec2(150, 0))) {
        currently_binding = key;
    }

    // Se estamos esperando por uma tecla para ESTE botão
    if (is_binding_this_one) {
        // Itera por todas as teclas possíveis para ver qual foi pressionada
        for (int i = 1; i < 255; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                // Tecla ESC cancela a alteração
                if (i == VK_ESCAPE) {
                    currently_binding = nullptr;
                    break;
                }
                *key = i;
                currently_binding = nullptr;
                break;
            }
        }
    }
}