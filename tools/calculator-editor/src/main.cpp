#include <cstdint>
#include <cstring>
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#include "clevel_format.h"

namespace {
constexpr uint8_t MAX_ROOMS = 8;
constexpr uint8_t HISTORY_SIZE = 64;
constexpr uint8_t PALETTE[] = {0,1,2,3,4,5,17,27,18,22,23,26,28,64,8,20,11,12,86,96,118};
constexpr uint8_t ENTITY_IDS[] = {18,22,23,26,28,64,8,20,11,12,86,96};

struct EditRoom { uint8_t tiles[256]; uint8_t spawn_x,spawn_y,exit_x,exit_y; };
struct Project { char title[64]; char author[32]; uint8_t room_count; EditRoom rooms[MAX_ROOMS]; } project;
struct Change { uint8_t room,index,before,after; } history[HISTORY_SIZE], redo_stack[HISTORY_SIZE];
uint8_t history_count=0,redo_count=0,room_index=0,cursor_x=2,cursor_y=13,palette_index=1;
char notice[40]="";

bool is_entity(uint8_t id){for(uint8_t v:ENTITY_IDS)if(v==id)return true;return false;}
void init_room(EditRoom &r){std::memset(&r,0,sizeof r);r.spawn_x=2;r.spawn_y=13;r.exit_x=13;r.exit_y=1;for(uint8_t x=0;x<16;x++)r.tiles[15*16+x]=2;for(uint8_t y=0;y<16;y++){r.tiles[y*16]=2;r.tiles[y*16+15]=2;}}
void new_project(){std::memset(&project,0,sizeof project);std::strcpy(project.title,"CALCULATOR LEVEL");std::strcpy(project.author,"LORD FUNION");project.room_count=1;init_room(project.rooms[0]);}
void set_notice(const char *s){std::strncpy(notice,s,sizeof notice-1);notice[sizeof notice-1]='\0';}
void save_draft(){uint8_t h=ti_Open("CELEDITS","w");if(!h){set_notice("DRAFT SAVE FAILED");return;}ti_Write(&project,sizeof project,1,h);ti_Close(h);set_notice("DRAFT SAVED");}
void load_draft(){uint8_t h=ti_Open("CELEDITS","r");if(!h){new_project();return;}if(ti_GetSize(h)==sizeof project&&ti_Read(&project,sizeof project,1,h)==1&&project.room_count>0&&project.room_count<=MAX_ROOMS){ti_Close(h);return;}ti_Close(h);new_project();}

uint8_t tile_color(uint8_t id){if(id==0)return 0;if(id==4)return 11;if(id==5)return 255;if(id==17||id==27)return 7;if(id==26||id==28)return 224;if(id==22)return 47;if(id==18)return 192;if(id==8)return 231;if(is_entity(id))return 164;return static_cast<uint8_t>(80+(id*13)%80);}
void draw(){
  gfx_FillScreen(0);const EditRoom &r=project.rooms[room_index];
  for(uint8_t y=0;y<16;y++)for(uint8_t x=0;x<16;x++){uint8_t id=r.tiles[y*16+x];gfx_SetColor(tile_color(id));gfx_FillRectangle(8+x*8,48+y*8,8,8);if(id){gfx_SetTextFGColor(0);gfx_SetTextXY(9+x*8,49+y*8);gfx_PrintUInt(id,1);}}
  gfx_SetColor(224);gfx_FillCircle(8+r.spawn_x*8+4,48+r.spawn_y*8+4,3);gfx_SetColor(52);gfx_FillCircle(8+r.exit_x*8+4,48+r.exit_y*8+4,3);
  gfx_SetColor(255);gfx_Rectangle(8+cursor_x*8,48+cursor_y*8,8,8);
  gfx_SetTextFGColor(255);gfx_PrintStringXY("CELESTE EDITOR",8,6);gfx_PrintStringXY(project.title,8,18);gfx_PrintStringXY("ROOM",8,31);gfx_SetTextXY(42,31);gfx_PrintUInt(room_index+1,1);gfx_PrintChar('/');gfx_PrintUInt(project.room_count,1);
  gfx_PrintStringXY("TILE",160,52);gfx_SetTextXY(199,52);gfx_PrintUInt(PALETTE[palette_index],1);
  gfx_PrintStringXY("MODE: tile",160,66);gfx_PrintStringXY("2ND: place",160,78);gfx_PrintStringXY("ALPHA: erase",160,90);gfx_PrintStringXY("+/-: rooms",160,102);gfx_PrintStringXY("ENTER: add",160,114);gfx_PrintStringXY("DEL: delete",160,126);gfx_PrintStringXY("TRACE: undo",160,138);gfx_PrintStringXY("GRAPH: export",160,150);gfx_PrintStringXY("STAT: details",160,162);gfx_PrintStringXY("Y=: help",160,174);gfx_PrintStringXY("CLEAR: quit",160,186);
  gfx_SetTextFGColor(231);gfx_PrintStringXY(notice,8,224);gfx_SwapDraw();
}
void record_change(uint8_t index,uint8_t before,uint8_t after){if(before==after)return;if(history_count==HISTORY_SIZE){std::memmove(history,history+1,sizeof(Change)*(HISTORY_SIZE-1));history_count--;}history[history_count++]={room_index,index,before,after};redo_count=0;}
void paint(uint8_t value){EditRoom &r=project.rooms[room_index];const uint8_t index=cursor_y*16+cursor_x,before=r.tiles[index];record_change(index,before,value);r.tiles[index]=value;}
void undo(){if(!history_count)return;Change c=history[--history_count];project.rooms[c.room].tiles[c.index]=c.before;if(redo_count<HISTORY_SIZE)redo_stack[redo_count++]=c;room_index=c.room;}
void redo(){if(!redo_count)return;Change c=redo_stack[--redo_count];project.rooms[c.room].tiles[c.index]=c.after;if(history_count<HISTORY_SIZE)history[history_count++]=c;room_index=c.room;}
void details(){gfx_End();os_ClrHomeFull();os_GetStringInput("LEVEL NAME",project.title,sizeof project.title);os_GetStringInput("AUTHOR",project.author,sizeof project.author);gfx_Begin();gfx_SetDrawBuffer();gfx_SetTextTransparentColor(0);}
void help(){gfx_FillScreen(0);gfx_SetTextFGColor(255);gfx_PrintStringXY("CELESTE EDITOR HELP",8,8);gfx_PrintStringXY("MODE cycles tile IDs.",8,30);gfx_PrintStringXY("Choose tile 1, then place",8,42);gfx_PrintStringXY("to move the player spawn.",8,54);gfx_PrintStringXY("Choose tile 118 to set the",8,66);gfx_PrintStringXY("suggested exit position.",8,78);gfx_PrintStringXY("The playable exit is a hole",8,90);gfx_PrintStringXY("in the TOP of each room.",8,102);gfx_PrintStringXY("GRAPH exports an AppVar.",8,126);gfx_PrintStringXY("TI Connect saves it as .8xv",8,138);gfx_PrintStringXY("Press any key.",8,190);gfx_SwapDraw();while(!os_GetCSC()){}while(os_GetCSC()){} }

struct Writer{uint8_t *p;std::size_t cap,pos;bool ok;void u8(uint8_t v){if(pos<cap)p[pos++]=v;else ok=false;}void u16(uint16_t v){u8(v);u8(v>>8);}void u32(uint32_t v){u8(v);u8(v>>8);u8(v>>16);u8(v>>24);}void bytes(const void *src,std::size_t n){if(n<=cap-pos){std::memcpy(p+pos,src,n);pos+=n;}else ok=false;}};
void patch_u32(uint8_t *p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
uint32_t hash_id(const char *s){uint32_t h=0x811c9dc5u;while(*s){h^=static_cast<uint8_t>(*s++);h*=0x01000193u;}return h;}
std::size_t rle_room(const EditRoom &r,uint8_t *out,std::size_t cap){std::size_t pos=0;for(uint16_t i=0;i<256;){uint8_t v=r.tiles[i],count=1;while(i+count<256&&r.tiles[i+count]==v&&count<255)count++;if(pos+2>cap)return 0;out[pos++]=count;out[pos++]=v;i+=count;}return pos;}

std::size_t encode(uint8_t *out,std::size_t cap){
  const uint8_t title_len=std::strlen(project.title),author_len=std::strlen(project.author);Writer w{out,cap,0,true};
  w.bytes("CELV",4);w.u8(1);w.u8(1);w.u16(0);const std::size_t length_pos=w.pos;w.u32(0);const std::size_t crc_pos=w.pos;w.u32(0);w.u32(hash_id(project.title));w.u16(project.room_count);w.u8(2);w.u8(0);w.u8(title_len);w.u8(author_len);w.u16(0);w.u16(0x0100);w.u32(0);w.bytes(project.title,title_len);w.bytes(project.author,author_len);
  for(uint8_t ri=0;ri<project.room_count;ri++){
    const EditRoom &r=project.rooms[ri];uint8_t rle[512];const std::size_t rle_len=rle_room(r,rle,sizeof rle);if(!rle_len)return 0;
    uint8_t entity_count=0;for(uint16_t i=0;i<256;i++)if(is_entity(r.tiles[i]))entity_count++;
    const std::size_t record_len=16+rle_len+entity_count*4;w.u16(record_len);w.u8(16);w.u8(16);w.u8(r.spawn_x);w.u8(r.spawn_y);w.u8(r.exit_x);w.u8(r.exit_y);w.u8(0);w.u8(0);w.u16(rle_len);w.u16(entity_count);w.u32(hash_id(project.title)+ri);w.bytes(rle,rle_len);
    for(uint16_t i=0;i<256;i++)if(is_entity(r.tiles[i])){w.u8(r.tiles[i]);w.u8(i%16);w.u8(i/16);w.u8(0);}
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
  load_draft();kb_SetMode(MODE_3_CONTINUOUS);gfx_Begin();gfx_SetDrawBuffer();gfx_SetTextTransparentColor(0);uint8_t old[8]={};bool running=true;
  while(running){
    draw();kb_Scan();
    auto pressed=[&](uint8_t group,uint8_t mask){const bool p=(kb_Data[group]&mask)&&!(old[group]&mask);return p;};
    if(pressed(7,kb_Left)&&cursor_x) cursor_x--;
    if(pressed(7,kb_Right)&&cursor_x<15) cursor_x++;
    if(pressed(7,kb_Up)&&cursor_y) cursor_y--;
    if(pressed(7,kb_Down)&&cursor_y<15) cursor_y++;
    if(pressed(1,kb_2nd)){
      uint8_t id=PALETTE[palette_index];
      if(id==1){project.rooms[room_index].spawn_x=cursor_x;project.rooms[room_index].spawn_y=cursor_y;set_notice("SPAWN SET");}
      else if(id==118){project.rooms[room_index].exit_x=cursor_x;project.rooms[room_index].exit_y=cursor_y;set_notice("EXIT MARKED");}
      else paint(id);
    }
    if(pressed(2,kb_Alpha)) paint(0);
    if(pressed(1,kb_Mode)) palette_index=(palette_index+1)%(sizeof PALETTE);
    if(pressed(6,kb_Add)&&room_index+1<project.room_count) room_index++;
    if(pressed(6,kb_Sub)&&room_index) room_index--;
    if(pressed(6,kb_Enter)) add_room();
    if(pressed(1,kb_Del)) delete_room();
    if(pressed(3,kb_Trace)) undo();
    if(pressed(3,kb_Zoom)) redo();
    if(pressed(3,kb_Graph)){export_level();save_draft();}
    if(pressed(4,kb_Stat)) details();
    if(pressed(3,kb_Yequ)) help();
    if(pressed(1,kb_Clear)) running=false;
    std::memcpy(old,kb_Data,sizeof old);
  }
  save_draft();gfx_End();return 0;
}
