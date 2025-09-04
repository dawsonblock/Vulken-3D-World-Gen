#pragma once
#include <functional>
#include <vector>
namespace voxelvk {
struct FrameCtx{ /* cmd buf, descriptors, size, etc. */ void* user= nullptr; };
using PassFn = std::function<void(FrameCtx&)>;
struct FrameGraphMin {
  std::vector<PassFn> passes;
  template<typename Fn> void add(Fn&& fn){ passes.emplace_back(std::forward<Fn>(fn)); }
  void execute(FrameCtx& ctx){ for(auto& p: passes) p(ctx); }
};
} // namespace voxelvk