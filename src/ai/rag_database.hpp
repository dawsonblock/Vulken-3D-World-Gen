#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace voxelvk::ai {

// Lightweight RAG database wrapper with optional FAISS acceleration.
// If FAISS is unavailable at build time, this falls back to a simple
// keyword-similarity search over loaded documents.
class RAGDatabase {
public:
    // Load FAISS index (optional) and documents file (required)
    // index_path can be empty to disable vector search
    bool load(const std::string &index_path, const std::string &docs_path);

    // Perform ANN search with a query embedding vector. If FAISS is not
    // available/loaded, falls back to keyword search (query_text should be
    // provided in that case for reasonable results).
    std::vector<int64_t> search(const std::vector<float> &query_embedding,
                                int k,
                                const std::string &query_text = std::string()) const;

    std::string getDocumentText(int64_t doc_id) const;

    // Utility: number of documents
    size_t size() const { return documents_.size(); }

private:
    // Documents, one per line in rag_documents.txt (with <NEWLINE> markers)
    std::vector<std::string> documents_;

#if defined(VOXELVK_HAS_FAISS) && VOXELVK_HAS_FAISS
    // Forward-declare FAISS types to avoid hard dependency in header
    struct FaissHolder;
    std::unique_ptr<FaissHolder> faiss_;
#endif

    // Simple keyword index for fallback
    std::unordered_map<std::string, std::vector<int>> inverted_index_;

    void buildFallbackIndex();
    static std::vector<std::string> tokenize(const std::string &text);
};

} // namespace voxelvk::ai