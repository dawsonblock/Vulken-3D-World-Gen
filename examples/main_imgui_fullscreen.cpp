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
#include <string>
#include <chrono>
#include "../src/core/fullscreen_toggle.hpp"
#include "../src/ai/ai_palette_config_io.hpp"
#include "../src/ai/rag_runtime_bridge.hpp"

#if __has_include(<imgui.h>) && __has_include(<imgui/backends/imgui_impl_glfw.h>) && __has_include(<imgui/backends/imgui_impl_opengl3.h>)
  #include <imgui.h>
  #include <imgui/backends/imgui_impl_glfw.h>
  #include <imgui/backends/imgui_impl_opengl3.h>
  #define MAIN_HAS_IMGUI 1
#endif

static void error_cb(int code, const char* desc){ std::fprintf(stderr, "GLFW error %d: %s\n", code, desc); }

int main(){
    glfwSetErrorCallback(error_cb);
    if(!glfwInit()){ std::fprintf(stderr, "Failed to init GLFW\n"); return 1; }

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Production App (Windowed)", nullptr, nullptr);
    if(!window){ std::fprintf(stderr, "Failed to create window\n"); glfwTerminate(); return 2; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    voxelvk::SetFullscreenWindow(window);
    voxelvk::SetOnFullscreenChanged([window](bool on){
        glfwSetWindowTitle(window, on? "Production App (Fullscreen)" : "Production App (Windowed)");
    });

    // Runtime RAG config wiring via palette hot-reload
    voxelvk::ai::PaletteRuntime palette_rt; palette_rt.load_from_file("ai_palette.cfg");
    // Register a handler; in your engine, update EnhancedAiGenConfig + trt_manager.updateConfig(cfg)
    voxelvk::ai::SetRagConfigHandler([&](bool enabled, int topk){
        std::printf("[RAG] enable=%d top_k=%d\n", (int)enabled, topk);
    });
    // Initialize RAG from palette file
    voxelvk::ai::UpdateRagConfig(palette_rt.cfg.ai_generation.enable_rag, palette_rt.cfg.ai_generation.rag_top_k);

#ifdef MAIN_HAS_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
#endif

#ifdef MAIN_HAS_IMGUI
    double last_time = glfwGetTime();
    int frames = 0; double fps_accum = 0.0; double fps = 0.0;
#endif

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0,0,fbw,fbh);
        glClearColor(0.08f,0.09f,0.12f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Palette hot reload -> propagate RAG settings live
        palette_rt.tick_hot_reload();
        voxelvk::ai::UpdateRagConfig(palette_rt.cfg.ai_generation.enable_rag, palette_rt.cfg.ai_generation.rag_top_k);

#ifdef MAIN_HAS_IMGUI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if(ImGui::BeginMainMenuBar()){
            if(ImGui::BeginMenu("View")){
                if(!voxelvk::IsFullscreen()){
                    if(ImGui::MenuItem("Go Fullscreen")) voxelvk::RequestFullscreen(true);
                } else {
                    if(ImGui::MenuItem("Exit Fullscreen")) voxelvk::RequestFullscreen(false);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGui::Begin("Display Settings");
        const bool is_fs = voxelvk::IsFullscreen();
        if(!is_fs){ if(ImGui::Button("Go Fullscreen", ImVec2(200,36))) voxelvk::RequestFullscreen(true); }
        else { if(ImGui::Button("Exit Fullscreen", ImVec2(200,36))) voxelvk::RequestFullscreen(false); }
        ImGui::SameLine(); ImGui::Text("State: %s", is_fs? "Fullscreen" : "Windowed");

        ImGui::Separator();
        ImGui::Text("RAG: %s", palette_rt.cfg.ai_generation.enable_rag? "Enabled" : "Disabled");
        ImGui::Text("RAG top-k: %d", palette_rt.cfg.ai_generation.rag_top_k);

        ImGui::Separator();
        // Simple metrics
        ImGui::Text("FPS: %.1f", fps);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
        glfwSwapBuffers(window);

        // Update FPS
#ifdef MAIN_HAS_IMGUI
        double now = glfwGetTime();
        double dt = now - last_time;
        fps_accum += dt; frames++;
        if(fps_accum >= 0.5){ fps = frames / fps_accum; frames=0; fps_accum=0.0; }
        last_time = now;
#endif
    }

#ifdef MAIN_HAS_IMGUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
