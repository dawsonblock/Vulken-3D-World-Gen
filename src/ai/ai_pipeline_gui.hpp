#pragma once
#include <functional>
#include <string>

namespace voxelvk::ai {

/**
 * AI Pipeline GUI Components
 * Provides modern ImGui interfaces for AI pipeline management, training, and monitoring.
 * This is a stub implementation preparing for Phase 9 (AI Pipeline Implementation)
 */

struct PipelineStatus {
    bool isInitialized = false;
    bool isTraining = false;
    bool isTensorRTEnabled = false;
    int trainingEpoch = 0;
    int maxEpochs = 100;
    float trainingLoss = 0.0f;
    float validationAccuracy = 0.0f;
    std::string currentModel = "None";
    std::string lastError = "";
    
    // Model performance metrics
    float inferenceLatency = 0.0f;  // ms
    float throughput = 0.0f;        // inferences/sec
    float memoryUsage = 0.0f;       // MB
    float gpuUtilization = 0.0f;    // %
};

struct ModelConfig {
    std::string modelType = "VoxelMesher";
    int batchSize = 32;
    float learningRate = 0.001f;
    bool useGPU = true;
    bool useTensorRT = false;
    bool enableMixedPrecision = false;
    int maxBatchSize = 64;
    std::string datasetPath = "datasets/voxel_training";
};

// Main GUI rendering function
bool RenderAIPipelinePanel(PipelineStatus& status, ModelConfig& config);

// Individual panel components
void RenderTrainingPanel(PipelineStatus& status, ModelConfig& config);
void RenderModelManagementPanel(PipelineStatus& status, ModelConfig& config);
void RenderInferenceMonitoringPanel(PipelineStatus& status);
void RenderDatasetPanel(ModelConfig& config);

// Utility functions for modern UI elements
void RenderProgressIndicator(const char* label, float progress, const char* status = nullptr);
void RenderMetricCard(const char* title, float value, const char* unit, float maxValue = 0.0f);
void RenderStatusBadge(const char* text, bool isActive);

} // namespace voxelvk::ai