#pragma once
#include <functional>

namespace voxelvk::ai {

// Handler bridge to let UI update the live EnhancedAiGenConfig in your engine
// without introducing a hard dependency here. Your app should register a handler
// once (e.g., after creating/updating EnhancedAiGenConfig) that applies the
// enable_rag and top_k values to your live config and propagates to
// TensorRTMultiModelManager via updateConfig().
using RagConfigHandler = std::function<void(bool /*enable_rag*/, int /*top_k*/)>;

inline RagConfigHandler& _RagHandler(){ static RagConfigHandler h; return h; }
inline void SetRagConfigHandler(RagConfigHandler h){ _RagHandler() = std::move(h); }
inline void UpdateRagConfig(bool enable_rag, int top_k){ auto& h=_RagHandler(); if(h) h(enable_rag, top_k); }

} // namespace voxelvk::ai