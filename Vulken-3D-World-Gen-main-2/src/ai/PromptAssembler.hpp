#pragma once
// PromptAssembler.hpp — header-only RAG→prompt builder (Apache-2.0)
// Drop-in: no external deps. C++17+.
//
// Features
// - Rank/clip RAG snippets to a token budget (approximation).
// - Deterministic formatting for system/context/task/constraints.
// - Optional JSON-output section (schema or skeleton).
// - Also emits chat-style messages if your model prefers role chunks.

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <cctype>

namespace voxelvk::ai {

// -------- Types --------

struct RagSnippet {
    std::string text;
    float       score = 0.0f;     // higher = better (optional)
    std::string source;           // filename/url/tag (optional)
};

struct PromptSpec {
    std::string system;                   // system preamble
    std::string task;                     // concrete instruction
    std::vector<std::string> constraints; // bullet points
    std::vector<RagSnippet>  references;  // RAG context
    std::size_t max_tokens   = 3200;      // rough token budget
    std::size_t max_refs     = 6;         // hard cap on refs to include
    bool json_output         = false;     // add JSON contract section
    std::string json_schema;              // optional schema/example JSON
};

struct ChatMessage {
    std::string role;    // "system" | "user" | "assistant"
    std::string content;
};

// -------- Helpers --------

inline std::string trim_copy(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return s.substr(a, b-a);
}

// Very rough token estimator: ~4 chars/token (OpenAI-ish heuristic).
inline std::size_t estimate_tokens(const std::string& s) {
    const double chars_per_token = 4.0;
    return static_cast<std::size_t>((s.size() / chars_per_token) + 0.5);
}

inline void append_line(std::ostringstream& oss, const std::string& line) {
    oss << line << '\n';
}

// Deduplicate near-identical refs by normalized hash (case/space squashing).
inline bool is_duplicate(const std::string& t, std::unordered_set<std::string>& seen) {
    std::string k; k.reserve(t.size());
    for (char c : t) if (!std::isspace(static_cast<unsigned char>(c)))
        k.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (k.size() > 600) k.resize(600);
    if (seen.count(k)) return true;
    seen.insert(k);
    return false;
}

// Truncate a paragraph to ~N tokens by cutting at boundary.
inline std::string truncate_tokens(const std::string& s, std::size_t token_budget) {
    if (estimate_tokens(s) <= token_budget) return s;
    // Cut by chars proportional to the token budget
    const std::size_t max_chars = token_budget * 4; // inverse of estimator
    if (s.size() <= max_chars) return s;
    std::size_t cut = max_chars;
    // Backtrack to a whitespace for clean cut
    while (cut > 40 && cut < s.size() && !std::isspace(static_cast<unsigned char>(s[cut])))
        --cut;
    return s.substr(0, cut) + " …";
}

// -------- Assembly --------

inline std::string assemble_prompt(const PromptSpec& spec_in) {
    // Defensive copy to sort & cap refs
    PromptSpec spec = spec_in;
    // Sort references by score DESC, stable on input order
    std::stable_sort(spec.references.begin(), spec.references.end(),
        [](const RagSnippet& a, const RagSnippet& b){ return a.score > b.score; });

    // Hard cap number of refs
    if (spec.references.size() > spec.max_refs)
        spec.references.resize(spec.max_refs);

    std::ostringstream oss;

    // System
    append_line(oss, "[System]");
    append_line(oss, trim_copy(spec.system));
    append_line(oss, "");

    // References — include while staying under budget (reserve ~40% of budget for refs)
    const std::size_t total_budget  = spec.max_tokens > 64 ? spec.max_tokens : 64;
    const std::size_t refs_budget   = (total_budget * 40) / 100;
    std::size_t used_ref_tokens = 0;
    std::unordered_set<std::string> seen;

    append_line(oss, "[Domain References]");
    if (spec.references.empty()) {
        append_line(oss, "(none provided)");
    } else {
        int idx = 1;
        for (const auto& r : spec.references) {
            if (r.text.empty()) continue;
            if (is_duplicate(r.text, seen)) continue;

            // Allocate a slice of refs_budget for this snippet
            const std::size_t remaining = (used_ref_tokens < refs_budget) ? (refs_budget - used_ref_tokens) : 0;
            if (remaining < 24) break; // too small to be useful

            // Bias: earlier refs get a bit more
            std::size_t per_ref_budget = std::max<std::size_t>(remaining / 2, 128);
            const std::string clipped = truncate_tokens(trim_copy(r.text), per_ref_budget);
            const std::size_t clipped_tokens = estimate_tokens(clipped);

            // Header line
            std::ostringstream hdr;
            hdr << idx++ << ") ";
            if (!r.source.empty())  hdr << "[" << r.source << "] ";
            if (r.score > 0.0f)     hdr << "(score=" << (int)(r.score*100+0.5) / 100.0 << ") ";
            append_line(oss, hdr.str());
            append_line(oss, clipped);
            append_line(oss, "");
            used_ref_tokens += clipped_tokens;
            if (used_ref_tokens >= refs_budget) break;
        }
    }

    // Task
    append_line(oss, "[Task]");
    append_line(oss, trim_copy(spec.task));
    append_line(oss, "");

    // Constraints
    append_line(oss, "[Constraints]");
    if (spec.constraints.empty()) {
        append_line(oss, "- Keep outputs concise and unambiguous.");
    } else {
        for (const auto& c : spec.constraints) {
            std::string line = "- " + trim_copy(c);
            append_line(oss, line);
        }
    }
    append_line(oss, "");

    // JSON output contract (optional)
    if (spec.json_output) {
        append_line(oss, "[Output]");
        append_line(oss, "Return JSON only. Do not include commentary.");
        if (!spec.json_schema.empty()) {
            append_line(oss, "Schema or example:");
            append_line(oss, spec.json_schema);
        }
    }

    return oss.str();
}

// Chat-style assembly (system + user only, to keep state simple)
inline std::vector<ChatMessage> assemble_chat(const PromptSpec& spec) {
    std::vector<ChatMessage> msgs;
    ChatMessage sys{"system", trim_copy(spec.system)};
    ChatMessage usr{"user",   assemble_prompt(spec)}; // reuse full prompt as user content
    msgs.push_back(std::move(sys));
    msgs.push_back(std::move(usr));
    return msgs;
}

} // namespace voxelvk::ai