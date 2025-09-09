#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include "../src/core/fullscreen_toggle.hpp"
#include "../src/ai/ai_palette_config_io.hpp"
#include "../src/ai/rag_runtime_bridge.hpp"

#if __has_include(<imgui.h>) && __has_include(<imgui/backends/imgui_impl_glfw.h>) && __has_include(<imgui/backends/imgui_impl_opengl3.h>)
  #include <imgui.h>
  #include <imgui/backends/imgui_impl_glfw.h>
  #include <imgui/backends/imgui_impl_opengl3.h>
  #define GUI_DEMO_HAS_IMGUI 1
#endif

static bool g_is_fullscreen = false;
static voxelvk::WindowedState g_windowed_state;

static void error_cb(int code, const char* desc){ std::fprintf(stderr, "GLFW error %d: %s\n", code, desc); }

static void toggle_fullscreen(GLFWwindow* win, bool on){
    g_is_fullscreen = on;
    voxelvk::ToggleFullscreen(win, on, g_windowed_state);
    glfwSetWindowTitle(win, on ? "Fullscreen Demo (Fullscreen)" : "Fullscreen Demo (Windowed)");
}

static bool point_in_rect(double x, double y, float rx, float ry, float rw, float rh){
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    return fx >= rx && fx <= rx+rw && fy >= ry && fy <= ry+rh;
}

int main(){
    glfwSetErrorCallback(error_cb);
    if(!glfwInit()){ std::fprintf(stderr, "Failed to init GLFW\n"); return 1; }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Fullscreen Demo (Windowed)", nullptr, nullptr);
    if(!window){ std::fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return 2; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Wire our helper so ImGui or other UI can call RequestFullscreen()
    voxelvk::SetFullscreenHandler([window](bool on){ toggle_fullscreen(window, on); });

    // RAG runtime wiring + palette hot reload propagation
    voxelvk::ai::PaletteRuntime palette_rt; palette_rt.load_from_file("ai_palette.cfg");
    voxelvk::ai::SetRagConfigHandler([&](bool enabled, int topk){ std::printf("[RAG] (demo) enable=%d top_k=%d\n", (int)enabled, topk); });
    voxelvk::ai::UpdateRagConfig(palette_rt.cfg.ai_generation.enable_rag, palette_rt.cfg.ai_generation.rag_top_k);

#ifdef GUI_DEMO_HAS_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
#endif

    const float btn_x = 20.0f, btn_y = 20.0f, btn_w = 220.0f, btn_h = 48.0f;

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();

        // Palette hot reload
        palette_rt.tick_hot_reload();
        voxelvk::ai::UpdateRagConfig(palette_rt.cfg.ai_generation.enable_rag, palette_rt.cfg.ai_generation.rag_top_k);

        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0,0,fbw,fbh);
        glClearColor(0.1f,0.12f,0.16f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

#ifdef GUI_DEMO_HAS_IMGUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Display Settings");
        if(!g_is_fullscreen){
            if(ImGui::Button("Go Fullscreen", ImVec2(200, 36))){
                voxelvk::RequestFullscreen(true);
            }
        } else {
            if(ImGui::Button("Exit Fullscreen", ImVec2(200, 36))){
                voxelvk::RequestFullscreen(false);
            }
        }
        ImGui::Text("RAG: %s, top-k=%d", palette_rt.cfg.ai_generation.enable_rag?"on":"off", palette_rt.cfg.ai_generation.rag_top_k);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
    // No ImGui path: minimal input handling without legacy GL fixed-function calls

        if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS){
            double mx, my; glfwGetCursorPos(window, &mx, &my);
            if(point_in_rect(mx, my, btn_x, btn_y, btn_w, btn_h)){
                while(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS){
                    glfwPollEvents();
                }
                voxelvk::RequestFullscreen(!g_is_fullscreen);
            }
        }
#endif

        glfwSwapBuffers(window);
    }

#ifdef GUI_DEMO_HAS_IMGUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
