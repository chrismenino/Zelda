#include "zelda_rtl.h"
#include "variables.h"
#include "misc.h"
#include "messaging.h"
#include "load_gfx.h"
#include "dark_prologue.h"
#include <string.h>

// Reserved extension bytes within the per-slot 0x500-byte saved block.
#define DWA_MAGIC g_ram[0xf4e0]
#define DWA_STAGE g_ram[0xf4e1]
#define DWA_TAG 0xd7
#define DWA_DONE 6
#define DWA_MSG 397
#define DWA_ROUTE g_ram[0xf4e2]
#define DWA_BRAZIERS g_ram[0xf4e3]
#define DWA_GATE g_ram[0xf4e4]

void DarkPrologue_OnLoad(void) {
  if (DWA_MAGIC != DWA_TAG) {
    // Do not rewrite existing adventures imported by the player.
    if (sram_progress_indicator != 0 || link_sword_type != 0) return;
    DWA_MAGIC = DWA_TAG;
    DWA_STAGE = 0;
    DWA_ROUTE = 0;
    DWA_BRAZIERS = 0;
    DWA_GATE = 0;
  }
  if (DWA_STAGE >= DWA_DONE) return;
  sram_progress_indicator = 2;
  savegame_is_darkworld = 0x40;
  player_is_indoors = 0;
  link_item_moon_pearl = 1;
  link_health_capacity = link_health_current = 48;
  link_sword_type = DWA_STAGE >= 2 ? 1 : 0;
  link_shield_type = DWA_STAGE >= 2 ? 1 : 0;
  follower_indicator = 0;
  link_is_bunny = 0;
}

static void Tell(int message, int stage) {
  DWA_STAGE = stage;
  SaveGameFile();
  dialogue_message_index = DWA_MSG + message;
  Main_ShowTextMessage();
}

static void ReturnHome(void) {
  uint8 name[12];
  memcpy(name, g_ram + 0xf3d9, sizeof(name));
  memset(save_dung_info, 0, 0x500);
  memcpy(g_ram + 0xf3d9, name, sizeof(name));
  WORD(g_ram[0xf3e5]) = 0x55aa;
  WORD(g_ram[0xf20c]) = WORD(g_ram[0xf20e]) = 0xf000;
  death_var2 = 0xffff;
  link_health_capacity = link_health_current = 24;
  link_ability_flags = 0xf8;
  DWA_MAGIC = DWA_TAG;
  DWA_STAGE = DWA_DONE;
  SaveGameFile();
  // Use the regular save-load path to restore the original house opening.
  WORD(g_ram[0]) = ((srm_var1 >> 1) - 1) * 0x500;
  CopySaveToWRAM();
  player_is_indoors = 0;
  death_var4 = death_var5 = 0;
  mosaic_level = 0;
}

void DarkPrologue_Tick(void) {
  if (DWA_MAGIC != DWA_TAG || DWA_STAGE >= DWA_DONE) return;
  if (main_module_index == 7 && submodule_index == 0) {
    // The prologue has no indoor objectives; return safely from pyramid holes.
    player_is_indoors = 0;
    main_module_index = 5;
    return;
  }
  if (main_module_index != 9 || submodule_index != 0) return;
  // The end is now player-initiated at the summit gate, not automatic.
  if (DWA_STAGE == 5 && DWA_GATE == 1) { ReturnHome(); return; }
  // Constrain the opening to the pyramid. Leaving reloads its safe summit.
  if ((overworld_area_index & 0x3f) != 0x1b || player_is_indoors) {
    main_module_index = 5;
    return;
  }
  unsigned y = link_y_coord - 0x600;
  // Track Ashen Pass milestones as persistent progress in the save slot.
  uint8 route = y >= 440 ? 3 : y >= 300 ? 2 : y >= 180 ? 1 : 0;
  if (route > DWA_ROUTE) {
    DWA_ROUTE = route;
    SaveGameFile();
  }
  if (DWA_STAGE == 0) { Tell(0, 1); return; }
  if (DWA_STAGE == 1 && y >= 180) {
    link_sword_type = link_shield_type = 1;
    DecompressSwordGraphics();
    DecompressShieldGraphics();
    Tell(1, 2);
  } else if (DWA_STAGE == 2 && y >= 300) {
    link_health_current = link_health_capacity;
    Tell(2, 3);
  } else if (DWA_STAGE == 3 && y >= 440) {
    Tell(3, 4);
  } else if (DWA_STAGE == 4 && y >= 420 && (filtered_joypad_L & 0x80)) {
    // Gate opens only on player interaction after the mountain approach.
    DWA_GATE = 1;
    Tell(4, 5);
  }
}
