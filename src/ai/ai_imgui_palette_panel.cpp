#include "ai_imgui_palette_panel.hpp"
#if defined(ENABLE_IMGUI_OVERLAY) && ENABLE_IMGUI_OVERLAY
# if __has_include("imgui.h")
#  include "imgui.h"
#  define VOXELVK_HAS_IMGUI 1
# endif
#endif
#include "../core/fullscreen_toggle.hpp"
#include "rag_runtime_bridge.hpp"
#include <cmath>

namespace voxelvk::ai {
#if VOXELVK_HAS_IMGUI

// Modern UI helpers
namespace {
    constexpr float SECTION_SPACING = 12.0f;
    constexpr float ITEM_SPACING = 8.0f;
    
    void HelpMarker(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
    
    bool InputU16WithValidation(const char* label, uint16_t* v, const char* help = nullptr) {
        int tmp = (int)*v;
        bool changed = ImGui::InputInt(label, &tmp, 1, 10, ImGuiInputTextFlags_CharsDecimal);
        
        if (help) {
            ImGui::SameLine();
            HelpMarker(help);
        }
        
        if (changed) {
            if (tmp < 0) tmp = 0;
            if (tmp > 65535) tmp = 65535;
            *v = (uint16_t)tmp;
        }
        
        return changed;
    }
    
    void RenderBlockPaletteSection(PaletteRuntime& rt, bool& dirty) {
        if (ImGui::CollapsingHeader("Block Palette Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            
            auto& ids = rt.cfg.ids;
            
            // Primary blocks in a 2-column layout
            ImGui::Columns(2, "PrimaryBlocks", false);
            
            dirty |= InputU16WithValidation("Air Block ID", &ids.Air, "ID for air/empty blocks");
            dirty |= InputU16WithValidation("Water Block ID", &ids.Water, "ID for water blocks");
            dirty |= InputU16WithValidation("Sand Block ID", &ids.Sand, "ID for sand blocks");
            dirty |= InputU16WithValidation("Grass Block ID", &ids.Grass, "ID for grass surface blocks");
            dirty |= InputU16WithValidation("Dirt Block ID", &ids.Dirt, "ID for dirt/soil blocks");
            
            ImGui::NextColumn();
            
            dirty |= InputU16WithValidation("Stone Block ID", &ids.Stone, "ID for stone blocks");
            dirty |= InputU16WithValidation("Snow Block ID", &ids.Snow, "ID for snow blocks");
            dirty |= InputU16WithValidation("Wood Block ID", &ids.Wood, "ID for wood/trunk blocks");
            dirty |= InputU16WithValidation("Leaves Block ID", &ids.Leaves, "ID for foliage blocks");
            dirty |= InputU16WithValidation("Clay Block ID", &ids.Clay, "ID for clay blocks");
            
            ImGui::Columns(1);
            ImGui::Unindent();
        }
    }
    
    void RenderDisplayControlsSection() {
        if (ImGui::CollapsingHeader("Display Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            
            const bool is_fs = voxelvk::IsFullscreen();
            
            // Modern fullscreen toggle with better UX
            ImVec4 buttonColor = is_fs ? ImVec4(0.8f, 0.3f, 0.3f, 1.0f) : ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            
            if (ImGui::Button(is_fs ? "Exit Fullscreen" : "Enter Fullscreen", ImVec2(200, 36))) {
                voxelvk::RequestFullscreen(!is_fs);
            }
            
            ImGui::PopStyleColor();
            
            ImGui::SameLine();
            ImGui::TextColored(is_fs ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.9f, 0.3f, 1.0f), 
                             "%s Mode", is_fs ? "Fullscreen" : "Windowed");
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Current display mode. Click button to toggle.");
            }
            
            ImGui::Unindent();
        }
    }
    
    void RenderRAGConfigurationSection(bool& ragStateChanged) {
        if (ImGui::CollapsingHeader("AI/RAG Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            
            static bool enable_rag = false;
            static int rag_top_k = 3;
            static float rag_confidence_threshold = 0.7f;
            static bool show_advanced = false;
            
            // Main RAG controls
            if (ImGui::Checkbox("Enable RAG (Retrieval-Augmented Generation)", &enable_rag)) {
                UpdateRagConfig(enable_rag, rag_top_k);
                ragStateChanged = true;
            }
            ImGui::SameLine();
            HelpMarker("Enable Retrieval-Augmented Generation for AI-powered world generation");
            
            ImGui::BeginDisabled(!enable_rag);
            
            if (ImGui::SliderInt("Top-K Results", &rag_top_k, 1, 16, "%d")) {
                UpdateRagConfig(enable_rag, rag_top_k);
                ragStateChanged = true;
            }
            ImGui::SameLine();
            HelpMarker("Number of top results to consider from the knowledge base");
            
            // Advanced settings toggle
            ImGui::Checkbox("Advanced Settings", &show_advanced);
            
            if (show_advanced) {
                ImGui::Indent();
                ImGui::SliderFloat("Confidence Threshold", &rag_confidence_threshold, 0.0f, 1.0f, "%.2f");
                ImGui::SameLine();
                HelpMarker("Minimum confidence score for RAG results");
                ImGui::Unindent();
            }
            
            ImGui::EndDisabled();
            
            // Status indicator
            ImGui::Spacing();
            ImGui::Text("Status: ");
            ImGui::SameLine();
            ImGui::TextColored(enable_rag ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                             enable_rag ? "RAG Active" : "RAG Disabled");
            
            ImGui::Unindent();
        }
    }
    
    void RenderAIGenerationSettings(PaletteRuntime& rt, bool& dirty) {
        if (ImGui::CollapsingHeader("AI Generation Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Indent();
            
            auto& ai = rt.cfg.ai_generation;
            
            // AI feature toggles
            dirty |= ImGui::Checkbox("AI Structures", &ai.enable_ai_structures);
            ImGui::SameLine();
            HelpMarker("Enable AI-generated structures and buildings");
            
            dirty |= ImGui::Checkbox("AI Textures", &ai.enable_ai_textures);
            ImGui::SameLine();
            HelpMarker("Enable AI-generated texture variations");
            
            dirty |= ImGui::Checkbox("Dynamic Biomes", &ai.enable_dynamic_biomes);
            ImGui::SameLine();
            HelpMarker("Enable AI-driven biome transitions and variations");
            
            ImGui::Spacing();
            
            // Quality and density sliders
            if (ImGui::SliderFloat("Structure Density", &ai.structure_density, 0.0f, 1.0f, "%.2f")) {
                dirty = true;
            }
            ImGui::SameLine();
            HelpMarker("Controls how many AI structures are generated");
            
            if (ImGui::SliderFloat("Terrain Variation", &ai.terrain_variation, 0.5f, 2.0f, "%.1f")) {
                dirty = true;
            }
            ImGui::SameLine();
            HelpMarker("Controls the amount of AI-driven terrain variation");
            
            if (ImGui::SliderFloat("Generation Quality", &ai.ai_generation_quality, 0.1f, 1.0f, "%.2f")) {
                dirty = true;
            }
            ImGui::SameLine();
            HelpMarker("Higher values use more compute for better quality");
            
            ImGui::Unindent();
        }
    }
}

bool DrawAIPalettePanel(PaletteRuntime& rt) {
    if (!ImGui::Begin("AI Palette & World Generation", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return false;
    }
    
    bool dirty = false;
    bool ragStateChanged = false;
    
    // Menu bar for quick actions
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Actions")) {
            if (ImGui::MenuItem("Reset to Defaults")) {
                // Reset palette to defaults
                rt.cfg = PaletteConfig{}; // This will use default constructor values
                dirty = true;
            }
            if (ImGui::MenuItem("Export Config")) {
                rt.save_to_file("exported_palette.cfg");
            }
            if (ImGui::MenuItem("Reload from File")) {
                rt.load_from_file(rt.path);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Status bar at top
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Configuration: %s", rt.path.c_str());
    ImGui::SameLine();
    if (dirty || ragStateChanged) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "(Modified)");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "(Saved)");
    }
    
    ImGui::Separator();
    
    // Main configuration sections
    RenderBlockPaletteSection(rt, dirty);
    ImGui::Spacing();
    
    RenderDisplayControlsSection();
    ImGui::Spacing();
    
    RenderRAGConfigurationSection(ragStateChanged);
    ImGui::Spacing();
    
    RenderAIGenerationSettings(rt, dirty);
    
    // Save prompt at bottom
    if (dirty) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Configuration has unsaved changes");
        if (ImGui::Button("Save Changes", ImVec2(120, 30))) {
            rt.save_to_file(rt.path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(80, 30))) {
            rt.load_from_file(rt.path);
            dirty = false;
        }
    }
    
    ImGui::End();
    return dirty;
}

#else
bool DrawAIPalettePanel(PaletteRuntime&) { return false; }
#endif
} // namespace voxelvk::ai