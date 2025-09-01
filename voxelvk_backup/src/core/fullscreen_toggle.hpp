#pragma once
#include <GLFW/glfw3.h>
#include <functional>
#include <algorithm>

namespace voxelvk {

// Tracks windowed state to restore when leaving fullscreen
struct WindowedState {
    int x = 0, y = 0, w = 1280, h = 720;
};

// Internal: global handler and state for production-grade decoupled UI
inline std::function<void(bool)>& _FullscreenHandler(){ static std::function<void(bool)> fn; return fn; }
inline bool& _FullscreenState(){ static bool s=false; return s; }
inline std::function<void(bool)>& _OnChanged(){ static std::function<void(bool)> fn; return fn; }

// Public API
using FullscreenToggleHandler = std::function<void(bool)>;
inline void SetFullscreenHandler(FullscreenToggleHandler handler){ _FullscreenHandler() = std::move(handler); }
inline void SetOnFullscreenChanged(std::function<void(bool)> cb){ _OnChanged() = std::move(cb); }
inline void RequestFullscreen(bool on){ _FullscreenState() = on; auto& h=_FullscreenHandler(); if(h) h(on); auto& cb=_OnChanged(); if(cb) cb(on); }
inline bool IsFullscreen(){ return _FullscreenState(); }

// Find the monitor with the largest overlap with the current window
inline GLFWmonitor* GetNearestMonitor(GLFWwindow* window){
    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);

    int count = 0; GLFWmonitor** mons = glfwGetMonitors(&count);
    if(!mons || count == 0) return glfwGetPrimaryMonitor();

    long bestOverlap = -1; GLFWmonitor* best = mons[0];
    for(int i=0;i<count;i++){
        int mx, my, mw, mh;
        if(glfwGetMonitorWorkarea) glfwGetMonitorWorkarea(mons[i], &mx, &my, &mw, &mh);
        else {
            const GLFWvidmode* m = glfwGetVideoMode(mons[i]); mx = my = 0; mw = m? m->width : 1920; mh = m? m->height : 1080;
        }
        int x1 = std::max(wx, mx);
        int y1 = std::max(wy, my);
        int x2 = std::min(wx + ww, mx + mw);
        int y2 = std::min(wy + wh, my + mh);
        long overlap = (long)std::max(0, x2 - x1) * (long)std::max(0, y2 - y1);
        if(overlap > bestOverlap){ bestOverlap = overlap; best = mons[i]; }
    }
    return best ? best : glfwGetPrimaryMonitor();
}

// Apply toggle on a concrete GLFW window (uses nearest monitor for better UX)
inline void ToggleFullscreen(GLFWwindow* window, bool fullscreen, WindowedState& state){
    if(!window) return;
    if(fullscreen){
        glfwGetWindowPos(window, &state.x, &state.y);
        glfwGetWindowSize(window, &state.w, &state.h);
        GLFWmonitor* monitor = GetNearestMonitor(window);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window, nullptr, state.x, state.y, state.w, state.h, 0);
    }
}

// Convenience: bind a specific GLFWwindow so UI buttons just work
inline void SetFullscreenWindow(GLFWwindow* window){
    static WindowedState s;
    SetFullscreenHandler([window](bool on){ ToggleFullscreen(window, on, s); });
}

} // namespace voxelvk