#pragma once
// LLMClient.hpp — tiny, header-only LLM HTTP client with pluggable transport.
// License: Apache-2.0
//
// Key points
// - Providers: OpenAI (chat or responses), Anthropic (messages), or Generic JSON.
// - No hard deps. Bring your own HTTP+JSON, OR enable libcurl / nlohmann JSON by defines.
// - Works with PromptAssembler.hpp (assemble_prompt / assemble_chat).
//
// Optional compile flags
//   - DLL:   -DLLMCLIENT_WITH_LIBCURL        (adds a curl-based transport)
//   - JSON:  -DLLMCLIENT_WITH_NLOHMANN_JSON  (parses JSON robustly)
// Otherwise: naive string parsing + user-supplied transport.
//
// Minimal usage (OpenAI chat):
//   LLMClient::Config cfg{ LLMClient::Provider::OpenAIChat, "https://api.openai.com", apiKey, "gpt-4o-mini" };
//   LLMClient cli{cfg};
//   auto out = cli.complete_chat({{"system","You are helpful"},{"user","Hello"}});
//   std::cout << out.text << "\n";

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <optional>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace voxelvk::ai {

struct ChatMessage { std::string role, content; };

struct HttpRequest {
  std::string method = "POST";
  std::string url;                       // absolute
  std::map<std::string,std::string> headers;
  std::string body;
  int timeout_ms = 60000;
};

struct HttpResponse {
  long status = 0;
  std::map<std::string,std::string> headers;
  std::string body;
};

using HttpTransport = std::function<bool(const HttpRequest&, HttpResponse&)>;

struct Completion {
  std::string text;          // primary text output
  std::string raw_body;      // full raw response body for debugging
  long        http_status=0;
};

struct LLMClient {
  enum class Provider { OpenAIChat, OpenAIResponses, AnthropicMessages, GenericJSON };

  struct Config {
    Provider provider = Provider::OpenAIChat;
    std::string api_base = "https://api.openai.com";   // no trailing slash
    std::string api_key;                               // "Bearer ..." value assembled automatically
    std::string model = "gpt-4o-mini";
    std::map<std::string,std::string> extra_headers;   // user headers
    int timeout_ms = 60000;
    // Anthropic requires version header; leave empty to provide manually in extra_headers
    std::string anthropic_version;                     // e.g., "2023-06-01"
    // Generic endpoint fields
    std::string generic_path;                          // e.g., "/v1/whatever"
    std::string generic_body_template;                 // use {MODEL}, {INPUT} placeholders
  };

  explicit LLMClient(Config cfg, HttpTransport tx = {})
  : cfg_(std::move(cfg)), transport_(std::move(tx)) {}

  // Attach/override transport after construction
  void set_transport(HttpTransport tx) { transport_ = std::move(tx); }

  // One-shot with a plain prompt (wrapped as a single-user chat)
  Completion complete_prompt(const std::string& prompt, double temperature = 0.2) const {
    std::vector<ChatMessage> msgs = { {"user", prompt} };
    return complete_chat(msgs, temperature);
  }

  // Chat completion
  Completion complete_chat(const std::vector<ChatMessage>& messages, double temperature = 0.2) const {
    HttpRequest req = build_request(messages, temperature);
    HttpResponse res;
    if (!ensure_transport_(req, res)) {
      return { /*text*/"", /*raw*/"Transport failed (no transport or error).", /*status*/0 };
    }
    return parse_response_(res);
  }

private:
  Config cfg_;
  HttpTransport transport_;

  // -------- Request builders --------
  HttpRequest build_request(const std::vector<ChatMessage>& messages, double temperature) const {
    HttpRequest r;
    r.method = "POST";
    r.timeout_ms = cfg_.timeout_ms;

    auto auth = std::string("Bearer ") + cfg_.api_key;
    switch (cfg_.provider) {
      case Provider::OpenAIChat: {
        r.url = cfg_.api_base + "/v1/chat/completions";
        r.headers = {
          {"Authorization", auth},
          {"Content-Type", "application/json"}
        };
        r.body = to_json_openai_chat_(messages, temperature);
        break;
      }
      case Provider::OpenAIResponses: {
        r.url = cfg_.api_base + "/v1/responses";
        r.headers = {
          {"Authorization", auth},
          {"Content-Type", "application/json"}
        };
        r.body = to_json_openai_responses_(messages, temperature);
        break;
      }
      case Provider::AnthropicMessages: {
        r.url = cfg_.api_base + "/v1/messages";
        r.headers = {
          {"x-api-key", cfg_.api_key},
          {"content-type", "application/json"},
          {"anthropic-version", cfg_.anthropic_version.empty() ? "2023-06-01" : cfg_.anthropic_version}
        };
        r.body = to_json_anthropic_(messages, temperature);
        break;
      }
      case Provider::GenericJSON: {
        r.url = cfg_.api_base + cfg_.generic_path;
        r.headers = { {"Content-Type","application/json"} };
        std::string body = cfg_.generic_body_template;
        replace_all_(body, "{MODEL}", cfg_.model);
        std::string in = flatten_chat_(messages);
        replace_all_(body, "{INPUT}", escape_json_(in));
        r.body = body;
        break;
      }
    }
    for (auto& kv : cfg_.extra_headers) r.headers[kv.first] = kv.second;
    return r;
  }

  // -------- JSON body assembly --------
  static std::string escape_json_(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
      switch (c) {
        case '\"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:   o << c; break;
      }
    }
    return o.str();
  }

  static std::string flatten_chat_(const std::vector<ChatMessage>& m) {
    std::ostringstream o;
    for (auto& x : m) { o << "[" << x.role << "] " << x.content << "\n"; }
    return o.str();
  }

  std::string to_json_openai_chat_(const std::vector<ChatMessage>& m, double t) const {
    std::ostringstream o;
    o << "{\"model\":\"" << cfg_.model << "\",\"temperature\":" << t << ",\"messages\":[";
    for (size_t i=0;i<m.size();++i){
      if(i) o<<",";
      o << "{\"role\":\"" << m[i].role << "\",\"content\":\"" << escape_json_(m[i].content) << "\"}";
    }
    o << "]}";
    return o.str();
  }

  std::string to_json_openai_responses_(const std::vector<ChatMessage>& m, double t) const {
    // Responses API can take 'input' as string or 'messages' array; we send messages.
    std::ostringstream o;
    o << "{\"model\":\"" << cfg_.model << "\",\"temperature\":" << t << ",\"messages\":[";
    for (size_t i=0;i<m.size();++i){
      if(i) o<<",";
      o << "{\"role\":\"" << m[i].role << "\",\"content\":\"" << escape_json_(m[i].content) << "\"}";
    }
    o << "]}";
    return o.str();
  }

  std::string to_json_anthropic_(const std::vector<ChatMessage>& m, double t) const {
    // Anthropic expects system + messages (user/assistant). We fold the first "system" if present.
    std::string system;
    std::vector<ChatMessage> rest;
    for (auto& x : m) {
      if (system.empty() && x.role == "system") { system = x.content; }
      else rest.push_back(x);
    }
    std::ostringstream o;
    o << "{\"model\":\"" << cfg_.model << "\",\"max_tokens\":1024,\"temperature\":" << t;
    if(!system.empty()) o << ",\"system\":\"" << escape_json_(system) << "\"";
    o << ",\"messages\":[";
    for (size_t i=0;i<rest.size();++i){
      if(i) o<<",";
      // Anthropic "content" prefers array of objects; we pass simple text block
      o << "{\"role\":\"" << rest[i].role << "\",\"content\":[{\"type\":\"text\",\"text\":\""
        << escape_json_(rest[i].content) << "\"}]}";
    }
    o << "]}";
    return o.str();
  }

  // -------- Response parsing --------
  Completion parse_response_(const HttpResponse& res) const {
    Completion out; out.http_status = res.status; out.raw_body = res.body;

#if defined(LLMCLIENT_WITH_NLOHMANN_JSON)
    try {
      #include <nlohmann/json.hpp>
      using nlohmann::json;
      auto j = json::parse(res.body);

      switch (cfg_.provider) {
        case Provider::OpenAIChat:
        case Provider::OpenAIResponses: {
          if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            if (j["choices"][0].contains("message")) {
              out.text = j["choices"][0]["message"].value("content", "");
            } else if (j["choices"][0].contains("text")) {
              out.text = j["choices"][0].value("text", "");
            }
          } else if (j.contains("output_text")) {
            out.text = j.value("output_text", "");
          }
          break;
        }
        case Provider::AnthropicMessages: {
          // messages API returns { content: [ {type:"text", text:"..."} ], ... }
          if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
            out.text = j["content"][0].value("text", "");
          }
          break;
        }
        case Provider::GenericJSON: {
          // try common fields
          if (j.contains("text")) out.text = j.value("text", "");
          else if (j.contains("output")) out.text = j.value("output", "");
          break;
        }
      }
    } catch (...) { /* fall through to naive */ }
