#include "ai_pipeline_gui.hpp"

#if __has_include("imgui.h")
#include "imgui.h"
#define VOXELVK_HAS_IMGUI 1
#endif

#include <vector>
#include <string>
#include <cmath>
#include <chrono>

namespace voxelvk::ai {

#if VOXELVK_HAS_IMGUI

namespace {
    // Modern UI styling helpers
    void PushCardStyle() {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));
    }
    
    void PopCardStyle() {
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
    
    void HelpTooltip(const char* desc) {
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip()) {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }
    
    // Simulate training metrics for demo
    void UpdateDemoMetrics(PipelineStatus& status) {
        static auto startTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        
        if (status.isTraining) {
            float progress = (elapsed * 0.0001f);
            status.trainingEpoch = (int)(progress * status.maxEpochs) % status.maxEpochs;
            status.trainingLoss = 1.0f - (progress * 0.8f) + 0.1f * sin(elapsed * 0.001f);
            status.validationAccuracy = 0.3f + 0.6f * progress + 0.05f * cos(elapsed * 0.0008f);
            
            if (status.trainingLoss < 0.1f) status.trainingLoss = 0.1f;
            if (status.validationAccuracy > 0.95f) status.validationAccuracy = 0.95f;
        }
        
        // Simulate inference metrics
        status.inferenceLatency = 12.5f + 3.0f * sin(elapsed * 0.0005f);
        status.throughput = 1000.0f / status.inferenceLatency;
        status.memoryUsage = 512.0f + 128.0f * cos(elapsed * 0.0003f);
        status.gpuUtilization = 45.0f + 25.0f * sin(elapsed * 0.0007f);
    }
}

void RenderProgressIndicator(const char* label, float progress, const char* status) {
    ImGui::Text("%s", label);
    
    // Custom progress bar with gradient
    ImVec4 progressColor = progress < 0.5f ? 
        ImVec4(0.9f, 0.7f, 0.3f, 1.0f) : ImVec4(0.3f, 0.9f, 0.5f, 1.0f);
    
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
    ImGui::ProgressBar(progress, ImVec2(-1, 24), "");
    ImGui::PopStyleColor();
    
    if (status) {
        ImGui::SameLine();
        ImGui::Text("(%s)", status);
    }
}

void RenderMetricCard(const char* title, float value, const char* unit, float maxValue) {
    PushCardStyle();
    ImGui::BeginChild(title, ImVec2(120, 80), true);
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", title);
    
    // Large value display
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assume default font is larger
    ImGui::Text("%.1f", value);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Text("%s", unit);
    
    // Optional progress bar for values with known max
    if (maxValue > 0.0f) {
        float progress = std::min(value / maxValue, 1.0f);
        ImVec4 color = progress < 0.7f ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f) : 
                      progress < 0.9f ? ImVec4(0.9f, 0.9f, 0.3f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        ImGui::ProgressBar(progress, ImVec2(-1, 4), "");
        ImGui::PopStyleColor();
    }
    
    ImGui::EndChild();
    PopCardStyle();
}

void RenderStatusBadge(const char* text, bool isActive) {
    ImVec4 bgColor = isActive ? ImVec4(0.2f, 0.8f, 0.2f, 0.3f) : ImVec4(0.6f, 0.6f, 0.6f, 0.3f);
    ImVec4 textColor = isActive ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    
    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bgColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    
    ImGui::Button(text, ImVec2(0, 24));
    
    ImGui::PopStyleColor(4);
}

