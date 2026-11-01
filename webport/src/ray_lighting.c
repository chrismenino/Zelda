// Flame & Lantern lighting: planar CPU rays against game collision geometry.
// Read-only rendering; all props/lantern effects are visual and preserve saves.
#include "ray_lighting.h"
#include "variables.h"
#include "zelda_rtl.h"
#include "tile_detect.h"
#include "snes/ppu.h"
#include "rt_torch_placements.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define RAYS 512
#define CELL 4
#define MAX_W 448
#define MAX_H 240
#define GW (MAX_W/CELL+1)
#define GH (MAX_H/CELL+1)
#define MAX_LIGHTS 20
#define MAX_FLAMES 48
#define PI_F 3.14159265358979323846f

typedef struct Light { float x,y,radius,r,g,b; } Light;
typedef struct Flame { float x,y; unsigned seed; int prop; } Flame;
static int enabled=1,cinematic=1,ready;
static unsigned visual_tick;
static float dirx[RAYS],diry[RAYS],linear_rgb[256];
static uint8 tone[8193];
static uint8 solid[GH][GW];
static float rgb[GH][GW][3],hit[3][RAYS];
static Light lights[MAX_LIGHTS];static Flame flames[MAX_FLAMES];
static int num_lights,num_flames,gridw,gridh;
#define STAMP 81
#define CENTER 40
#define CACHE_COUNT 64
typedef struct LightCache { unsigned signature,last_use; int wx,wy,radius,valid; float power[STAMP][STAMP]; } LightCache;
static LightCache light_cache[CACHE_COUNT];
static unsigned geometry_signature,cache_builds,cache_hits;
static int world_scroll_x,world_scroll_y;
static unsigned GeometrySignature(void) {
 const uint8 *p=player_is_indoors?dung_bg2_attr_table:(const uint8*)overworld_tileattr;
 unsigned h=2166136261u;
 for(int i=0;i<8192;i++)h=(h^p[i])*16777619u;
 h=(h^(player_is_indoors?dungeon_room_index:overworld_area_index))*16777619u;
 h=(h^player_is_indoors^(link_is_on_lower_level<<8))*16777619u;
 if(!player_is_indoors)h^=overworld_offset_base_x*31u+overworld_offset_base_y*131u+overworld_offset_mask_x*37u+overworld_offset_mask_y*17u;
 return h;
}
static int ui_left,ui_right,ui_top,ui_bottom,has_dialogue;
static float lantern_x,lantern_y;static int has_lantern;
static uint8 *draw_pixels;static int draw_pitch,draw_scale,draw_w,draw_h;
static void SaveSettings(void) { FILE *f=fopen("saves/ray-lighting.cfg","w");if(f){fprintf(f,"%d %d\n",enabled,cinematic);fclose(f);} }
void RayLighting_Init(void) {
 FILE *f=fopen("saves/ray-lighting.cfg","r");if(f){int a,b;if(fscanf(f,"%d %d",&a,&b)==2){enabled=!!a;cinematic=!!b;}fclose(f);}
 for(int i=0;i<RAYS;i++){float a=i*(2*PI_F/RAYS);dirx[i]=cosf(a);diry[i]=sinf(a);}
 for(int i=0;i<256;i++){float c=i/255.0f;linear_rgb[i]=c<=0.04045f?c/12.92f:powf((c+0.055f)/1.055f,2.4f);}
 for(int i=0;i<=8192;i++){float x=i/1024.0f;float v=x/(1+x);float s=v<=0.0031308f?12.92f*v:1.055f*powf(v,1/2.4f)-0.055f;tone[i]=(uint8)(s*255+0.5f);}
 ready=1;
}
void RayLighting_Toggle(void){enabled=!enabled;SaveSettings();}
void RayLighting_CycleStyle(void){cinematic=!cinematic;SaveSettings();}
const char *RayLighting_Status(void){return !enabled?"Original lighting":cinematic?"Flame & Lantern - Deep night":"Flame & Lantern - Brighter night";}
static int IsSolid(uint8 a) {
  if(a==1 || a==2 || a==3 || a==0x26 || a==0x43) return 1;
  if(a>=0x10 && a<=0x13) return 1;
  if(a>=0x18 && a<=0x1b) return 1;
  if(a>=0x50 && a<=0x5d) return 1;
  if(player_is_indoors && (a==4 || a==0xb || a==0x67 || (a>=0x6c && a<=0x7f))) return 1;
  return 0;
}
static uint8 Attribute(int x,int y) {
  if(player_is_indoors) {
    unsigned off=((y & 0x1f8)<<3) + ((x>>3)&63) + (link_is_on_lower_level?0x1000:0);
    return dung_bg2_attr_table[off];
  }
  return Overworld_GetTileAttributeAtLocation(((uint16)x & ~7)>>3, (uint16)y & ~7);
}

