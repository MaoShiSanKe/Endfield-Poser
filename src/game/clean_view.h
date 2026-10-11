#pragma once
#include "core/clean_view_state.h"
#include "game/mmd_visibility.h"

// Only called by the game-thread maintenance callback. No GameObjects,
// gameplay panels, input modules, cameras or time settings are disabled.
namespace poser_clean_view {
struct Api {
  void *all=nullptr,*type=nullptr,*get=nullptr,*set=nullptr;
} static api;
struct Canvas {mmd_visibility::Ref object;bool enabled=false;};
static std::vector<Canvas> canvases;
static double nextScan=0,nextRetry=0;
static int lastMode=Visible;
static bool failed=false;
static const char *status=u8"游戏界面正常显示";
inline bool Pending(){return !canvases.empty();}
inline bool Ready() {
  if(api.all)return true;
  if(!mmd_visibility::Ready())return false;
  auto canvas=mmd_api::Class("UnityEngine","Canvas"),object=mmd_api::Class("UnityEngine","Object");
  if(!canvas||!object||!il2cpp_class_get_type||!il2cpp_type_get_object)return false;
  Api a;
  a.all=mmd_api::Method(object,"FindObjectsOfType","UnityEngine.Object[]",{"System.Type","System.Boolean"},true);
  auto type=il2cpp_class_get_type(canvas);a.type=type?il2cpp_type_get_object(type):nullptr;
  a.get=mmd_api::Method(canvas,"get_enabled","System.Boolean");
  a.set=mmd_api::Method(canvas,"set_enabled","System.Void",{"System.Boolean"});
  if(!a.all||!a.type||!a.get||!a.set)return false;
  api=a;return true;
}
inline bool Write(void *canvas,bool value) {
  bool current=false;if(!mmd_api::Value(api.get,canvas,current))return false;
  if(current==value)return true;
  void *result=nullptr,*args[]{&value};
  return mmd_api::Call(api.set,canvas,args,result)&&mmd_api::Value(api.get,canvas,current)&&current==value;
}
inline bool Restore() {
  for(auto it=canvases.begin();it!=canvases.end();) {
    void *object=nullptr;auto life=it->object.inspect(object);
    if(life==mmd_visibility::Life::Unavailable||
        (life==mmd_visibility::Life::Alive&&!Write(object,it->enabled))){++it;continue;}
    it=canvases.erase(it);
  }
  return canvases.empty();
}
inline bool Discover() {
  void *array=nullptr;bool inactive=true;void *args[]{api.type,&inactive};size_t count=0;
  if(!mmd_api::Call(api.all,nullptr,args,array)||!array||
      !mmd_api::Copy((char*)array+IL2CPP_ARRAY_LEN,&count,sizeof(count))||count>4096)return false;
  std::vector<void*> objects(count);
  if(count&&!mmd_api::Copy((char*)array+IL2CPP_ARRAY_DATA,objects.data(),count*sizeof(void*)))return false;
  std::unordered_set<void*> known;
  for(const auto &c:canvases) {
    void *object=nullptr;const auto life=c.object.inspect(object);
    // Never recapture an already hidden Canvas as originally disabled while
    // its retained identity is temporarily unreadable.
    if(life==mmd_visibility::Life::Unavailable)return false;
    if(life==mmd_visibility::Life::Alive)known.insert(object);
  }
  for(auto object:objects) {
    if(!UnityObjAlive(object)||known.count(object))continue;
    if(canvases.size()>=4096)return false;
    Canvas c;
    if(!mmd_api::Value(api.get,object,c.enabled)||!c.object.capture(object))return false;
    canvases.push_back(std::move(c));known.insert(object);
  }
  return true;
}
inline void Tick(double now,bool allowed) {
  if(RuntimeClosing())return;
  if(!allowed)Request(Visible);
  const int mode=requested.load();
  if(mode!=lastMode){nextScan=nextRetry=0;lastMode=mode;if(mode!=Visible)failed=false;}
  if(mode==Visible) {
    if(!Pending()){if(!failed)status=u8"游戏界面正常显示";return;}
    if(now<nextRetry)return;
    nextRetry=now+.1;
    if(Restore()){if(!failed)status=u8"游戏界面已恢复";Log("[CLEAN-VIEW] restored");}
    else status=u8"正在恢复游戏界面";
    return;
  }
  if(now<nextRetry)return;
  if(!Ready()){failed=true;status=u8"当前游戏界面接口不可用";Request(Visible);return;}
  bool ok=true;
  for(auto it=canvases.begin();it!=canvases.end();) {
    void *object=nullptr;auto life=it->object.inspect(object);
    if(life==mmd_visibility::Life::Dead){it=canvases.erase(it);continue;}
    if(life!=mmd_visibility::Life::Alive||!Write(object,false))ok=false;
    ++it;
  }
  if(ok&&now>=nextScan) {
    nextScan=now+.25;ok=Discover();
    if(ok)for(auto &c:canvases) {
      void *object=nullptr;auto life=c.object.inspect(object);
      if(life==mmd_visibility::Life::Unavailable||
          (life==mmd_visibility::Life::Alive&&!Write(object,false)))ok=false;
    }
  }
  if(!ok) {
    // Keep the editor accessible and roll back every captured Canvas, even
    // after a partially successful setter. Retry restoration on the next tick.
    failed=true;Request(Visible);Restore();status=u8"隐藏未完成，已请求恢复界面";
    Log("[CLEAN-VIEW] hide failed; restorePending=%zu",canvases.size());return;
  }
  failed=false;
  status=canvases.empty()?u8"等待游戏界面就绪":u8"游戏界面已隐藏；快捷键或 Esc 恢复";
}
}