#endif

    if (out.text.empty()) {
      // Naive fallback: try to fish the first "content":"..."
      out.text = naive_extract_("\"content\":\"", res.body);
      if (out.text.empty()) out.text = naive_extract_("\"text\":\"", res.body);
    }
    return out;
  }

  static std::string naive_extract_(const std::string& key, const std::string& s) {
    auto p = s.find(key); if (p==std::string::npos) return {};
    p += key.size();
    std::ostringstream o;
    for (size_t i=p;i<s.size();++i) {
      char c = s[i];
      if (c=='\"' && (i==0 || s[i-1] != '\\')) break;
      o << c;
    }
    return o.str();
  }

  static void replace_all_(std::string& s, const std::string& a, const std::string& b){
    size_t pos=0;
    while((pos=s.find(a,pos))!=std::string::npos){ s.replace(pos, a.size(), b); pos += b.size(); }
  }

  bool ensure_transport_(const HttpRequest& req, HttpResponse& res) const {
    if (transport_) return transport_(req, res);
#if defined(LLMCLIENT_WITH_LIBCURL)
    return curl_transport_(req, res);
#else
    (void)req; (void)res;
    return false; // no transport installed
#endif
  }

#if defined(LLMCLIENT_WITH_LIBCURL)
  // -------- Minimal libcurl transport (UTF-8 JSON POST only) --------
  static bool curl_transport_(const HttpRequest& req, HttpResponse& out) {
    // Minimal, in-header to avoid external .cpp.
    // Requires: -lcurl and curl headers.
    #include <curl/curl.h>
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    struct Buf { std::string s; };
    auto write_cb = +[](char* ptr, size_t size, size_t nmemb, void* userdata)->size_t {
      auto* b = static_cast<Buf*>(userdata);
      b->s.append(ptr, size*nmemb);
      return size*nmemb;
    };

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, req.timeout_ms);

    struct curl_slist* hdrs = nullptr;
    for (auto& kv : req.headers) {
      std::string h = kv.first + ": " + kv.second;
      hdrs = curl_slist_append(hdrs, h.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);

    Buf buf; curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode rc = curl_easy_perform(curl);
    long code = 0; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    out.status = code; out.body = std::move(buf.s);

    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(curl);
    return (rc == CURLE_OK);
  }
#endif
};

} // namespace voxelvk::ai