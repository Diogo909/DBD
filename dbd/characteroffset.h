#pragma once
#include <map>
#include <string>

// Estrutura para armazenar os nomes dos Killers
struct DBDKillers
{
    // Apenas a declaração da variável estática
    static const std::map<int, std::string> Data;
};

// Estrutura para armazenar os nomes dos Sobreviventes
struct DBDSurvivors
{
    // Apenas a declaração da variável estática
    static const std::map<int, std::string> Data;
};