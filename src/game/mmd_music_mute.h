#pragma once
#include "game/mmd_runtime_api.h"

// Music-system requests are identifier scoped. Do not change user volume or
// invoke the no-argument UnmuteMusic (which would release the game's requests).
namespace mmd_music_mute {
static void *managerField=nullptr,*instance=nullptr,*getMusic=nullptr,*mute=nullptr,*unmute=nullptr;
static bool resolved=false,available=false,managerStatic=false;
static uint32_t owner=0,key=0;
static bool recovery=false;
static double nextAttempt=0;
static std::string status;
inline bool Pending(){return owner!=0;}
inline bool Resolve() {
  if(resolved)return available;resolved=true;
  auto game=mmd_api::Class("Beyond.Gameplay","GameInstance");
  auto manager=mmd_api::Class("Beyond.Gameplay.Audio","AudioManager");
  auto music=mmd_api::Class("Beyond.Gameplay.Audio","AudioMusicSystem");
  if(!game||!manager||!music)return false;
  void *it=nullptr;
  while(auto f=il2cpp_class_get_fields(game,&it)) {
    auto name=il2cpp_field_get_name(f);
    if(name&&!strcmp(name,"audioManager")&&mmd_api::Type(il2cpp_field_get_type(f),"Beyond.Gameplay.Audio.AudioManager")) {managerField=f;managerStatic=(il2cpp_field_get_flags(f)&0x10)!=0;break;}
  }
  instance=mmd_api::Method(game,"get_instance","Beyond.Gameplay.GameInstance",{},true);
  getMusic=mmd_api::Method(manager,"get_music","Beyond.Gameplay.Audio.AudioMusicSystem");
  mute=mmd_api::Method(music,"MuteMusic","System.Void",{"System.String"});
  unmute=mmd_api::Method(music,"UnmuteMusic","System.Void",{"System.String"});
  available=managerField&&(managerStatic||instance)&&getMusic&&mute&&unmute&&il2cpp_gchandle_new&&il2cpp_gchandle_get_target&&il2cpp_gchandle_free&&il2cpp_string_new&&(!managerStatic||il2cpp_field_static_get_value);
  return available;
}
inline bool Current(void *&music) {
  music=nullptr;void *manager=nullptr;
  if(managerStatic) {
    __try {il2cpp_field_static_get_value(managerField,&manager);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
  } else {
    void *game=nullptr;if(!mmd_api::Call(instance,nullptr,nullptr,game))return false;
    if(!game)return true;
    auto offset=il2cpp_field_get_offset(managerField);
    if(offset<16||offset>=65536||!mmd_api::Copy((char*)game+offset,&manager,sizeof(manager)))return false;
  }
  return !manager||mmd_api::Call(getMusic,manager,nullptr,music);
}
inline void ReleaseHandles() {
  if(owner)il2cpp_gchandle_free(owner);if(key)il2cpp_gchandle_free(key);owner=key=0;recovery=false;
}
// Called only by the verified game-thread maintenance callback under pose lock.
inline void Tick(double now,bool wanted) {
  if(RuntimeClosing()||(!wanted&&!owner)||now<nextAttempt)return;
  if(!Resolve()){status=u8"当前游戏版本不支持背景音乐静音接口";return;}
  void *current=nullptr;
  if(!Current(current)){status=u8"等待背景音乐系统";nextAttempt=now+.5;return;}
  void *held=owner?il2cpp_gchandle_get_target(owner):nullptr;
  if(owner&&(!wanted||recovery||held!=current)) {
    void *result=nullptr,*identifier=key?il2cpp_gchandle_get_target(key):nullptr,*args[]{identifier};
    if(held&&identifier&&!mmd_api::Call(unmute,held,args,result)) {
      status=u8"正在恢复游戏背景音乐";nextAttempt=now+.5;return;
    }
    ReleaseHandles();status.clear();Log("[MMD-MUSIC] own background-music mute released");
  }
  if(!wanted||owner)return;
  if(!current){status=u8"等待背景音乐系统";nextAttempt=now+.5;return;}
  auto identifier=il2cpp_string_new("EndfieldPoser.MmdMusic");
  key=identifier?il2cpp_gchandle_new(identifier,false):0;
  owner=il2cpp_gchandle_new(current,false);
  if(!owner||!key){ReleaseHandles();status=u8"无法保存背景音乐恢复状态";nextAttempt=now+1;return;}
  void *result=nullptr,*args[]{identifier};
  if(!mmd_api::Call(mute,current,args,result)) {
    // A failed invocation may have registered before throwing. Keep ownership
    // and release it before retrying; never register repeated mute requests.
    status=u8"背景音乐静音失败，正在恢复";nextAttempt=now+.5;recovery=true;
    void *ignored=nullptr;if(mmd_api::Call(unmute,current,args,ignored))ReleaseHandles();
    return;
  }
  status=u8"游戏背景音乐已静音";Log("[MMD-MUSIC] own background-music mute acquired");
}
} // namespace mmd_music_mute
