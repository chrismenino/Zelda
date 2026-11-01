#include <assert.h>
#include "src/ray_lighting.c"
uint8 g_ram[0x20000];
uint8_t g_rt_ui_mask[240][448];
uint32_t g_rt_scene_rgb[240][448];
static int wall_enabled;
uint8 Overworld_GetTileAttributeAtLocation(uint16 x,uint16 y) { return wall_enabled && x>=6 && x<7 ? 1 : 0; }
int main(void) {
  RayLighting_Init();
  gridw=65;gridh=57;
  Light l={24,80,128,1,1,1};
  memset(solid,0,sizeof(solid));memset(rgb,0,sizeof(rgb));
  TraceLight(&l);float open=rgb[20][24][0];
  memset(rgb,0,sizeof(rgb));
  wall_enabled=1;geometry_signature++;
  TraceLight(&l);float shadow=rgb[20][24][0];
  assert(open>0.02f && shadow<open*0.1f);
  assert(rgb[20][10][0]>0.1f);
  unsigned built=cache_builds;TraceLight(&l);assert(cache_builds==built);
  world_scroll_x=16;l.x-=16;TraceLight(&l);assert(cache_builds==built);
  geometry_signature++;TraceLight(&l);assert(cache_builds==built+1);
  world_scroll_x=0;wall_enabled=0;geometry_signature++;
  static uint32 pixels[256*224],original[256*224];
  static uint8 ramcopy[0x20000];
  for(int i=0;i<256*224;i++)pixels[i]=original[i]=0xff808080;
  main_module_index=9;submodule_index=0;INIDISP_copy=15;
  link_x_coord=80;link_y_coord=80;savegame_is_darkworld=64;
  memcpy(ramcopy,g_ram,sizeof(g_ram));
  enabled=0;RayLighting_Apply((uint8*)pixels,1024,256,224,1);
  assert(!memcmp(pixels,original,sizeof(pixels)));
  enabled=1;RayLighting_Apply((uint8*)pixels,1024,256,224,1);
  assert(memcmp(pixels,original,sizeof(pixels)));
  assert(!memcmp(ramcopy,g_ram,sizeof(g_ram)));
  enabled=1;cinematic=0;RayLighting_CycleStyle();RayLighting_Toggle();
  enabled=1;cinematic=0;RayLighting_Init();assert(enabled==0 && cinematic==1);
  // Regression: game resets dung_index_of_torches to zero after loading.
  player_is_indoors=1;dungeon_room_index=999;link_visibility_status=1;
  memset(dung_bg2_attr_table,1,8192);dung_bg2_attr_table[0]=0xc0;
  dung_index_of_torches_start=0;dung_index_of_torches=0;
  dung_object_tilemap_pos[0]=0x8000|0x510;
  GatherLights(256,224,0);assert(num_flames==1);
  dung_object_tilemap_pos[0]&=0x7fff;
  GatherLights(256,224,0);assert(num_flames==0);
  // Permanent wall fixtures are independent of the dynamic torch counter.
  dungeon_room_index=119;GatherLights(256,224,0);assert(num_flames>0);
  remove("ray-lighting.cfg");
  puts("PASS: wall occlusion; lit near face; disabled pixel identity; enabled pixel changes; gameplay RAM unchanged; settings persistence; reset-counter torch regression; extinguished torch regression; permanent fixture detection; cache reuse across camera movement; cache invalidation.");
}
