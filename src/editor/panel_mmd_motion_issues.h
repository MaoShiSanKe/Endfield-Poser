#pragma once
#include "game/mmd_squad.h"
#include "editor/panel_mmd_adaptation.h"

static bool s_motionIssuesShow=false;
static int s_motionIssuesTarget=0;
static std::string s_motionIssueTrack;
static const mmd::MotionIssues &MmdMotionIssuesFor(int target) {
  static const mmd::MotionIssues empty;
  if(target==0)return g_mmd.editedBody?empty:g_mmd.boneIssues;
  auto &slot=g_squad.slots[target-1];
  if(slot.issueRevision!=g_mmd.adaptationRevision) {
    slot.boneIssues=slot.edited?mmd::MotionIssues{}:mmd::AnalyzeMotionIssues(g_mmd.rig,slot.clip,g_mmd.adaptation);
    slot.issueRevision=g_mmd.adaptationRevision;
  }
  return slot.boneIssues;
}
static void DrawMmdMotionIssueBanner(int target) {
  const auto &issues=MmdMotionIssuesFor(target);
  if(!issues.unresolved.empty()) {
    ImGui::TextColored(ImVec4(1,.65f,.2f,1),u8"动作适配：%zu 项需要手动处理",issues.unresolved.size());
    ImGui::SameLine();
  }
  if(!issues.manuallyMapped.empty()) {
    ImGui::TextColored(ImVec4(.45f,.85f,.55f,1),u8"已手动映射 %zu 项",issues.manuallyMapped.size());
    ImGui::SameLine();
  }
  if(ImGui::SmallButton(issues.unresolved.empty()?u8"检查动作扩展骨…":u8"查看并处理…")) {
    s_motionIssuesShow=true;s_motionIssuesTarget=target;s_motionIssueTrack.clear();
  }
}
static void DrawMmdMotionIssuesPanel() {
  if(!s_motionIssuesShow)return;
  poser_ui::NextPanel(u8"动作扩展骨与导入检查",{630,160},{620,630},{450,350},g_resetPanelLayoutFrames>0);
  if(!ImGui::Begin(u8"动作扩展骨与导入检查",&s_motionIssuesShow,g_pinPanels?ImGuiWindowFlags_NoMove:0)) {ImGui::End();return;}
  if(ImGui::Combo(u8"检查动作",&s_motionIssuesTarget,u8"单人动作\0小队第 1 位\0小队第 2 位\0小队第 3 位\0小队第 4 位\0"))s_motionIssueTrack.clear();
  const auto &file=s_motionIssuesTarget==0?g_mmd.file:g_squad.slots[s_motionIssuesTarget-1].file;
  if(file.empty())ImGui::TextDisabled(u8"此位置尚未导入动作");
  else ImGui::TextWrapped("%s",file.c_str());
  const auto &issues=MmdMotionIssuesFor(s_motionIssuesTarget);
  ImGui::TextWrapped(u8"逐条检查有实际运动但尚未生效的轨道。骨架适配设置由单人和小队共用；每个动作分别检查。不按文件名或说明文件自动启用规则。");
  ImGui::TextColored(issues.unresolved.empty()?ImVec4(.45f,.85f,.55f,1):ImVec4(1,.65f,.2f,1),
                    u8"需要处理 %zu 项 / 已手动映射 %zu 项",issues.unresolved.size(),issues.manuallyMapped.size());
  ImGui::TextDisabled(u8"仅归零 %zu 项 / 已忽略 %zu 项",issues.neutral.size(),issues.ignored.size());
  static char filter[160]={};ImGui::InputText(u8"筛选轨道",filter,sizeof(filter));
  ImGui::BeginChild("##motionissues",poser_ui::Size(0,170),true);
  auto &edit=MmdEditAdaptation();
  for(const auto &item:issues.unresolved) {
    if(filter[0]&&item.track.find(filter)==std::string::npos)continue;
    ImGui::PushID(item.track.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.7f,.3f,1));
    const auto pending=edit.choices.find(item.track);
    const auto label=item.track+(issues.manuallyMapped.count(item.track)?u8"（已手动映射，需检查）":"")+
                     (pending!=edit.choices.end()&&pending->second.dirty?u8"（待应用）":"");
    if(ImGui::Selectable(label.c_str(),s_motionIssueTrack==item.track))s_motionIssueTrack=item.track;
    ImGui::PopStyleColor();
    ImGui::TextWrapped("%s",item.reason.c_str());ImGui::PopID();
  }
  for(const auto &mapping:issues.manuallyMapped) {
    if(filter[0]&&mapping.first.find(filter)==std::string::npos)continue;
    if(std::any_of(issues.unresolved.begin(),issues.unresolved.end(),[&](const mmd::BoneMotionIssue &v){return v.track==mapping.first;}))continue;
    ImGui::TextColored(ImVec4(.45f,.85f,.55f,1),u8"%s（已手动映射）",mapping.first.c_str());
    ImGui::TextWrapped(u8"目标源骨骼：%s",mapping.second.c_str());
  }
  if(issues.unresolved.empty())ImGui::TextWrapped(u8"未发现未处理的有效骨骼轨道；这不代表动作已完成画面验收。");
  ImGui::EndChild();
  auto selected=std::find_if(issues.unresolved.begin(),issues.unresolved.end(),[](const mmd::BoneMotionIssue &v){return v.track==s_motionIssueTrack;});
  if(selected!=issues.unresolved.end()) {
    const auto item=*selected; // Applying a change rebuilds the report.
    ImGui::TextWrapped(u8"%s：%s",item.track.c_str(),item.advice.c_str());
    if(item.keys)ImGui::Text(u8"首个待处理关键帧：第 %u 帧 / %.2f 秒；关键帧 %zu",item.firstFrame,item.firstFrame/30.,item.keys);
    auto [where,created]=edit.choices.try_emplace(item.track);
    auto &choice=where->second;auto &resolution=choice.value;auto &c=resolution.control;
    if(created) {
      c.side=item.track.find(u8"右")!=std::string::npos?1:0;
      resolution.action=item.kind==mmd::MotionIssueKind::Grip?2:item.kind==mmd::MotionIssueKind::Spread?3:
             item.kind==mmd::MotionIssueKind::ArmIk?4:item.kind==mmd::MotionIssueKind::Gaze?5:
             item.kind==mmd::MotionIssueKind::Parent?1:0;
    }
    auto &action=resolution.action;auto &destination=resolution.destination;
    const bool busy=g_mmd.session.active||g_mmd.loading||s_mmdStartRequest.active||MmdSquadBusy()||g_blenderEditing||g_mmd.preview;
    if(busy)ImGui::TextWrapped(u8"停止播放或断开 Blender 后可修改适配。");
    ImGui::BeginDisabled(busy);
    choice.dirty|=ImGui::Combo(u8"处理方式",&action,u8"绑定到已有源骨骼\0插入多段父级\0通用握拳控制\0通用张指控制\0手臂 IK 目标\0视线目标\0");
    if(action<=1) {
      choice.dirty|=MmdBoneChoice(action==0?u8"目标源骨骼":u8"插入到此骨骼之前",destination,u8"请选择实际控制部位");
      if(action==1)ImGui::TextWrapped(u8"父级初始位置与目标骨一致。不同旋转中心可在“高级 → 骨架适配”调整；已有同名骨需修正其连接关系。");
    } else {
      if(action!=5)choice.dirty|=ImGui::Combo(u8"控制哪只手",&c.side,u8"左手\0右手\0");
      if(action==2||action==3) {
        int finger=c.finger+1;
        if(ImGui::Combo(u8"手指范围",&finger,u8"全部手指\0拇指\0食指\0中指\0无名指\0小指\0")){c.finger=finger-1;choice.dirty=true;}
        choice.dirty|=ImGui::Combo(u8"动作输入轴",&c.inputAxis,"X\0Y\0Z\0");
        choice.dirty|=ImGui::SliderFloat(u8"控制比例（负值反向）",&c.gain,-3,3,"%.2f");
        ImGui::TextWrapped(u8"通用模板按手掌方向分配关节旋转，并非所有握拳插件的精确复刻。应用后逐帧核对轴向和比例。");
      }
      choice.dirty|=MmdBoneChoice(u8"控制骨父级",c.parent,u8"默认（手腕 / 总父级 / 头部）");
      if(action!=5)choice.dirty|=ImGui::Checkbox(u8"同时建立左右对应控制（无轨道的一侧不启用）",&c.paired);
      if(action==4)choice.dirty|=ImGui::Checkbox(u8"使用目标骨的手腕朝向",&c.wristRotation);
    }
    ImGui::BeginDisabled(action<=1&&destination.empty());
    if(ImGui::Button(u8"应用此处理并重新检查")) {
      choice.dirty=true;MmdCommitAdaptationDraft(item.track);
    }
    ImGui::EndDisabled();ImGui::SameLine();
    if(ImGui::Button(u8"明确忽略此轨道")) {
      auto next=edit.value;
      if(item.kind==mmd::MotionIssueKind::Binding) {
        for(auto i=next.tracks.begin();i!=next.tracks.end();)if(i->second==item.track)i=next.tracks.erase(i);else ++i;
      } else next.ignoredTracks.insert(item.track);
      if(MmdApplyAdaptation(next,g_mmd.sourcePreset)) {
        choice.dirty=false;edit.value=g_mmd.adaptation;edit.revision=g_mmd.adaptationRevision;edit.dirty=false;
      }
    }
    ImGui::EndDisabled();
  }
  if(ImGui::TreeNode(u8"仅归零的轨道（通常无需处理）")) {
    for(const auto &name:issues.neutral)ImGui::TextDisabled("%s",name.c_str());ImGui::TreePop();
  }
  if(!issues.replaced.empty()&&ImGui::TreeNode(u8"被手动映射替换的原轨道")) {
    for(const auto &name:issues.replaced)ImGui::TextWrapped("%s",name.c_str());ImGui::TreePop();
  }
  if(ImGui::TreeNode(u8"已忽略轨道")) {
    std::string restore;
    ImGui::BeginDisabled(g_mmd.session.active||g_mmd.loading||s_mmdStartRequest.active||MmdSquadBusy()||g_blenderEditing||g_mmd.preview);
    for(const auto &name:issues.ignored) {ImGui::PushID(name.c_str());ImGui::TextUnformatted(name.c_str());ImGui::SameLine();if(ImGui::SmallButton(u8"恢复检查"))restore=name;ImGui::PopID();}
    if(!restore.empty()) {
      auto next=edit.value;next.ignoredTracks.erase(restore);auto binding=next.tracks.find(restore);
      if(binding!=next.tracks.end()&&binding->second.empty())next.tracks.erase(binding);
      if(MmdApplyAdaptation(next,g_mmd.sourcePreset)){edit.value=g_mmd.adaptation;edit.revision=g_mmd.adaptationRevision;edit.dirty=false;}
    }
    ImGui::EndDisabled();ImGui::TreePop();
  }
  const bool busy=g_mmd.session.active||g_mmd.loading||s_mmdStartRequest.active||MmdSquadBusy()||g_blenderEditing||g_mmd.preview;
  if(MmdAdaptationDraftDirty())ImGui::TextColored(ImVec4(1,.7f,.3f,1),u8"有待应用的选择；保存时会一并应用并重新检查");
  ImGui::BeginDisabled(g_mmd.loading||(MmdAdaptationDraftDirty()&&busy));
  if(ImGui::Button(MmdAdaptationDraftDirty()?u8"应用并保存适配预设…":u8"保存适配预设…")) {
    if(!MmdAdaptationDraftDirty()||MmdCommitAdaptationDraft())MmdBeginLoad(4);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(g_mmd.session.active||g_mmd.loading||s_mmdStartRequest.active||MmdSquadBusy()||g_blenderEditing||g_mmd.preview);
  if(ImGui::Button(u8"载入适配预设…"))MmdBeginLoad(3);
  ImGui::EndDisabled();
  if(!g_mmd.adaptationFile.empty()) {
    ImGui::TextWrapped(u8"预设文件：%s",g_mmd.adaptationFile.c_str());
    if(g_mmd.adaptationSavedRevision!=g_mmd.adaptationRevision)ImGui::TextColored(ImVec4(1,.7f,.3f,1),u8"当前适配已修改，请重新保存");
  }
  if(!g_mmd.adaptationRestoreStatus.empty())ImGui::TextWrapped("%s",g_mmd.adaptationRestoreStatus.c_str());
  ImGui::TextWrapped(u8"保存会包含已选择但尚未应用的处理。重启后自动恢复最近一次成功保存 / 载入的预设。");
  ImGui::TextWrapped(u8"已启用控制的参数与删除：MMD 播放器 → 高级 → 骨架适配 → 动作扩展骨。");
  ImGui::TextWrapped("%s",g_mmd.status.c_str());
  ImGui::End();
}
