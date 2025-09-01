#include "rag_query_engine.hpp"
#include <numeric>
#include <cmath>
#include <cstring>

namespace voxelvk::ai {

RAGQueryEngine::RAGQueryEngine() = default;
RAGQueryEngine::~RAGQueryEngine() = default;

bool RAGQueryEngine::initialize(const std::string &embedding_model_path,
                                const std::string &index_path,
                                const std::string &docs_path) {
    // Load DB first
    if (!database_.load(index_path, docs_path)) {
        // Still allow operation with empty DB (no retrieval)
    }

#if defined(VOXELVK_HAS_ONNXRUNTIME) && VOXELVK_HAS_ONNXRUNTIME
    if (!embedding_model_path.empty()) {
        initOnnx(embedding_model_path);
    }
#endif
    return true;
}

std::vector<std::string> RAGQueryEngine::query(const std::string &text_prompt, int k) const {
    std::vector<float> emb;
#if defined(VOXELVK_HAS_ONNXRUNTIME) && VOXELVK_HAS_ONNXRUNTIME
    if (ort_session_) emb = embedONNX(text_prompt);
#endif
    if (emb.empty()) emb = embedFallback(text_prompt);

    auto ids = database_.search(emb, k, text_prompt);
    std::vector<std::string> docs;
    docs.reserve(ids.size());
    for (auto id : ids) docs.push_back(database_.getDocumentText(id));
    return docs;
}

#if defined(VOXELVK_HAS_ONNXRUNTIME) && VOXELVK_HAS_ONNXRUNTIME
bool RAGQueryEngine::initOnnx(const std::string &model_path) {
    try {
        ort_env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "rag_embed");
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetInterOpNumThreads(1);
        ort_session_ = std::make_unique<Ort::Session>(*ort_env_, model_path.c_str(), opts);

        size_t in_count = ort_session_->GetInputCount();
        size_t out_count = ort_session_->GetOutputCount();
        input_names_.resize(in_count);
        output_names_.resize(out_count);
        // Names may not be required depending on the model; keep empty
        return true;
    } catch (...) {
        ort_session_.reset();
        ort_env_.reset();
        return false;
    }
}

std::vector<float> RAGQueryEngine::embedONNX(const std::string &text) const {
    // Placeholder: implement tokenization + ONNX model execution if model schema is known.
    // For now, fall back to portable embedding to avoid runtime dependency issues.
    (void)text;
    return {};
}
#endif

std::vector<float> RAGQueryEngine::embedFallback(const std::string &text) const {
    // Simple, deterministic hashing-based embedding. Not semantically rich but stable.
    // Produces a 384-dim vector similar to MiniLM for FAISS dimensionality.
    const int dim = 384;
    std::vector<float> v(dim, 0.0f);

    // Tokenize on non-alnum
    uint32_t h = 2166136261u;
    auto add_token = [&](const std::string &tok) {
        // FNV-1a
        uint32_t th = 2166136261u;
        for (unsigned char c : tok) {
            th ^= (uint32_t)std::tolower(c);
            th *= 16777619u;
        }
        int idx = (int)(th % dim);
        v[idx] += 1.0f;
    };

    std::string cur;
    cur.reserve(32);
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) cur.push_back((char)std::tolower((unsigned char)c));
        else if (!cur.empty()) { add_token(cur); cur.clear(); }
    }
    if (!cur.empty()) add_token(cur);

    // L2 normalize
    float sumsq = 0.0f; for (float x : v) sumsq += x * x;
    float inv = sumsq > 0 ? 1.0f / std::sqrt(sumsq) : 1.0f;
    for (float &x : v) x *= inv;
    return v;
}

} // namespace voxelvk::ai