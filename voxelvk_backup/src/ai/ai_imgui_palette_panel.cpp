#include "ai_imgui_palette_panel.hpp"
#if defined(ENABLE_IMGUI_OVERLAY) && ENABLE_IMGUI_OVERLAY
# if __has_include("imgui.h")
#  include "imgui.h"
#  define VOXELVK_HAS_IMGUI 1
# endif
#endif
#include "../core/fullscreen_toggle.hpp"
#include "rag_runtime_bridge.hpp"

namespace voxelvk::ai {
#if VOXELVK_HAS_IMGUI
static bool InputU16(const char* label, uint16_t* v){ int tmp=(int)*v; bool changed=ImGui::InputInt(label,&tmp); if(tmp<0)tmp=0; if(tmp>65535)tmp=65535; *v=(uint16_t)tmp; return changed; }
bool DrawAIPalettePanel(PaletteRuntime& rt){
  if(!ImGui::Begin("AI Palette/World Settings")){ ImGui::End(); return false; }
  bool dirty=false; auto& ids=rt.cfg.ids;
  dirty|=InputU16("Air",&ids.Air); dirty|=InputU16("Water",&ids.Water); dirty|=InputU16("Sand",&ids.Sand);
  dirty|=InputU16("Grass",&ids.Grass); dirty|=InputU16("Dirt",&ids.Dirt); dirty|=InputU16("Stone",&ids.Stone);
  dirty|=InputU16("Snow",&ids.Snow); dirty|=InputU16("Wood",&ids.Wood); dirty|=InputU16("Leaves",&ids.Leaves); dirty|=InputU16("Clay",&ids.Clay);
  // RAG + Fullscreen UI additions
  ImGui::Separator();
  ImGui::Text("Display Settings");
  const bool is_fs = voxelvk::IsFullscreen();
  if(!is_fs){
    if(ImGui::Button("Go Fullscreen", ImVec2(200, 36))){ voxelvk::RequestFullscreen(true); ImGui::OpenPopup("Entered Fullscreen"); }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Switch to borderless fullscreen on the nearest monitor");
  } else {
    if(ImGui::Button("Exit Fullscreen", ImVec2(200, 36))){ voxelvk::RequestFullscreen(false); ImGui::OpenPopup("Exited Fullscreen"); }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Restore previous windowed position and size");
  }
  ImGui::SameLine();
  ImGui::TextColored(is_fs? ImVec4(0.3f,0.9f,0.3f,1):ImVec4(0.9f,0.9f,0.3f,1), "State: %s", is_fs? "Fullscreen" : "Windowed");
  if(ImGui::BeginPopup("Entered Fullscreen")) { ImGui::Text("Fullscreen ON"); ImGui::EndPopup(); }
  if(ImGui::BeginPopup("Exited Fullscreen")) { ImGui::Text("Fullscreen OFF"); ImGui::EndPopup(); }

  ImGui::Separator();
  ImGui::Text("RAG Settings");
  static bool enable_rag = false;
  static int rag_top_k = 3;
  if(ImGui::Checkbox("Enable RAG", &enable_rag)){
    UpdateRagConfig(enable_rag, rag_top_k);
  }
  if(ImGui::SliderInt("RAG top-k", &rag_top_k, 1, 8)){
    UpdateRagConfig(enable_rag, rag_top_k);
  }

  ImGui::End(); return dirty;
}
#else
bool DrawAIPalettePanel(PaletteRuntime&){ return false; }
#endif
} // ns