void RenderTrainingPanel(PipelineStatus& status, ModelConfig& config) {
    if (ImGui::CollapsingHeader("Training & Optimization", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        // Training status
        ImGui::Text("Training Status:");
        ImGui::SameLine();
        RenderStatusBadge(status.isTraining ? "TRAINING" : "IDLE", status.isTraining);
        
        if (!status.isTraining) {
            if (ImGui::Button("Start Training", ImVec2(120, 30))) {
                status.isTraining = true;
                status.trainingEpoch = 0;
                status.lastError.clear();
            }
        } else {
            if (ImGui::Button("Stop Training", ImVec2(120, 30))) {
                status.isTraining = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Pause", ImVec2(80, 30))) {
                // Pause training (stub)
            }
        }
        
        ImGui::Spacing();
        
        // Training progress
        if (status.isTraining) {
            float progress = (float)status.trainingEpoch / (float)status.maxEpochs;
            char progressText[64];
            snprintf(progressText, sizeof(progressText), "Epoch %d/%d", status.trainingEpoch, status.maxEpochs);
            RenderProgressIndicator("Training Progress", progress, progressText);
            
            ImGui::Spacing();
            
            // Training metrics in columns
            ImGui::Columns(2, "TrainingMetrics", false);
            RenderMetricCard("Loss", status.trainingLoss, "", 2.0f);
            ImGui::NextColumn();
            RenderMetricCard("Accuracy", status.validationAccuracy * 100.0f, "%", 100.0f);
            ImGui::Columns(1);
        }
        
        ImGui::Spacing();
        
        // Training configuration
        ImGui::Text("Configuration:");
        ImGui::SliderInt("Batch Size", &config.batchSize, 1, 128);
        ImGui::SliderFloat("Learning Rate", &config.learningRate, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic);
        
        ImGui::Checkbox("Use GPU", &config.useGPU);
        ImGui::SameLine();
        HelpTooltip("Enable GPU acceleration for training");
        
        if (config.useGPU) {
            ImGui::Checkbox("TensorRT Optimization", &config.useTensorRT);
            ImGui::SameLine();
            HelpTooltip("Enable TensorRT optimization for inference (requires NVIDIA GPU)");
            
            ImGui::Checkbox("Mixed Precision", &config.enableMixedPrecision);
            ImGui::SameLine();
            HelpTooltip("Use FP16 mixed precision training to speed up training and reduce memory usage");
        }
        
        ImGui::Unindent();
    }
}

void RenderModelManagementPanel(PipelineStatus& status, ModelConfig& config) {
    if (ImGui::CollapsingHeader("Model Management", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        // Current model info
        ImGui::Text("Active Model: %s", status.currentModel.c_str());
        ImGui::SameLine();
        RenderStatusBadge(status.isInitialized ? "LOADED" : "NOT LOADED", status.isInitialized);
        
        ImGui::Spacing();
        
        // Model type selection
        const char* modelTypes[] = { "VoxelMesher", "TerrainGen", "BiomeClassifier", "StructureGen" };
        static int currentModel = 0;
        if (ImGui::Combo("Model Type", &currentModel, modelTypes, IM_ARRAYSIZE(modelTypes))) {
            config.modelType = modelTypes[currentModel];
        }
        
        ImGui::Spacing();
        
        // Model actions
        if (ImGui::Button("Load Model", ImVec2(100, 30))) {
            status.currentModel = config.modelType;
            status.isInitialized = true;
            status.lastError.clear();
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Save Model", ImVec2(100, 30))) {
            // Save model (stub)
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Export ONNX", ImVec2(100, 30))) {
            // Export to ONNX (stub)
        }
        
        ImGui::Spacing();
        
        // Model performance optimization
        ImGui::Text("Optimization Settings:");
        ImGui::SliderInt("Max Batch Size", &config.maxBatchSize, 1, 256);
        ImGui::SameLine();
        HelpTooltip("Maximum batch size for inference optimization");
        
        if (status.lastError.length() > 0) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", status.lastError.c_str());
        }
        
        ImGui::Unindent();
    }
}

void RenderInferenceMonitoringPanel(PipelineStatus& status) {
    if (ImGui::CollapsingHeader("Inference Monitoring", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        // Performance metrics in a grid
        ImGui::Text("Real-time Performance:");
        ImGui::Columns(4, "InferenceMetrics", false);
        
        RenderMetricCard("Latency", status.inferenceLatency, "ms", 50.0f);
        ImGui::NextColumn();
        
        RenderMetricCard("Throughput", status.throughput, "/s", 100.0f);
        ImGui::NextColumn();
        
        RenderMetricCard("GPU Usage", status.gpuUtilization, "%", 100.0f);
        ImGui::NextColumn();
        
        RenderMetricCard("Memory", status.memoryUsage, "MB", 1024.0f);
        ImGui::Columns(1);
        
        ImGui::Spacing();
        
        // Status indicators
        ImGui::Text("System Status:");
        ImGui::BulletText("Model: %s", status.isInitialized ? "Ready" : "Not Loaded");
        ImGui::BulletText("TensorRT: %s", status.isTensorRTEnabled ? "Enabled" : "Disabled");
        ImGui::BulletText("Batch Processing: %s", "Available");
        
        ImGui::Unindent();
    }
}

void RenderDatasetPanel(ModelConfig& config) {
    if (ImGui::CollapsingHeader("Dataset Management", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        ImGui::Text("Dataset Path:");
        static char datasetPath[256];
        strncpy(datasetPath, config.datasetPath.c_str(), sizeof(datasetPath) - 1);
        if (ImGui::InputText("##DatasetPath", datasetPath, sizeof(datasetPath))) {
            config.datasetPath = datasetPath;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse")) {
            // Open file browser (stub)
        }
        
        ImGui::Spacing();
        
        // Dataset statistics (mock)
        ImGui::Text("Dataset Statistics:");
        ImGui::Indent();
        ImGui::BulletText("Training Samples: 12,543");
        ImGui::BulletText("Validation Samples: 3,186");
        ImGui::BulletText("Test Samples: 1,271");
        ImGui::BulletText("Data Size: 2.4 GB");
        ImGui::Unindent();
        
        ImGui::Spacing();
        
        // Dataset actions
        if (ImGui::Button("Refresh Dataset", ImVec2(120, 30))) {
            // Refresh dataset (stub)
        }
        ImGui::SameLine();
        if (ImGui::Button("Validate Data", ImVec2(120, 30))) {
            // Validate dataset (stub)
        }
        
        ImGui::Unindent();
    }
}

bool RenderAIPipelinePanel(PipelineStatus& status, ModelConfig& config) {
    if (!ImGui::Begin("AI Pipeline Control", nullptr, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return false;
    }
    
    // Update demo metrics
    UpdateDemoMetrics(status);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Pipeline")) {
            if (ImGui::MenuItem("Initialize", nullptr, false, !status.isInitialized)) {
                status.isInitialized = true;
                status.currentModel = config.modelType;
            }
            if (ImGui::MenuItem("Reset", nullptr, false, status.isInitialized)) {
                status.isInitialized = false;
                status.isTraining = false;
                status.currentModel = "None";
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Export")) {
            ImGui::MenuItem("Training Logs");
            ImGui::MenuItem("Model Weights");
            ImGui::MenuItem("Performance Report");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Status overview
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "AI Pipeline Status");
    ImGui::SameLine();
    RenderStatusBadge(status.isInitialized ? "READY" : "OFFLINE", status.isInitialized);
    ImGui::Separator();
    
    // Main panels
    RenderTrainingPanel(status, config);
    RenderModelManagementPanel(status, config);
    RenderInferenceMonitoringPanel(status);
    RenderDatasetPanel(config);
    
    ImGui::End();
    return true;
}

#else
// No ImGui stubs
bool RenderAIPipelinePanel(PipelineStatus&, ModelConfig&) { return false; }
void RenderTrainingPanel(PipelineStatus&, ModelConfig&) {}
void RenderModelManagementPanel(PipelineStatus&, ModelConfig&) {}
void RenderInferenceMonitoringPanel(PipelineStatus&) {}
void RenderDatasetPanel(ModelConfig&) {}
void RenderProgressIndicator(const char*, float, const char*) {}
void RenderMetricCard(const char*, float, const char*, float) {}
void RenderStatusBadge(const char*, bool) {}
#endif

} // namespace voxelvk::ai