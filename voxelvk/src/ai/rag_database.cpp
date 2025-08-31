#include "rag_database.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(VOXELVK_HAS_FAISS) && VOXELVK_HAS_FAISS
#include <faiss/Index.h>
#include <faiss/index_io.h>
#endif

namespace voxelvk::ai {

#if defined(VOXELVK_HAS_FAISS) && VOXELVK_HAS_FAISS
struct RAGDatabase::FaissHolder {
    std::unique_ptr<faiss::Index> index;
};
#endif

static std::string trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

bool RAGDatabase::load(const std::string &index_path, const std::string &docs_path) {
    documents_.clear();

    // Load documents
    {
        std::ifstream in(docs_path);
        if (!in.is_open()) {
            return false;
        }
        std::string line;
        while (std::getline(in, line)) {
            // Replace placeholder newline markers back to real newlines
            size_t pos = 0;
            while ((pos = line.find("<NEWLINE>", pos)) != std::string::npos) {
                line.replace(pos, 9, "\n");
                pos += 1;
            }
            documents_.push_back(trim(line));
        }
    }

#if defined(VOXELVK_HAS_FAISS) && VOXELVK_HAS_FAISS
    if (!index_path.empty()) {
        try {
            auto raw = faiss::read_index(index_path.c_str());
            faiss_ = std::make_unique<FaissHolder>();
            faiss_->index.reset(raw);
        } catch (...) {
            // Fall back silently to keyword search
            faiss_.reset();
        }
    }
#endif

    // Build fallback inverted index regardless (cheap, used if FAISS missing)
    buildFallbackIndex();
    return !documents_.empty();
}

std::vector<int64_t> RAGDatabase::search(const std::vector<float> &query_embedding,
                                         int k,
                                         const std::string &query_text) const {
    k = std::max(1, k);
    std::vector<int64_t> result;

#if defined(VOXELVK_HAS_FAISS) && VOXELVK_HAS_FAISS
    if (faiss_ && faiss_->index) {
        if (faiss_->index->is_trained && faiss_->index->ntotal > 0) {
            std::vector<float> distances(k, 0.0f);
            std::vector<faiss::idx_t> labels(k, -1);
            faiss_->index->search(1, query_embedding.data(), k, distances.data(), labels.data());
            for (int i = 0; i < k; ++i) {
                if (labels[i] >= 0) result.push_back(static_cast<int64_t>(labels[i]));
            }
            if (!result.empty()) return result;
        }
    }
#endif

    // Fallback: simple keyword score using inverted index
    if (!query_text.empty() && !inverted_index_.empty()) {
        std::unordered_map<int, int> doc_hits;
        for (const auto &tok : tokenize(query_text)) {
            auto it = inverted_index_.find(tok);
            if (it != inverted_index_.end()) {
                for (int doc_id : it->second) doc_hits[doc_id]++;
            }
        }
        // Rank by hits
        std::vector<std::pair<int, int>> scored(doc_hits.begin(), doc_hits.end());
        std::sort(scored.begin(), scored.end(), [](auto &a, auto &b) { return a.second > b.second; });
        for (size_t i = 0; i < scored.size() && static_cast<int>(i) < k; ++i) {
            result.push_back(scored[i].first);
        }
    }

    // As a last resort, return first k docs
    if (result.empty()) {
        for (int i = 0; i < k && i < static_cast<int>(documents_.size()); ++i) result.push_back(i);
    }
    return result;
}

std::string RAGDatabase::getDocumentText(int64_t doc_id) const {
    if (doc_id < 0 || doc_id >= static_cast<int64_t>(documents_.size())) return {};
    return documents_[static_cast<size_t>(doc_id)];
}

void RAGDatabase::buildFallbackIndex() {
    inverted_index_.clear();
    for (int i = 0; i < static_cast<int>(documents_.size()); ++i) {
        for (const auto &tok : tokenize(documents_[i])) inverted_index_[tok].push_back(i);
    }
}

std::vector<std::string> RAGDatabase::tokenize(const std::string &text) {
    std::vector<std::string> out;
    std::string cur;
    cur.reserve(32);
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!cur.empty()) {
            out.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace voxelvk::ai