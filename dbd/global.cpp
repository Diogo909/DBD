#include "global.h"
#include "GameState.h"

DRV* DBD = new DRV();

uint64_t process_base = 0;
uint32_t process_id = 0;
bool rendering = true;

GameState g_GameState;