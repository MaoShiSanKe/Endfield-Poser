#pragma once
#include "game/mmd_runtime_api.h"
#include <algorithm>
#include <memory>
#include <unordered_set>

// Render-only model visibility. Never disable a GameObject, Animator or BBC.
// No mesh/material access; hidden actors retain the ordinary playback updates.
namespace mmd_visibility {
struct Api {
  void *query=nullptr,*rendererType=nullptr,*instance=nullptr;
  void *get=nullptr,*set=nullptr,*childOf=nullptr;
} static api;
inline bool Ready() {
  if(api.query&&api.rendererType&&api.instance&&api.get&&api.set&&api.childOf)return true;
  if(!il2cpp_class_get_type||!il2cpp_type_get_object||!g_gameObjectClass)return false;
  auto renderer=mmd_api::Class("UnityEngine","Renderer"),object=mmd_api::Class("UnityEngine","Object");
  auto transform=mmd_api::Class("UnityEngine","Transform");
  if(!renderer||!object||!transform)return false;
  Api next;
  next.query=mmd_api::Method(g_gameObjectClass,"GetComponentsInChildren","UnityEngine.Component[]",{"System.Type","System.Boolean"});
  auto type=il2cpp_class_get_type(renderer);next.rendererType=type?il2cpp_type_get_object(type):nullptr;
  next.instance=mmd_api::Method(object,"GetInstanceID","System.Int32");
  next.get=mmd_api::Method(renderer,"get_forceRenderingOff","System.Boolean");
  next.set=mmd_api::Method(renderer,"set_forceRenderingOff","System.Void",{"System.Boolean"});
  next.childOf=mmd_api::Method(transform,"IsChildOf","System.Boolean",{"UnityEngine.Transform"});
  if(!next.query||!next.rendererType||!next.instance||!next.get||!next.set||!next.childOf)return false;
  api=next;return true;
}
inline void Free(uint32_t handle) noexcept {
  __try {if(handle&&!RuntimeClosing()&&il2cpp_gchandle_free)il2cpp_gchandle_free(handle);}
  __except(EXCEPTION_EXECUTE_HANDLER){}
}
enum class Life { Dead, Alive, Unavailable };
struct Ref {
  uint32_t handle=0;int instance=0;
  Ref()=default;
  Ref(const Ref&)=delete;Ref &operator=(const Ref&)=delete;
  Ref(Ref &&r) noexcept:handle(r.handle),instance(r.instance){r.handle=0;}
  Ref &operator=(Ref &&r) noexcept {if(this!=&r){Free(handle);handle=r.handle;instance=r.instance;r.handle=0;}return *this;}
  ~Ref(){Free(handle);}
  bool capture(void *object) {
    if(!UnityObjAlive(object)||!il2cpp_gchandle_new||!il2cpp_gchandle_get_target||!il2cpp_gchandle_free||
        !mmd_api::Value(api.instance,object,instance))return false;
    handle=il2cpp_gchandle_new(object,false);return handle!=0;
  }
  Life inspect(void *&object) const {
    object=nullptr;
    __try {
      if(!handle)return Life::Dead;
      if(!il2cpp_gchandle_get_target)return Life::Unavailable;
      object=il2cpp_gchandle_get_target(handle);if(!object)return Life::Dead;
      uintptr_t native=0;int id=0;
      if(!mmd_api::Copy((char*)object+16,&native,sizeof(native)))return Life::Unavailable;
      if(!native)return Life::Dead;
      if(!mmd_api::Value(api.instance,object,id))return Life::Unavailable;
      return id==instance?Life::Alive:Life::Dead;
    } __except(EXCEPTION_EXECUTE_HANDLER){return Life::Unavailable;}
  }
  void *target() const {
    void *object=nullptr;return inspect(object)==Life::Alive?object:nullptr;
  }
};
// HG character draws can ignore forceRenderingOff (also observed by layered
// recording). Lease an unused scene layer and exclude it from camera culling.
// Shared by hidden squad members; restore the bit only after the last member.
struct CullingApi {
  void *all=nullptr,*goType=nullptr,*cameraType=nullptr,*transform=nullptr;
  void *getLayer=nullptr,*setLayer=nullptr,*getMask=nullptr,*setMask=nullptr;
} static cullingApi;
inline bool CullingReady() {
  if(cullingApi.all)return true;
  if(!il2cpp_class_get_type||!il2cpp_type_get_object)return false;
  auto go=mmd_api::Class("UnityEngine","GameObject"),camera=mmd_api::Class("UnityEngine","Camera");
  auto object=mmd_api::Class("UnityEngine","Object");
  if(!go||!camera||!object)return false;
  CullingApi a;
  auto type=[&](void *cls){auto t=il2cpp_class_get_type(cls);return t?il2cpp_type_get_object(t):nullptr;};
  a.goType=type(go);a.cameraType=type(camera);
  a.all=mmd_api::Method(object,"FindObjectsOfType","UnityEngine.Object[]",{"System.Type","System.Boolean"},true);
  a.transform=mmd_api::Method(go,"get_transform","UnityEngine.Transform");
  a.getLayer=mmd_api::Method(go,"get_layer","System.Int32");
  a.setLayer=mmd_api::Method(go,"set_layer","System.Void",{"System.Int32"});
  a.getMask=mmd_api::Method(camera,"get_cullingMask","System.Int32");
  a.setMask=mmd_api::Method(camera,"set_cullingMask","System.Void",{"System.Int32"});
  if(!a.all||!a.goType||!a.cameraType||!a.transform||!a.getLayer||!a.setLayer||!a.getMask||!a.setMask)return false;
  cullingApi=a;return true;
}
inline bool WriteInt(void *get,void *set,void *object,int value) {
  int current=0;if(!mmd_api::Value(get,object,current))return false;
  if(current==value)return true;
  void *result=nullptr,*args[]{&value};
  return mmd_api::Call(set,object,args,result)&&mmd_api::Value(get,object,current)&&current==value;
}
inline bool Objects(void *type,size_t limit,std::vector<void*> &values) {
  void *array=nullptr;bool inactive=true;void *args[]{type,&inactive};size_t count=0;
  if(!mmd_api::Call(cullingApi.all,nullptr,args,array)||!array||
     !mmd_api::Copy((char*)array+IL2CPP_ARRAY_LEN,&count,sizeof(count))||count>limit)return false;
  values.resize(count);
  return !count||mmd_api::Copy((char*)array+IL2CPP_ARRAY_DATA,values.data(),count*sizeof(void*));
}
struct Camera {Ref object;bool included=false;};
struct Exclusion {int layer=-1;std::vector<Camera> cameras;double nextScan=0;};
static std::shared_ptr<Exclusion> exclusion;
static int captureLayer=-1; // Reserved even between recording passes.
inline bool CullingPending(){return bool(exclusion);}
inline uint32_t ReservedLayers(){return exclusion?uint32_t(1)<<exclusion->layer:0;}
inline int CameraMask(void *camera,int mask,bool original=false) {
  if(!exclusion)return mask;
  const uint32_t bit=uint32_t(1)<<exclusion->layer;
  for(const auto &c:exclusion->cameras)if(c.object.target()==camera)
    return int((uint32_t(mask)&~bit)|(original&&c.included?bit:0));
  return original?mask:int(uint32_t(mask)&~bit);
}
inline bool ObserveCamera(void *camera) {
  if(!exclusion||!UnityObjAlive(camera))return true;
  auto &e=*exclusion;const uint32_t bit=uint32_t(1)<<e.layer;
  bool captured=false;
  for(const auto &c:e.cameras) {
    void *object=nullptr;auto life=c.object.inspect(object);
    if(life==Life::Unavailable)return false; // Never recapture our own masked bit as its original value.
    captured|=life==Life::Alive&&object==camera;
  }
  int mask=0;if(!mmd_api::Value(cullingApi.getMask,camera,mask))return false;
  if(!captured) {
    if(e.cameras.size()>=256)return false;
    Camera c;c.included=(uint32_t(mask)&bit)!=0;
    if(!c.object.capture(camera))return false;
    e.cameras.push_back(std::move(c));
  }
  return WriteInt(cullingApi.getMask,cullingApi.setMask,camera,int(uint32_t(mask)&~bit));
}
inline bool RestoreCulling() {
  if(!exclusion||exclusion.use_count()>1)return true;
  if(RuntimeClosing()){exclusion.reset();return true;}
  auto &e=*exclusion;const uint32_t bit=uint32_t(1)<<e.layer;
  for(auto it=e.cameras.begin();it!=e.cameras.end();) {
    void *camera=nullptr;const auto life=it->object.inspect(camera);int mask=0;
    // Restore only the owned bit; camera/game changes to other layers survive.
    if(life==Life::Unavailable||(life==Life::Alive&&(!mmd_api::Value(cullingApi.getMask,camera,mask)||
       !WriteInt(cullingApi.getMask,cullingApi.setMask,camera,int((uint32_t(mask)&~bit)|(it->included?bit:0)))))) {++it;continue;}
    it=e.cameras.erase(it);
  }
  if(!e.cameras.empty())return false;
  exclusion.reset();return true;
}
inline bool MaintainCulling(double now) {
  if(!exclusion)return true;
  if(exclusion.use_count()==1)return RestoreCulling();
  auto &e=*exclusion;
  bool ok=true;
  for(auto it=e.cameras.begin();it!=e.cameras.end();) {
    void *camera=nullptr;auto life=it->object.inspect(camera);
    if(life==Life::Dead){it=e.cameras.erase(it);continue;}
    int mask=0;const uint32_t bit=uint32_t(1)<<e.layer;
    if(life==Life::Unavailable||!mmd_api::Value(cullingApi.getMask,camera,mask)||
       !WriteInt(cullingApi.getMask,cullingApi.setMask,camera,int(uint32_t(mask)&~bit)))ok=false;
    ++it;
  }
  if(now>=e.nextScan) {
    e.nextScan=now+.2;std::vector<void*> cameras;
    if(!Objects(cullingApi.cameraType,256,cameras))return false;
    for(auto camera:cameras)if(!ObserveCamera(camera))ok=false;
  }
  return ok&&!e.cameras.empty();
}
inline void AfterCamera(void *camera) {
  if(!exclusion)return;
  if(exclusion.use_count()>1)ObserveCamera(camera);else RestoreCulling();
}
inline bool AcquireCulling(std::shared_ptr<Exclusion> &out,double now) {
  if(out)return MaintainCulling(now);
  if(exclusion&&exclusion.use_count()==1&&!RestoreCulling())return false;
  if(!exclusion) {
    std::vector<void*> objects;if(!CullingReady()||!Objects(cullingApi.goType,262144,objects)||objects.empty())return false;
    uint32_t used=captureLayer>=0?uint32_t(1)<<captureLayer:0;int layer=0;
    for(auto go:objects)if(UnityObjAlive(go)) {
      if(!mmd_api::Value(cullingApi.getLayer,go,layer)||layer<0||layer>31)return false;
      used|=uint32_t(1)<<layer;
    }
    int freeLayer=-1;for(int n=31;n>=8;--n)if(!(used&(uint32_t(1)<<n))){freeLayer=n;break;}
    if(freeLayer<0)return false;
    exclusion=std::make_shared<Exclusion>();exclusion->layer=freeLayer;
  }
  out=exclusion;return MaintainCulling(now);
}
struct Renderer {Ref object;bool original=false;};
struct Layer {Ref object;int original=0;};
struct Lease {Ref owner,root;std::vector<Renderer> renderers;std::vector<Layer> layers;
  std::shared_ptr<Exclusion> culling;double nextScan=0;};
struct State {std::shared_ptr<Lease> lease;bool failed=false;double retryAt=0;};
static std::vector<std::weak_ptr<Lease>> leases;
inline void PruneLeases() {
  leases.erase(std::remove_if(leases.begin(),leases.end(),[](const auto &w){return w.expired();}),leases.end());
}
// Captures save the pre-playback layer and restore the currently requested
// display state. Their longer lifetime may cross several hide/show keyframes.
inline int ObjectLayer(void *go,int fallback,bool original=false) {
  for(const auto &weak:leases)if(auto lease=weak.lock())if(lease->culling)
    for(const auto &l:lease->layers)if(l.object.target()==go)return original?l.original:lease->culling->layer;
  return fallback;
}
inline int Scope(void *renderer,void *root,void *getTransform=g_component_get_transform) {
  void *transform=nullptr,*result=nullptr,*args[]{root};bool child=false;
  if(!root||!mmd_api::Call(getTransform,renderer,nullptr,transform)||!UnityObjAlive(transform))return -1;
  if(transform==root)return 1;
  if(!mmd_api::Call(api.childOf,transform,args,result)||!result||!mmd_api::Copy((char*)result+16,&child,sizeof(child)))return -1;
  return child?1:0;
}
inline bool Write(void *renderer,bool value) {
  bool current=false;
  if(!mmd_api::Value(api.get,renderer,current))return false;
  if(current==value)return true;
  void *result=nullptr,*args[]{&value};
  return mmd_api::Call(api.set,renderer,args,result)&&mmd_api::Value(api.get,renderer,current)&&current==value;
}
inline bool Restore(State &s) {
  if(!s.lease)return true;
  void *root=nullptr;
  auto life=RuntimeClosing()?Life::Dead:s.lease->root.inspect(root);
  if(life==Life::Unavailable)return false;
  if(life==Life::Dead)root=nullptr;
  if(root)for(auto it=s.lease->renderers.begin();it!=s.lease->renderers.end();) {
    void *renderer=nullptr;life=it->object.inspect(renderer);
    int scope=life==Life::Alive?Scope(renderer,root):0;
    if(life==Life::Unavailable||scope<0||(scope>0&&!Write(renderer,it->original))){++it;continue;}
    it=s.lease->renderers.erase(it);
  }
  if(root&&!s.lease->renderers.empty())return false;
  if(root)for(auto it=s.lease->layers.begin();it!=s.lease->layers.end();) {
    void *go=nullptr;life=it->object.inspect(go);
    int scope=life==Life::Alive?Scope(go,root,cullingApi.transform):0;
    if(life==Life::Unavailable||scope<0||(scope>0&&!WriteInt(cullingApi.getLayer,cullingApi.setLayer,go,it->original))){++it;continue;}
    it=s.lease->layers.erase(it);
  }
  if(root&&!s.lease->layers.empty())return false;
  s.lease.reset();PruneLeases();RestoreCulling();return true;
}
inline bool Hide(Lease &lease,void *root) {
  for(auto &l:lease.layers) {
    void *go=nullptr;auto life=l.object.inspect(go);
    if(life==Life::Dead)continue;
    if(life==Life::Unavailable)return false;
    int scope=Scope(go,root,cullingApi.transform);
    if(scope<0||(scope>0&&!WriteInt(cullingApi.getLayer,cullingApi.setLayer,go,lease.culling->layer)))return false;
  }
  for(auto &r:lease.renderers) {
    void *renderer=nullptr;auto life=r.object.inspect(renderer);
    if(life==Life::Dead)continue;
    if(life==Life::Unavailable)return false;
    bool current=false;
    if(!mmd_api::Value(api.get,renderer,current))return false;
    if(!current){int scope=Scope(renderer,root);if(scope<0||(scope>0&&!Write(renderer,true)))return false;}
  }
  return true;
}
inline bool Tick(State &s,void *owner,void *root,bool visible,double now) {
  if(visible){s.retryAt=0;return Restore(s);}
  if(RuntimeClosing())return false;
  if(now<s.retryAt)return false;
  auto fail=[&](){Restore(s);s.retryAt=now+1;return false;};
  if(!Ready()||!UnityObjAlive(owner)||!UnityObjAlive(root))return fail();
  if(!s.lease) {
    auto lease=std::make_shared<Lease>();
    if(!lease->owner.capture(owner)||!lease->root.capture(root))return fail();
    s.lease=std::move(lease);
    PruneLeases();leases.push_back(s.lease);
  }
  auto &lease=*s.lease;
  if(lease.owner.target()!=owner||lease.root.target()!=root)return fail();
  if(!AcquireCulling(lease.culling,now))return fail();
  if(now<lease.nextScan)return Hide(lease,root)?true:fail();
  lease.nextScan=now+.2;
  // Include inactive LODs and particle renderers so switching LODs or enabling
  // an existing effect cannot reveal an otherwise hidden character.
  void *go=nullptr,*array=nullptr;bool inactive=true;void *args[]{api.rendererType,&inactive};size_t count=0;
  if(!mmd_api::Call(g_component_get_gameObject,root,nullptr,go)||!UnityObjAlive(go)||
      !mmd_api::Call(api.query,go,args,array)||!array||
      !mmd_api::Copy((char*)array+IL2CPP_ARRAY_LEN,&count,sizeof(count))||count==0||count>1024)return fail();
  for(auto it=lease.renderers.begin();it!=lease.renderers.end();) {
    void *renderer=nullptr;auto life=it->object.inspect(renderer);
    int scope=life==Life::Alive?Scope(renderer,root):0;
    if(life==Life::Unavailable||scope<0)return fail();
    if(life==Life::Dead||!scope)it=lease.renderers.erase(it);else ++it;
  }
  for(auto it=lease.layers.begin();it!=lease.layers.end();) {
    void *go=nullptr;auto life=it->object.inspect(go);
    int scope=life==Life::Alive?Scope(go,root,cullingApi.transform):0;
    if(life==Life::Unavailable||scope<0)return fail();
    if(life==Life::Dead||!scope)it=lease.layers.erase(it);else ++it;
  }
  std::unordered_set<void*> captured;
  std::unordered_set<void*> objects;
  for(const auto &l:lease.layers)objects.insert(l.object.target());
  for(const auto &r:lease.renderers)captured.insert(r.object.target());
  for(size_t n=0;n<count;++n) {
    void *renderer=nullptr;
    if(!mmd_api::Copy((char*)array+IL2CPP_ARRAY_DATA+n*sizeof(void*),&renderer,sizeof(renderer)))return fail();
    if(!UnityObjAlive(renderer)||captured.count(renderer))continue;
    if(Scope(renderer,root)!=1)return fail();
    Renderer entry;
    if(!mmd_api::Value(api.get,renderer,entry.original)||!entry.object.capture(renderer))return fail();
    lease.renderers.push_back(std::move(entry));captured.insert(renderer);
    void *object=nullptr;
    if(!mmd_api::Call(g_component_get_gameObject,renderer,nullptr,object)||!UnityObjAlive(object))return fail();
    if(objects.insert(object).second) {
      Layer layer;
      if(!mmd_api::Value(cullingApi.getLayer,object,layer.original)||layer.original<0||layer.original>31||
         !layer.object.capture(object))return fail();
      lease.layers.push_back(std::move(layer));
    }
  }
  if(lease.renderers.empty())return fail();
  return Hide(lease,root)?true:fail();
}
} // namespace mmd_visibility