static int NearTorch(float x,float y,float distance) {
 for(int i=0;i<num_flames;i++){float dx=x-flames[i].x,dy=y-flames[i].y;if(dx*dx+dy*dy<distance*distance)return 1;}return 0;
}
static void AddLight(float x,float y,float radius,float r,float g,float b,int w,int h) {
 if(num_lights>=MAX_LIGHTS || x<-radius || y<-radius || x>w+radius || y>h+radius)return;
 // Wall fixtures radiate into nearby free space rather than self-occluding.
 int wx=(int)(x+world_scroll_x),wy=(int)(y+world_scroll_y);
 if(IsSolid(Attribute(wx,wy))) {
  int best=10000,bx=0,by=0;
  for(int dy=-7;dy<=7;dy++)for(int dx=-7;dx<=7;dx++){
   if(IsSolid(Attribute(wx+dx*CELL,wy+dy*CELL)))continue;
   int cost=dx*dx+dy*dy;if(cost<best){best=cost;bx=dx;by=dy;}
  }
  if(best<10000){x+=bx*CELL;y+=by*CELL;}
 }
 lights[num_lights++]=(Light){x,y,radius,r,g,b};
}
static void AddFire(float x,float y,int prop,unsigned seed,int w,int h) {
 if(x<-48||x>w+48||y<-48||y>h+48||num_flames>=MAX_FLAMES||NearTorch(x,y,9))return;
 flames[num_flames++]=(Flame){x,y,seed,prop};
 float t=visual_tick*0.085f+seed*1.37f;
 float flicker=0.94f+0.035f*sinf(t)+0.025f*sinf(t*2.31f);
 AddLight(x,y+4,prop==2?133:155,6.8f*flicker,3.6f*flicker,1.05f*flicker,w,h);
}
static int FloorAt(int x,int y){uint8 a=Attribute(x,y);return a==0||a==5||a==6||a==7;}
static void GatherLights(int w,int h,int extra) {
 num_lights=num_flames=0;has_lantern=0;
 // Visible carried lantern replaces the old invisible light at Link's feet.
 if((player_is_indoors||savegame_is_darkworld||sram_progress_indicator<2) && link_visibility_status==0 && !link_is_bunny) {
  int lx=(int16)(link_x_coord-BG2HOFS_copy2)+extra;
  int ly=(int16)(link_y_coord-BG2VOFS_copy2);
  float bob=(link_x_vel||link_y_vel)?sinf(visual_tick*0.28f)*0.8f:0;
  lantern_x=lx+(link_direction_facing==4?-2:18);lantern_y=ly+14+bob;
  has_lantern=1;
  AddFire(lantern_x,lantern_y,2,901,w,h);
 }
 if(player_is_indoors) {
  // Static wall, standing, and permanently lit torches are original map objects.
  for(unsigned i=0;i<sizeof(kIndoorTorches)/sizeof(kIndoorTorches[0]);i++) {
   const TorchPlacement *p=&kIndoorTorches[i];if(p->area!=dungeon_room_index)continue;
   int x=(link_x_coord&~511)+p->x,y=(link_y_coord&~511)+p->y;
   AddFire((int16)(x-BG2HOFS_copy2)+extra,(int16)(y-BG2VOFS_copy2),0,i,w,h);
  }
  // Live puzzle torches: the load counter is reset, so find slots through attrs.
  unsigned seen=0;int base=dung_index_of_torches_start>>1;
  for(int off=0;off<8192;off++) {
   unsigned a=dung_bg2_attr_table[off];if(a<0xc0||a>0xcf)continue;
   int id=a&15;if(seen&(1u<<id))continue;seen|=1u<<id;
   int slot=base+id;if(slot<0||slot>=16)continue;
   unsigned pos=dung_object_tilemap_pos[slot];if(!(pos&0x8000))continue;
   int x=(link_x_coord&~511)+((pos&0x7e)<<2)+8;
   int y=(link_y_coord&~511)+((pos&0x1f80)>>4)+5;
   AddFire((int16)(x-BG2HOFS_copy2)+extra,(int16)(y-BG2VOFS_copy2),0,2000+slot,w,h);
  }
  // Sparse extra floor braziers against walls, anchored to room coordinates.
  // Candidate validation avoids water, pits, doors, and original puzzle torches.
  int added=0;
  for(int y=64;y<480 && added<8;y+=64)for(int x=48;x<480 && added<8;x+=96) {
   int wx=(link_x_coord&~511)+x,wy=(link_y_coord&~511)+y;
   if(!FloorAt(wx,wy)||!FloorAt(wx-4,wy)||!FloorAt(wx+4,wy)||!FloorAt(wx,wy+8))continue;
   if(!IsSolid(Attribute(wx,wy-16))&&!IsSolid(Attribute(wx-16,wy))&&!IsSolid(Attribute(wx+16,wy)))continue;
   int sx=(int16)(wx-BG2HOFS_copy2)+extra,sy=(int16)(wy-BG2VOFS_copy2)-12;
   if(sx<-32||sx>w+32||sy<36||sy>h+32||NearTorch(sx,sy,56))continue;
   int puzzle=0;for(int dy=-24;dy<=24;dy+=8)for(int dx=-24;dx<=24;dx+=8){uint8 a=Attribute(wx+dx,wy+dy);if(a>=0xc0&&a<=0xcf)puzzle=1;}
   if(puzzle)continue;
   AddFire(sx,sy,1,x+y*64+dungeon_room_index,w,h);added++;
  }
 } else {
  for(unsigned i=0;i<sizeof(kOutdoorTorches)/sizeof(kOutdoorTorches[0]);i++) {
   const TorchPlacement *p=&kOutdoorTorches[i];if(p->area!=overworld_area_index)continue;
   int x=p->x,y=p->y;
   if(!FloorAt(x,y)||!FloorAt(x-4,y)||!FloorAt(x+4,y))continue;
   AddFire((int16)(x-BG2HOFS_copy2)+extra,(int16)(y-BG2VOFS_copy2)-12,1,3000+i,w,h);
  }
 }
 for(int i=0;i<16&&num_lights<MAX_LIGHTS;i++) {
  if(!sprite_state[i]||sprite_type[i]!=0xe3)continue;
  int x=(sprite_x_hi[i]<<8)|sprite_x_lo[i],y=(sprite_y_hi[i]<<8)|sprite_y_lo[i];
  AddLight((int16)(x-BG2HOFS_copy2)+extra+8,(int16)(y-BG2VOFS_copy2)+8,66,0.7f,0.25f,0.6f,w,h);
 }
}
static LightCache *CachedLight(Light *l) {
 int wx=(int)floorf(l->x+world_scroll_x+0.5f),wy=(int)floorf(l->y+world_scroll_y+0.5f),radius=(int)l->radius;
 LightCache *c=&light_cache[0];
 for(int i=0;i<CACHE_COUNT;i++){
  LightCache *q=&light_cache[i];
  if(q->valid&&q->signature==geometry_signature&&q->wx==wx&&q->wy==wy&&q->radius==radius){q->last_use=visual_tick;cache_hits++;return q;}
  if(!q->valid||q->last_use<c->last_use)c=q;
 }
 c->valid=1;c->signature=geometry_signature;c->wx=wx;c->wy=wy;c->radius=radius;c->last_use=visual_tick;cache_builds++;
 const float ox[3]={-2.5f,0,2.5f},oy[3]={0,-2,0};
 for(int sample=0;sample<3;sample++)for(int a=0;a<RAYS;a++){
  float distance=l->radius,sx=wx+ox[sample],sy=wy+oy[sample];
  for(float t=5;t<l->radius;t+=2){
   // World-space collision rays survive camera movement. No floorf per step.
   int x=(int)(sx+dirx[a]*t),y=(int)(sy+diry[a]*t);
   if(IsSolid(Attribute(x,y))){distance=t;break;}
  }
  hit[sample][a]=distance;
 }
 for(int y=0;y<STAMP;y++)for(int x=0;x<STAMP;x++){
  float dx=(x-CENTER)*CELL,dy=(y-CENTER)*CELL,d2=dx*dx+dy*dy;
  if(d2>=l->radius*l->radius){c->power[y][x]=0;continue;}
  float d=sqrtf(d2),angle=atan2f(dy,dx)*(RAYS/(2*PI_F));if(angle<0)angle+=RAYS;
  int a=(int)angle;float frac=angle-a,vis=0;
  for(int sample=0;sample<3;sample++){
   float boundary=hit[sample][a]*(1-frac)+hit[sample][(a+1)&(RAYS-1)]*frac;
   float softness=3+fmaxf(0,d-boundary)*0.05f,v=(boundary+5-d)/softness;
   if(v<0)v=0;if(v>1)v=1;vis+=v/3;
  }
  float cut=1-d2/(l->radius*l->radius);c->power[y][x]=cut*cut*vis/(1+d2/900.0f);
 }
 return c;
}
static void TraceLight(Light *l) {
 LightCache *c=CachedLight(l);
 // Translate a cached stamp; flame flicker only scales its radiance.
 float offsetx=CENTER-l->x/CELL,offsety=CENTER-l->y/CELL;
 int bx=(int)floorf(offsetx),by=(int)floorf(offsety);
 float fx=offsetx-bx,fy=offsety-by;
 float aa=(1-fx)*(1-fy),ab=fx*(1-fy),ba=(1-fx)*fy,bb=fx*fy;
 for(int gy=0;gy<gridh;gy++){
  int sy=gy+by;if(sy<0||sy>=STAMP-1)continue;
  for(int gx=0;gx<gridw;gx++){
   int sx=gx+bx;if(sx<0||sx>=STAMP-1)continue;
   float power=c->power[sy][sx]*aa+c->power[sy][sx+1]*ab+c->power[sy+1][sx]*ba+c->power[sy+1][sx+1]*bb;
   rgb[gy][gx][0]+=power*l->r;rgb[gy][gx][1]+=power*l->g;rgb[gy][gx][2]+=power*l->b;
  }
 }
}
static int UiPixel(int x,int y) {
 if(x<0||y<0||x>=draw_w||y>=draw_h)return 1;
 return g_rt_ui_mask[y][x] || (has_dialogue&&x>=ui_left&&x<=ui_right&&y>=ui_top&&y<=ui_bottom);
}
static void Pixel(int x,int y,uint32 color,float alpha) {
 if(UiPixel(x,y))return;
 for(int yy=0;yy<draw_scale;yy++){
  uint32 *p=(uint32*)(draw_pixels+(y*draw_scale+yy)*draw_pitch)+x*draw_scale;
  for(int xx=0;xx<draw_scale;xx++){
   uint32 old=p[xx],out=0;for(int shift=0;shift<=16;shift+=8){int v=(int)(((color>>shift)&255)*alpha+((old>>shift)&255)*(1-alpha));out|=v<<shift;}p[xx]=out;
  }
 }
}
static int LinkCovers(int x,int y) {
 int extra=(draw_w-256)/2,lx=(int16)(link_x_coord-BG2HOFS_copy2)+extra,ly=(int16)(link_y_coord-BG2VOFS_copy2);
 return x>=lx&&x<lx+16&&y>=ly-3&&y<ly+23;
}
static void DrawBodies(void) {
 for(int i=0;i<num_flames;i++) {
  Flame *f=&flames[i];if(f->prop!=1)continue;int x=(int)f->x,y=(int)f->y;
  for(int dy=4;dy<16;dy++)for(int dx=-2;dx<=2;dx++)if(!LinkCovers(x+dx,y+dy))Pixel(x+dx,y+dy,dx==0?0x80674b:0x342b28,1);
  for(int dy=14;dy<=17;dy++)for(int dx=-5;dx<=5;dx++)if(!LinkCovers(x+dx,y+dy))Pixel(x+dx,y+dy,dy==14?0x8f8270:0x38363b,1);
  for(int dy=2;dy<=5;dy++)for(int dx=-5+dy/2;dx<=5-dy/2;dx++)if(!LinkCovers(x+dx,y+dy))Pixel(x+dx,y+dy,dy==2?0xbda17a:0x5c4735,1);
 }
}
static void DrawFlames(void) {
 for(int i=0;i<num_flames;i++) {
  Flame *f=&flames[i];int x=(int)f->x,y=(int)f->y;
  float t=visual_tick*0.16f+f->seed;
  int small=f->prop==2,height=small?5:10+(int)(sinf(t)*2);
  // Small additive-looking glow, confined to the immediate flame, never a room wash.
  for(int dy=-10;dy<=8;dy++)for(int dx=-10;dx<=10;dx++){
   float q=(dx*dx+dy*dy)/100.0f;if(q<1)Pixel(x+dx,y+dy,0xff8d20,(1-q)*0.06f);
  }
  for(int dy=-height;dy<=2;dy++) {
   float u=(dy+height)/(float)(height+2);int half=(int)(u*(small?2:4));
   int sway=small?0:(int)(sinf(t+dy*0.5f)*(1-u)*2);
   for(int dx=-half;dx<=half;dx++){
    int px=x+dx+sway,py=y+dy;
    if(f->prop!=2&&LinkCovers(px,py))continue;
    uint32 c=abs(dx)<=half/2?(dy>-height/2?0xfff4b0:0xffca46):0xf17320;
    Pixel(px,py,c,1);
   }
  }
  if(!small){int age=(visual_tick+f->seed*7)%55; if(age<28)Pixel(x+(int)(sinf(age*0.2f+f->seed)*3),y-10-age/2,0xffbd4f,1-age/35.0f);}
  if(small) {
   // Brass handle and cage: the flame is visibly held beside Link's hand.
   for(int yy=-7;yy<=4;yy++){Pixel(x-3,y+yy,0x8e733c,1);Pixel(x+3,y+yy,0x8e733c,1);}
   for(int xx=-3;xx<=3;xx++){Pixel(x+xx,y-7,0xc6ac66,1);Pixel(x+xx,y+4,0xc6ac66,1);}
   for(int xx=-2;xx<=2;xx++)Pixel(x+xx,y-10,0xc6ac66,1);
   Pixel(x-2,y-9,0xc6ac66,1);Pixel(x+2,y-9,0xc6ac66,1);
  }
 }
}
void RayLighting_Apply(uint8 *pixels,int pitch,int width,int height,int scale) {
 if(!ready)RayLighting_Init();
 if(!enabled||!pixels||scale<1)return;
 int w=width/scale,h=height/scale;if(w>MAX_W||h>MAX_H||w<256||h<160)return;
 int m=main_module_index;
 has_dialogue=(m==14&&submodule_index==2&&(saved_module_for_menu==7||saved_module_for_menu==9));
 if(!has_dialogue&&m!=7&&m!=9)return;
 if(INIDISP_copy==0||INIDISP_copy==0x80)return;
 visual_tick++;
 draw_pixels=pixels;draw_pitch=pitch;draw_scale=scale;draw_w=w;draw_h=h;
 int extra=(w-256)/2;gridw=w/CELL+1;gridh=h/CELL+1;
 world_scroll_x=BG2HOFS_copy2-extra;world_scroll_y=BG2VOFS_copy2;
 geometry_signature=GeometrySignature();
 ui_left=w;ui_top=h;ui_right=ui_bottom=-1;
 if(has_dialogue)for(int y=60;y<h;y++)for(int x=0;x<w;x++)if(g_rt_ui_mask[y][x]){
  if(x<ui_left)ui_left=x;if(x>ui_right)ui_right=x;if(y<ui_top)ui_top=y;if(y>ui_bottom)ui_bottom=y;
 }
 float ambient=player_is_indoors?0.014f:savegame_is_darkworld?0.024f:sram_progress_indicator<2?0.030f:0.22f;
 if(!cinematic)ambient*=2.5f;
 for(int gy=0;gy<gridh;gy++)for(int gx=0;gx<gridw;gx++){
  int x=BG2HOFS_copy2+gx*CELL-extra,y=BG2VOFS_copy2+gy*CELL;

  rgb[gy][gx][0]=ambient*0.65f;rgb[gy][gx][1]=ambient*0.85f;rgb[gy][gx][2]=ambient*1.20f;
 }
 GatherLights(w,h,extra);
 // Recover scene colors before the SNES darkness mask in dark rooms. The new
 // lantern supplies visibility instead of stacking darkness over a black cone.
 if(player_is_indoors && dung_want_lights_out) {
  for(int y=0;y<height;y++)for(int x=0;x<width;x++)if(!g_rt_ui_mask[y/scale][x/scale])
   ((uint32*)(pixels+y*pitch))[x]=g_rt_scene_rgb[y/scale][x/scale];
 }
 DrawBodies();
 for(int i=0;i<num_lights;i++)TraceLight(&lights[i]);
 for(int y=0;y<height;y++){
  uint32 *row=(uint32*)(pixels+y*pitch);float fy=(float)y/(scale*CELL);int iy=(int)fy;fy-=iy;
  for(int x=0;x<width;x++){
   int px=x/scale,py=y/scale;
   // Actual BG3/sprite UI mask, not estimated rectangles or color thresholds.
   if(g_rt_ui_mask[py][px])continue;
   if(has_dialogue&&px>=ui_left&&px<=ui_right&&py>=ui_top&&py<=ui_bottom){row[x]=0x080b15;continue;}
   float fx=(float)x/(scale*CELL);int ix=(int)fx;fx-=ix;uint32 c=row[x],out=0;
   for(int ch=0;ch<3;ch++){
    float top=rgb[iy][ix][ch]*(1-fx)+rgb[iy][ix+1][ch]*fx;
    float bot=rgb[iy+1][ix][ch]*(1-fx)+rgb[iy+1][ix+1][ch]*fx;
    float gain=top*(1-fy)+bot*fy;int shift=16-ch*8;
    int idx=(int)(linear_rgb[(c>>shift)&255]*gain*1024);if(idx>8192)idx=8192;if(idx<0)idx=0;
    out|=(uint32)tone[idx]<<shift;
   }
   row[x]=(c&0xff000000)|out;
  }
 }
 DrawFlames();
#ifdef DWA_TEST
 if(visual_tick%600==0)fprintf(stderr,"CACHE builds=%u hits=%u lights=%d\n",cache_builds,cache_hits,num_lights);
#endif
}
