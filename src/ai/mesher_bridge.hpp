/**
 * AI Mesher Bridge - Interface for AI-enhanced meshing
 * ====================================================
 * 
 * Provides integration point for AI models to optimize voxel meshing.
 * Can operate in metrics-only mode when AI models are not available.
 */

#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <optional>

namespace VulkenAI {

    struct VoxelChunkData {
        std::vector<uint8_t> voxels;
        uint32_t chunkSize = 64;
        float voxelScale = 1.0f;
        
        // Neighbor chunk hints for edge cases
        bool hasNeighborXPos = false;
        bool hasNeighborXNeg = false;  
        bool hasNeighborYPos = false;
        bool hasNeighborYNeg = false;
        bool hasNeighborZPos = false;
        bool hasNeighborZNeg = false;
    };

    struct MeshOptimizationHint {
        float lodBias = 1.0f;           // Level-of-detail bias
        float vertexReduction = 0.0f;   // Suggested vertex reduction (0-1)
        bool useGreedyMerging = true;   // Enable greedy meshing
        bool enableBackfaceCull = true; // Enable backface culling optimization
        
        // Performance predictions
        float expectedVertexCount = 0.0f;
        float expectedRenderTime = 16.67f;  // Expected frame time in ms
    };

    struct MesherMetrics {
        std::chrono::microseconds processingTime{0};
        uint32_t inputVoxelCount = 0;
        uint32_t outputVertexCount = 0;
        uint32_t outputIndexCount = 0;
        float optimizationRatio = 1.0f;  // Output/Input complexity ratio
        bool usedAI = false;
        bool fallbackUsed = false;
        std::string fallbackReason;
    };

    /**
     * AI Mesher Bridge Interface
     * 
     * Provides a clean interface for AI-enhanced meshing while maintaining
     * compatibility when AI models are not available.
     */
    class MesherBridge {
    public:
        virtual ~MesherBridge() = default;
        
        /**
         * Process a voxel chunk and generate optimization hints
         */
        virtual MeshOptimizationHint processChunk(const VoxelChunkData& chunk) = 0;
        
        /**
         * Record metrics for model training and analysis
         */
        virtual void recordMetrics(const MesherMetrics& metrics) = 0;
        
        /**
         * Get current AI model status
         */
        virtual bool isAIEnabled() const = 0;
        virtual std::string getModelVersion() const = 0;
        virtual float getModelAccuracy() const = 0;
        
        /**
         * Export collected metrics for analysis
         */
        virtual void exportMetrics(const std::string& filepath) = 0;
    };

    /**
     * Metrics-Only Implementation
     * 
     * Records performance metrics without AI processing.
     * Used when AI models are not available or disabled.
     */
    class MetricsOnlyBridge : public MesherBridge {
    private:
        std::vector<MesherMetrics> metricsHistory_;
        static constexpr size_t MAX_METRICS_HISTORY = 10000;
        
    public:
        MeshOptimizationHint processChunk(const VoxelChunkData& chunk) override {
            // Return default optimization hint
            MeshOptimizationHint hint;
            hint.expectedVertexCount = chunk.voxels.size() * 0.1f;  // Rough estimate
            hint.expectedRenderTime = 16.67f;  // Target 60 FPS
            
            return hint;
        }
        
        void recordMetrics(const MesherMetrics& metrics) override {
            metricsHistory_.push_back(metrics);
            
            // Keep history bounded
            if (metricsHistory_.size() > MAX_METRICS_HISTORY) {
                metricsHistory_.erase(metricsHistory_.begin());
            }
        }
        
        bool isAIEnabled() const override { return false; }
        std::string getModelVersion() const override { return "metrics-only"; }
        float getModelAccuracy() const override { return 0.0f; }
        
        void exportMetrics(const std::string& filepath) override {
            // Export metrics to JSON for analysis
            // Implementation would serialize metricsHistory_
        }
        
        // Additional metrics-only functionality
        size_t getMetricsCount() const { return metricsHistory_.size(); }
        
        MesherMetrics getAverageMetrics() const {
            if (metricsHistory_.empty()) return {};
            
            MesherMetrics avg;
            for (const auto& m : metricsHistory_) {
                avg.processingTime += m.processingTime;
                avg.inputVoxelCount += m.inputVoxelCount;
                avg.outputVertexCount += m.outputVertexCount;
                avg.outputIndexCount += m.outputIndexCount;
                avg.optimizationRatio += m.optimizationRatio;
            }
            
            size_t count = metricsHistory_.size();
            avg.processingTime /= count;
            avg.inputVoxelCount /= count;
            avg.outputVertexCount /= count;
            avg.outputIndexCount /= count;
            avg.optimizationRatio /= count;
            
            return avg;
        }
    };

#ifdef ENABLE_AI_TRT
    /**
     * TensorRT-Powered AI Bridge
     * 
     * Uses TensorRT inference engine for AI-enhanced meshing optimization.
     * Falls back to metrics-only mode if models fail to load.
     */
    class TensorRTBridge : public MesherBridge {
    private:
        std::unique_ptr<class TensorRTEngine> engine_;
        std::unique_ptr<MetricsOnlyBridge> fallbackBridge_;
        bool aiEnabled_ = false;
        std::string modelVersion_ = "unknown";
        float modelAccuracy_ = 0.0f;
        
    public:
        TensorRTBridge(const std::string& modelPath);
        ~TensorRTBridge();
        
        MeshOptimizationHint processChunk(const VoxelChunkData& chunk) override;
        void recordMetrics(const MesherMetrics& metrics) override;
        
        bool isAIEnabled() const override { return aiEnabled_; }
        std::string getModelVersion() const override { return modelVersion_; }
        float getModelAccuracy() const override { return modelAccuracy_; }
        
        void exportMetrics(const std::string& filepath) override;
        
        // TensorRT-specific functionality
        bool loadModel(const std::string& modelPath);
        void unloadModel();
        bool validateModel() const;
        size_t getModelMemoryUsage() const;
    };
#endif

    /**
     * Factory function to create appropriate mesher bridge
     */
    std::unique_ptr<MesherBridge> createMesherBridge(const std::string& configPath = "");

    /**
     * Utility functions for AI integration
     */
    namespace Utils {
        bool isAIRuntimeAvailable();
        std::vector<std::string> getAvailableModels(const std::string& modelsPath);
        bool validateModelFile(const std::string& modelPath);
        
        struct AICapabilities {
            bool tensorrtSupport = false;
            bool onnxSupport = false;
            bool cudaSupport = false;
            std::string cudaVersion;
            std::string tensorrtVersion;
        };
        
        AICapabilities detectAICapabilities();
    }

} // namespace VulkenAI