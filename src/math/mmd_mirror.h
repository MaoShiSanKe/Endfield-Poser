#pragma once
#include "quat_math.h"
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

namespace mmd {
// Reflect across the character's sagittal plane, not screen/world X. Keep a
// proper rotation and positive bone scales; never mirror the rendered mesh.
inline Vec3 MirrorX(Vec3 p) { return {-p.x,p.y,p.z}; }
inline Quat MirrorX(Quat q) { return {q.x,-q.y,-q.z,q.w}; }
inline int MirrorRole(int r) {
  if(r>=24&&r<=53)return r<39?r+15:r-15;
  for(int left:{1,3,5,11,13,15,17,19,21})if(r==left||r==left+1)return r==left?r+1:r-1;
  return r;
}
inline std::string MirrorBoneName(std::string name) {
  // MMD side prefixes also cover auxiliary/IK and semi-standard bones.
  auto l=name.find(u8"左"),r=name.find(u8"右");
  if(l!=std::string::npos&&(r==std::string::npos||l<r)){name.replace(l,3,u8"右");return name;}
  if(r!=std::string::npos){name.replace(r,3,u8"左");return name;}
  // Native Bip001_L/R_... and Blender .L/.R names; no arbitrary substring swap.
  for(const auto &pair:{std::pair<const char*,const char*>{"_L_","_R_"},{" L "," R "},{"_l_","_r_"}}) {
    auto at=name.find(pair.first);if(at!=std::string::npos){name.replace(at,3,pair.second);return name;}
    at=name.find(pair.second);if(at!=std::string::npos){name.replace(at,3,pair.first);return name;}
  }
  if(name.rfind("Bip",0)==0)for(size_t i=0;i+2<name.size();++i)
    if(name[i]=='_'&&(name[i+1]=='L'||name[i+1]=='R')&&name[i+2]>='A'&&name[i+2]<='Z') {name[i+1]=name[i+1]=='L'?'R':'L';return name;}
  if(name.size()>2&&(name[name.size()-2]=='.'||name[name.size()-2]=='_')) {
    char &c=name.back();if(c=='L')c='R';else if(c=='R')c='L';else if(c=='l')c='r';else if(c=='r')c='l';
  }
  return name;
}
// Already-retargeted exports carry native local transforms. Transfer each
// opposite joint's bind-relative delta through its calibrated parent frame.
// The destination retains its own bone lengths and asymmetric bind pose.
template<class Profile>
void MirrorNative(const Profile &rest,Profile &pose,std::vector<Quat> &rotations,
                  std::vector<bool> &writes,Quat body,std::vector<bool> *scaleTracks=nullptr,bool enabled=true) {
  const auto original=pose;const auto q=rotations;const auto w=writes;
  const auto scales=scaleTracks?*scaleTracks:std::vector<bool>{};
  std::vector<int> pairs(rest.bones.size(),-1);
  std::unordered_map<std::string,int> names;
  for(size_t i=0;i<rest.bones.size();++i){auto inserted=names.emplace(rest.bones[i].name,int(i));if(!inserted.second)inserted.first->second=-1;}
  for(size_t i=0;i<rest.bones.size();++i) {
    const auto &b=rest.bones[i];int other=-1;
    if(b.role>=0&&b.role<55)other=rest.roles[MirrorRole(b.role)];
    else {
      const auto name=MirrorBoneName(b.name);
      if(name==b.name)other=int(i);
      else {auto at=names.find(name);if(at!=names.end())other=at->second;}
    }
    pairs[i]=other;
  }
  for(size_t i=0;i<rest.bones.size();++i) {
    const int j=pairs[i];const auto &dst=rest.bones[i];
    if(!enabled){if(j>=0&&size_t(j)<w.size()){writes[i]=w[i]||w[j];if(scaleTracks)(*scaleTracks)[i]=scales[i]||scales[j];}continue;}
    // An unpaired helper has no safe contralateral target. Leave its bind pose.
    if(j<0||size_t(j)>=q.size()||size_t(j)>=w.size()) {
      rotations[i]=dst.localRot;pose.bones[i].localPos=dst.localPos;pose.bones[i].localScale=dst.localScale;writes[i]=w[i];
      if(scaleTracks)(*scaleTracks)[i]=scales[i];continue;
    }
    const auto &src=rest.bones[j];
    const Quat from=Conj(body)*(src.parent>=0?rest.bones[src.parent].restRot:Quat{});
    const Quat to=Conj(body)*(dst.parent>=0?rest.bones[dst.parent].restRot:Quat{});
    const Quat delta=NormQ(from*q[j]*Conj(src.localRot)*Conj(from));
    rotations[i]=NormQ(Conj(to)*MirrorX(delta)*to*dst.localRot);
    pose.bones[i].localPos=dst.localPos+Conj(to)*MirrorX(from*(original.bones[j].localPos-src.localPos));
    const auto a=original.bones[j].localScale,b=src.localScale,c=dst.localScale;
    // Some hidden accessories legitimately have a zero bind scale. A missing
    // scale axis has no transferable ratio; preserve the destination bind axis.
    auto ratio=[](float value,float base){return std::fabs(base)>1e-6f?value/base:1.f;};
    pose.bones[i].localScale={c.x*ratio(a.x,b.x),c.y*ratio(a.y,b.y),c.z*ratio(a.z,b.z)};
    writes[i]=w[i]||w[j];if(scaleTracks)(*scaleTracks)[i]=scales[i]||scales[j];
  }
}
} // namespace mmd
