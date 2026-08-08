#include <cstdint>
#include <cstring>
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#include "clevel_format.h"
#include "gfx/gfx.h"

namespace {
constexpr uint8_t MAX_ROOMS = 8;
constexpr uint8_t HISTORY_SIZE = 64;
// Every standalone map piece used by Celeste Classic, plus every logical
// gameplay entity. Companion/animation fragments (fake-wall quadrants, big
// chest quadrants, message quadrants, etc.) are intentionally represented by
// one complete logical piece instead of making authors assemble fragments.
constexpr uint8_t PALETTE[] = {
  0,1,
  // solid mountain terrain
  32,33,34,35,36,37,38,39,48,49,50,51,52,53,54,55,72,
  // ice/slippery terrain
  66,67,68,69,82,83,84,85,98,99,100,101,114,115,116,117,
  // directional spikes
  17,59,27,43,
  // original background / foreground / decorative map pieces
  16,40,41,42,44,56,57,58,60,61,62,63,73,74,75,76,77,78,79,
  88,89,90,91,92,93,94,95,103,104,105,106,107,108,109,110,111,
  121,122,123,124,125,126,127,
  // logical gameplay pieces
  8,11,12,18,20,22,23,26,28,64,86,96,118
};
constexpr uint8_t ENTITY_IDS[] = {18,22,23,26,28,64,8,20,11,12,86,96,118};

struct EditRoom { uint8_t tiles[256]; uint8_t rotations[256]; uint8_t spawn_x,spawn_y,exit_x,exit_y; };
struct Project { char title[64]; char author[32]; uint8_t room_count; EditRoom rooms[MAX_ROOMS]; } project;
struct Change { uint8_t room,index,before,after,before_rotation,after_rotation; } history[HISTORY_SIZE], redo_stack[HISTORY_SIZE];
uint8_t history_count=0,redo_count=0,room_index=0,cursor_x=2,cursor_y=13,palette_index=2,placement_rotation=0;
char notice[40]="";

bool is_entity(uint8_t id){for(uint8_t v:ENTITY_IDS)if(v==id)return true;return false;}
uint8_t migrate_legacy_tile(uint8_t id){if(id==2)return 32;if(id==3)return 33;if(id==4)return 66;if(id==5)return 67;return id;}
void migrate_legacy_tiles(){for(uint8_t r=0;r<project.room_count;r++)for(uint16_t i=0;i<256;i++)project.rooms[r].tiles[i]=migrate_legacy_tile(project.rooms[r].tiles[i]);}
void init_room(EditRoom &r){std::memset(&r,0,sizeof r);r.spawn_x=2;r.spawn_y=13;r.exit_x=13;r.exit_y=1;for(uint8_t x=0;x<16;x++)r.tiles[15*16+x]=37;for(uint8_t y=0;y<16;y++){r.tiles[y*16]=37;r.tiles[y*16+15]=37;}}
void new_project(){std::memset(&project,0,sizeof project);std::strcpy(project.title,"CALCULATOR LEVEL");std::strcpy(project.author,"LORD FUNION");project.room_count=1;init_room(project.rooms[0]);}
void set_notice(const char *s){std::strncpy(notice,s,sizeof notice-1);notice[sizeof notice-1]='\0';}
void save_draft(){uint8_t h=ti_Open("CELEDITS","w");if(!h){set_notice("DRAFT SAVE FAILED");return;}ti_Write(&project,sizeof project,1,h);ti_Close(h);set_notice("DRAFT SAVED");}
void load_draft(){uint8_t h=ti_Open("CELEDITS","r");if(!h){new_project();return;}if(ti_GetSize(h)==sizeof project&&ti_Read(&project,sizeof project,1,h)==1&&project.room_count>0&&project.room_count<=MAX_ROOMS){ti_Close(h);migrate_legacy_tiles();return;}ti_Close(h);new_project();}

uint8_t tile_color(uint8_t id){if(id==0)return 0;if((id>=66&&id<=69)||(id>=82&&id<=85)||(id>=98&&id<=101)||(id>=114&&id<=117))return 11;if(id==17||id==27||id==43||id==59)return 7;if(id==26||id==28)return 224;if(id==22)return 47;if(id==18)return 192;if(id==8)return 231;if(is_entity(id))return 164;return static_cast<uint8_t>(80+(id*13)%80);}
void draw_sprite(uint8_t id,int x,int y,uint8_t rotation=0){
  if(id<128&&atlas_tiles[id]) {
    rotation&=3;
    if(!rotation) gfx_TransparentSprite(atlas_tiles[id],x,y);
    else {gfx_TempSprite(tmp,8,8);if(rotation==1)gfx_RotateSpriteC(atlas_tiles[id],tmp);else if(rotation==2)gfx_RotateSpriteHalf(atlas_tiles[id],tmp);else gfx_RotateSpriteCC(atlas_tiles[id],tmp);gfx_TransparentSprite(tmp,x,y);}
  } else {gfx_SetColor(tile_color(id));gfx_FillRectangle(x,y,8,8);}
}
void rotate_offset(int dx,int dy,uint8_t rotation,int &rx,int &ry){switch(rotation&3){case 1:rx=-dy;ry=dx;break;case 2:rx=-dx;ry=-dy;break;case 3:rx=dy;ry=-dx;break;default:rx=dx;ry=dy;break;}}
void draw_child(uint8_t id,int x,int y,int dx,int dy,uint8_t rotation){int rx,ry;rotate_offset(dx,dy,rotation,rx,ry);draw_sprite(id,x+rx,y+ry,rotation);}
void draw_piece(uint8_t id,int x,int y,uint8_t rotation=0){
  // Complete logical objects, matching the actual CEleste runtime renderer.
  if(id==64){draw_child(64,x,y,0,0,rotation);draw_child(65,x,y,8,0,rotation);draw_child(80,x,y,0,8,rotation);draw_child(81,x,y,8,8,rotation);return;}
  if(id==96){draw_child(96,x,y,0,0,rotation);draw_child(97,x,y,8,0,rotation);draw_child(112,x,y,0,8,rotation);draw_child(113,x,y,8,8,rotation);return;}
  if(id==86){draw_child(70,x,y,0,-8,rotation);draw_child(71,x,y,8,-8,rotation);draw_child(86,x,y,0,0,rotation);draw_child(87,x,y,8,0,rotation);return;}
  if(id==11||id==12){draw_child(11,x-4,y-1,0,0,rotation);draw_child(12,x-4,y-1,8,0,rotation);return;}
  if(id==28){draw_child(45,x,y,-6,-2,rotation);draw_sprite(28,x,y,rotation);draw_child(45,x,y,6,-2,rotation);return;}
  if(id==22){draw_child(13,x,y,0,6,rotation);draw_sprite(22,x,y,rotation);return;}
  draw_sprite(id,x,y,rotation);
}
int palette_index_for(uint8_t id){for(unsigned i=0;i<sizeof PALETTE;i++)if(PALETTE[i]==id)return static_cast<int>(i);return -1;}
void draw(){
  gfx_FillScreen(0);const EditRoom &r=project.rooms[room_index];
  for(uint8_t y=0;y<16;y++)for(uint8_t x=0;x<16;x++){uint8_t id=r.tiles[y*16+x];if(id)draw_piece(id,8+x*8,48+y*8,r.rotations[y*16+x]);}
  draw_piece(1,8+r.spawn_x*8,48+r.spawn_y*8);
  gfx_SetColor(255);gfx_Rectangle(8+cursor_x*8,48+cursor_y*8,8,8);
  gfx_SetTextFGColor(255);gfx_PrintStringXY("CELESTE EDITOR",8,6);gfx_PrintStringXY(project.title,8,18);gfx_PrintStringXY("ROOM",8,31);gfx_SetTextXY(42,31);gfx_PrintUInt(room_index+1,1);gfx_PrintChar('/');gfx_PrintUInt(project.room_count,1);
  gfx_PrintStringXY("TILE",160,52);gfx_SetTextXY(199,52);gfx_PrintUInt(PALETTE[palette_index],1);draw_piece(PALETTE[palette_index],236,48,placement_rotation);
  gfx_PrintStringXY("COMPLETE PIECES",160,66);gfx_PrintStringXY("MODE: next",160,80);gfx_PrintStringXY("2ND: place",160,92);gfx_PrintStringXY("ALPHA: erase",160,104);gfx_PrintStringXY("WINDOW: rotate 90",160,212);gfx_PrintStringXY("+/-: rooms",160,116);gfx_PrintStringXY("ENTER: add",160,128);gfx_PrintStringXY("DEL: delete",160,140);gfx_PrintStringXY("TRACE: undo",160,152);gfx_PrintStringXY("GRAPH: export",160,164);gfx_PrintStringXY("STAT: details",160,176);gfx_PrintStringXY("Y=: help",160,188);gfx_PrintStringXY("CLEAR: quit",160,200);
  gfx_SetTextFGColor(231);gfx_PrintStringXY(notice,8,224);gfx_SwapDraw();
}
void record_change(uint8_t index,uint8_t before,uint8_t after,uint8_t before_rotation,uint8_t after_rotation){if(before==after&&before_rotation==after_rotation)return;if(history_count==HISTORY_SIZE){std::memmove(history,history+1,sizeof(Change)*(HISTORY_SIZE-1));history_count--;}history[history_count++]={room_index,index,before,after,before_rotation,after_rotation};redo_count=0;}
void paint_cell(uint8_t x,uint8_t y,uint8_t value,uint8_t rotation=0){if(x>=16||y>=16)return;EditRoom &r=project.rooms[room_index];const uint8_t index=y*16+x,before=r.tiles[index],before_rotation=r.rotations[index];record_change(index,before,value,before_rotation,rotation&3);r.tiles[index]=value;r.rotations[index]=rotation&3;}
void paint(uint8_t value){paint_cell(cursor_x,cursor_y,value,placement_rotation);}
bool place_piece(uint8_t value){
  // Keep compound pieces truly empty behind their full gameplay footprint.
  if((value==64||value==96)&&(cursor_x>=15||cursor_y>=15)){set_notice("NEEDS 2X2 SPACE");return false;}
  if(value==86&&(cursor_x>=15||cursor_y==0)){set_notice("MEMORIAL NEEDS 2X2");return false;}
  if(value==64||value==96){for(uint8_t dy=0;dy<2;dy++)for(uint8_t dx=0;dx<2;dx++)paint_cell(cursor_x+dx,cursor_y+dy,0,0);}
  else if(value==86){for(uint8_t dy=0;dy<2;dy++)for(uint8_t dx=0;dx<2;dx++)paint_cell(cursor_x+dx,cursor_y-1+dy,0,0);}
  paint(value);return true;
}
void erase_piece(){
  EditRoom &r=project.rooms[room_index];
  // Erasing any visible quadrant of a logical 2x2 object removes its anchor.
  for(int8_t dy=-1;dy<=1;dy++)for(int8_t dx=-1;dx<=1;dx++){int ax=int(cursor_x)+dx,ay=int(cursor_y)+dy;if(ax<0||ay<0||ax>=16||ay>=16)continue;uint8_t v=r.tiles[ay*16+ax];if((v==64||v==96)&&cursor_x>=ax&&cursor_x<=ax+1&&cursor_y>=ay&&cursor_y<=ay+1){paint_cell(ax,ay,0,0);return;}if(v==86&&cursor_x>=ax&&cursor_x<=ax+1&&cursor_y>=ay-1&&cursor_y<=ay){paint_cell(ax,ay,0,0);return;}}
  paint_cell(cursor_x,cursor_y,0,0);
}
void undo(){if(!history_count)return;Change c=history[--history_count];project.rooms[c.room].tiles[c.index]=c.before;project.rooms[c.room].rotations[c.index]=c.before_rotation;if(redo_count<HISTORY_SIZE)redo_stack[redo_count++]=c;room_index=c.room;}
void redo(){if(!redo_count)return;Change c=redo_stack[--redo_count];project.rooms[c.room].tiles[c.index]=c.after;project.rooms[c.room].rotations[c.index]=c.after_rotation;if(history_count<HISTORY_SIZE)history[history_count++]=c;room_index=c.room;}
void details(){gfx_End();os_ClrHomeFull();os_GetStringInput("LEVEL NAME",project.title,sizeof project.title);os_GetStringInput("AUTHOR",project.author,sizeof project.author);gfx_Begin();gfx_SetDrawBuffer();gfx_SetTextTransparentColor(0);gfx_SetPalette(mypalette,sizeof mypalette,0);}
void help(){gfx_FillScreen(0);gfx_SetTextFGColor(255);gfx_PrintStringXY("CELESTE EDITOR HELP",8,8);gfx_PrintStringXY("MODE cycles ALL original",8,30);gfx_PrintStringXY("terrain + logical pieces.",8,42);gfx_PrintStringXY("Tile 1 sets player spawn.",8,58);gfx_PrintStringXY("WINDOW rotates ANY piece.",8,70);gfx_PrintStringXY("118 is the summit flag",8,86);gfx_PrintStringXY("entity, NOT the exit.",8,98);gfx_PrintStringXY("64/86/96 draw as complete",8,114);gfx_PrintStringXY("multi-sprite objects.",8,126);gfx_PrintStringXY("Climb out through the TOP.",8,142);gfx_PrintStringXY("GRAPH exports an AppVar.",8,158);gfx_PrintStringXY("TI Connect -> .8xv",8,170);gfx_PrintStringXY("Press any key.",8,190);gfx_SwapDraw();while(!os_GetCSC()){}while(os_GetCSC()){} }

struct Writer{uint8_t *p;std::size_t cap,pos;bool ok;void u8(uint8_t v){if(pos<cap)p[pos++]=v;else ok=false;}void u16(uint16_t v){u8(v);u8(v>>8);}void u32(uint32_t v){u8(v);u8(v>>8);u8(v>>16);u8(v>>24);}void bytes(const void *src,std::size_t n){if(n<=cap-pos){std::memcpy(p+pos,src,n);pos+=n;}else ok=false;}};
void patch_u32(uint8_t *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
uint32_t hash_id(const char *s){uint32_t h=0x811c9dc5u;while(*s){h^=static_cast<uint8_t>(*s++);h*=0x01000193u;}return h;}
std::size_t rle_room(const EditRoom &r,uint8_t *out,std::size_t cap){std::size_t pos=0;for(uint16_t i=0;i<256;){uint8_t v=is_entity(r.tiles[i])?0:r.tiles[i],count=1;while(i+count<256&&(is_entity(r.tiles[i+count])?0:r.tiles[i+count])==v&&count<255)count++;if(pos+2>cap)return 0;out[pos++]=count;out[pos++]=v;i+=count;}return pos;}

std::size_t encode(uint8_t *out,std::size_t cap){
  const uint8_t title_len=std::strlen(project.title),author_len=std::strlen(project.author);Writer w{out,cap,0,true};
  w.bytes("CELV",4);w.u8(2);w.u8(1);w.u16(0);const std::size_t length_pos=w.pos;w.u32(0);const std::size_t crc_pos=w.pos;w.u32(0);w.u32(hash_id(project.title));w.u16(project.room_count);w.u8(2);w.u8(0);w.u8(title_len);w.u8(author_len);w.u16(0);w.u16(0x0101);w.u32(0);w.bytes(project.title,title_len);w.bytes(project.author,author_len);
  for(uint8_t ri=0;ri<project.room_count;ri++){
    const EditRoom &r=project.rooms[ri];uint8_t rle[512];const std::size_t rle_len=rle_room(r,rle,sizeof rle);if(!rle_len)return 0;
    uint8_t entity_count=0;for(uint16_t i=0;i<256;i++)if(is_entity(r.tiles[i]))entity_count++;
    uint8_t packed_rot[clevel::ROTATION_PLANE_BYTES]={0};for(uint16_t i=0;i<256;i++)if(!is_entity(r.tiles[i]))packed_rot[i>>2]|=(r.rotations[i]&3)<<((i&3)*2);const std::size_t record_len=16+rle_len+clevel::ROTATION_PLANE_BYTES+entity_count*4;w.u16(record_len);w.u8(16);w.u8(16);w.u8(r.spawn_x);w.u8(r.spawn_y);w.u8(r.exit_x);w.u8(r.exit_y);w.u8(0);w.u8(clevel::ROTATION_ENCODING_2BPP);w.u16(rle_len);w.u16(entity_count);w.u32(hash_id(project.title)+ri);w.bytes(rle,rle_len);w.bytes(packed_rot,sizeof packed_rot);
    for(uint16_t i=0;i<256;i++)if(is_entity(r.tiles[i])){w.u8(r.tiles[i]);w.u8(i%16);w.u8(i/16);w.u8(static_cast<uint8_t>((r.rotations[i]&3)<<clevel::ENTITY_ROTATION_SHIFT));}
  }
  if(!w.ok) return 0;
  patch_u32(out+length_pos,w.pos);
  patch_u32(out+crc_pos,clevel::crc32(out+34,w.pos-34));
  return w.pos;
}

void export_level(){static uint8_t payload[12000];const std::size_t size=encode(payload,sizeof payload);if(!size){set_notice("LEVEL TOO LARGE");return;}char name[9]="CL000000";const char hex[]="0123456789ABCDEF";uint32_t id=hash_id(project.title);for(uint8_t i=0;i<6;i++)name[7-i]=hex[(id>>(i*4))&15];uint8_t h=ti_Open(name,"w");if(!h||ti_Write(payload,1,size,h)!=size){if(h)ti_Close(h);set_notice("EXPORT FAILED");return;}ti_SetArchiveStatus(true,h);ti_Close(h);set_notice("EXPORTED APPVAR");}

void add_room(){if(project.room_count>=MAX_ROOMS){set_notice("8 ROOM LIMIT");return;}init_room(project.rooms[project.room_count]);room_index=project.room_count++;set_notice("ROOM ADDED");}
void delete_room(){if(project.room_count==1){set_notice("NEED ONE ROOM");return;}for(uint8_t i=room_index;i+1<project.room_count;i++)project.rooms[i]=project.rooms[i+1];project.room_count--;if(room_index>=project.room_count)room_index=project.room_count-1;set_notice("ROOM DELETED");}
}

int main(){
  load_draft();kb_SetMode(MODE_3_CONTINUOUS);gfx_Begin();gfx_SetDrawBuffer();gfx_SetTextTransparentColor(0);gfx_SetPalette(mypalette,sizeof mypalette,0);uint8_t old[8]={};bool running=true;
  while(running){
    draw();
    kb_Scan();
    auto pressed=[&](uint8_t group,uint8_t mask){
      const bool p=(kb_Data[group]&mask)&&!(old[group]&mask);
      return p;
    };
    if(pressed(7,kb_Left)&&cursor_x) cursor_x--;
    if(pressed(7,kb_Right)&&cursor_x<15) cursor_x++;
    if(pressed(7,kb_Up)&&cursor_y) cursor_y--;
    if(pressed(7,kb_Down)&&cursor_y<15) cursor_y++;
    if(pressed(1,kb_2nd)){
      uint8_t id=PALETTE[palette_index];
      if(id==1){
        project.rooms[room_index].spawn_x=cursor_x;
        project.rooms[room_index].spawn_y=cursor_y;
        set_notice("SPAWN SET");
      }else{
        place_piece(id);
      }
    }
    if(pressed(2,kb_Alpha)) erase_piece();
    if(pressed(1,kb_Mode)) palette_index=(palette_index+1)%(sizeof PALETTE);
    if(pressed(3,kb_Window)){
      EditRoom &r=project.rooms[room_index];const uint8_t index=cursor_y*16+cursor_x;
      if(r.tiles[index]){
        const uint8_t before=r.rotations[index],after=static_cast<uint8_t>((before+1)&3);
        record_change(index,r.tiles[index],r.tiles[index],before,after);r.rotations[index]=after;
        set_notice(after==0?"PIECE ROTATION 0":after==1?"PIECE ROTATION 90":after==2?"PIECE ROTATION 180":"PIECE ROTATION 270");
      }else{
        placement_rotation=static_cast<uint8_t>((placement_rotation+1)&3);
        set_notice(placement_rotation==0?"PALETTE ROTATION 0":placement_rotation==1?"PALETTE ROTATION 90":placement_rotation==2?"PALETTE ROTATION 180":"PALETTE ROTATION 270");
      }
    }
    if(pressed(6,kb_Add)&&room_index+1<project.room_count) room_index++;
    if(pressed(6,kb_Sub)&&room_index) room_index--;
    if(pressed(6,kb_Enter)) add_room();
    if(pressed(1,kb_Del)) delete_room();
    if(pressed(3,kb_Trace)) undo();
    if(pressed(3,kb_Zoom)) redo();
    if(pressed(3,kb_Graph)){ export_level(); save_draft(); }
    if(pressed(4,kb_Stat)) details();
    if(pressed(3,kb_Yequ)) help();
    if(pressed(1,kb_Clear)) running=false;
    for(uint8_t group=1;group<=7;group++) old[group]=kb_Data[group];
  }
  save_draft();gfx_End();return 0;
}
