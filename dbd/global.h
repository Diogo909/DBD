// global.h (VERSÃO MELHORADA E DEFINITIVA)
#pragma once

//#define _DEV_NO_AUTH

#include "driver.h"
#include <cstdint>

class GameState;
struct ImFont; // Forward declaration continua necessária

extern DRV* DBD;
extern uint64_t process_base;
extern uint32_t process_id;
extern bool rendering;

// DECLARA que as variáveis existem em algum lugar
extern ImFont* g_espFontName;
extern ImFont* g_espFontStatus;

extern GameState g_GameState;