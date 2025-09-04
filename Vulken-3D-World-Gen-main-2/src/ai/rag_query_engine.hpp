#pragma once
#include <string>
#include <vector>
#include <memory>
#include "rag_database.hpp"

#if defined(VOXELVK_HAS_ONNXRUNTIME) && VOXELVK_HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace voxelvk::ai {

// Query engine that converts text into embeddings (optional ONNXRuntime)
// and queries the RAGDatabase to retrieve relevant documents.
class RAGQueryEngine {
public:
    RAGQueryEngine();
    ~RAGQueryEngine();

    // Initialize the engine. Any of the paths can be empty to use defaults/fallbacks.
    bool initialize(const std::string &embedding_model_path,
                    const std::string &index_path,
                    const std::string &docs_path);

    // Query text and return top-k raw document strings
    std::vector<std::string> query(const std::string &text_prompt, int k = 3) const;

private:
    RAGDatabase database_;

#if defined(VOXELVK_HAS_ONNXRUNTIME) && VOXELVK_HAS_ONNXRUNTIME
    std::unique_ptr<Ort::Env> ort_env_;
    std::unique_ptr<Ort::Session> ort_session_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    bool initOnnx(const std::string &model_path);
    std::vector<float> embedONNX(const std::string &text) const;
#endif

    // Portable fallback embedding if ONNX is not available
    std::vector<float> embedFallback(const std::string &text) const;
};

} // namespace voxelvk::ai