#pragma once
#include "math/mmd_rig.h"

namespace mmd {
enum class MotionControlKind { Grip, Spread, ArmIk, Gaze };
struct MotionControl {
  std::string name, parent;
  MotionControlKind kind = MotionControlKind::Grip;
  int side = 0, finger = -1, inputAxis = 0;
  float gain = 1;
  bool paired = true, wristRotation = true;
};
inline void AddMotionControl(RigDefinition &r, const MotionControl &c) {
  auto require = [&](const std::string &name) {
    int i=r.find(name);if(i<0)throw std::runtime_error(u8"扩展骨所需骨骼不存在："+name);return i;
  };
  if(r.find(c.name)>=0)throw std::runtime_error(u8"控制骨已存在，请使用原结构或改用轨道绑定："+c.name);
  const std::string side=c.side?u8"右":u8"左";
  const bool hand=c.kind==MotionControlKind::Grip||c.kind==MotionControlKind::Spread;
  const int anchor=require(c.kind==MotionControlKind::Gaze?u8"両目":side+u8"手首");
  RigBone control;control.name=Name(c.name);control.rest=r.bones[anchor].rest;
  control.parent=c.parent.empty()?(hand?anchor:require(c.kind==MotionControlKind::Gaze?u8"頭":u8"全ての親")):require(c.parent);
  control.motionControl=true;
  if(c.kind==MotionControlKind::Gaze)control.rest=control.rest+Vec3{0,0,-10};
  const int index=int(r.bones.size());r.names[control.name]=index;r.bones.push_back(control);
  if(hand) {
    const char *fingers[]={u8"親指",u8"人指",u8"中指",u8"薬指",u8"小指"};
    const Vec3 forward=Norm(r.bones[require(side+u8"中指1")].rest-r.bones[anchor].rest);
    Vec3 across=Norm(r.bones[require(side+u8"人指1")].rest-r.bones[require(side+u8"小指1")].rest);
    if(Len(across)<.01f||std::fabs(Dot(across,forward))>.95f)across={0,0,-1};
    Vec3 normal=Norm(Cross(forward,across));
    if(Dot(normal,Vec3{0,0,-1})<0)normal=normal*-1;
    // Configurable anatomical approximation. Plugin-specific rigs can instead
    // be described with explicit inserted bones and append relationships.
    for(int f=0;f<5;++f) {
      if(c.finger>=0&&c.finger!=f)continue;
      for(int j=0;j<3;++j) {
        const int bone=require(side+fingers[f]+std::to_string(f==0?j:j+1));
        Vec3 axis;float weight=c.gain;
        if(c.kind==MotionControlKind::Spread) {
          if(j!=0)continue;
          static const float spread[]={-.65f,-.35f,0,.25f,.5f};
          axis=normal;weight*=spread[f]*(c.side?-1.f:1.f);
        } else {
          Vec3 direction=forward;
          if(j<2)direction=Norm(r.bones[require(side+fingers[f]+std::to_string(f==0?j+1:j+2))].rest-r.bones[bone].rest);
          axis=Norm(Cross(direction,normal));
          if(f==0)weight*=.55f;
          else if(j==0)weight*=.75f;
        }
        r.bones[bone].drivers.push_back({index,c.inputAxis,axis,weight});
      }
    }
  } else if(c.kind==MotionControlKind::ArmIk) {
    auto &b=r.bones[index];b.controlIk=true;b.copyIkRotation=c.wristRotation;
    b.effector=anchor;b.iterations=32;b.angleLimit=.35f;
    b.links={{require(side+u8"ひじ"),false,{},{}},{require(side+u8"腕"),false,{},{}}};
  } else {
    for(const auto *eye:{u8"左目",u8"右目"})r.aims.push_back({index,require(eye)});
  }
}
inline void AddMotionControls(RigDefinition &r,const std::vector<MotionControl> &controls) {
  for(const auto &c:controls) {
    AddMotionControl(r,c);
    auto other=MirrorBoneName(c.name);
    if(c.paired&&c.kind!=MotionControlKind::Gaze&&other!=c.name) {
      auto mirrored=c;mirrored.name=other;mirrored.side=1-c.side;
      mirrored.parent=MirrorBoneName(c.parent);
      AddMotionControl(r,mirrored);
    }
  }
}
} // namespace mmd
