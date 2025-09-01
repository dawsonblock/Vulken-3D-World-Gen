
// GLFW hook example — call this once after creating the GLFW window and Vulkan instance:
// (Only an example — wire into your own input system as needed)
/*
#include <GLFW/glfw3.h>
#include "src/core/runtime_debug.hpp"

static voxelvk::DebugRuntime gDebug;
static void on_key(GLFWwindow* win, int key, int sc, int action, int mods){
    if(action == GLFW_PRESS && key == GLFW_KEY_F9){
        voxelvk::DebugRuntime_Toggle(gDebug);
    }
}
// ...
glfwSetKeyCallback(window, on_key);
// After VkInstance creation:
voxelvk::DebugRuntime_Init(gDebug, instance);
*/
