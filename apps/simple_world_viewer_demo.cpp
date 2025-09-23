#ifdef ENABLE_GRAPHICS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

struct Camera {
    float x = 64.0f, y = 40.0f, z = 64.0f;
    float pitch = 0.0f, yaw = 0.0f;
    float speed = 5.0f;
};

struct VoxelRenderer {
    Camera camera;
    bool keys[1024] = {false};
    double lastX = 400, lastY = 300;
    bool firstMouse = true;

    void processInput(GLFWwindow* window, float deltaTime) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        float velocity = camera.speed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            camera.x += velocity * sin(camera.yaw * M_PI / 180.0f);
            camera.z -= velocity * cos(camera.yaw * M_PI / 180.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            camera.x -= velocity * sin(camera.yaw * M_PI / 180.0f);
            camera.z += velocity * cos(camera.yaw * M_PI / 180.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            camera.x -= velocity * cos(camera.yaw * M_PI / 180.0f);
            camera.z -= velocity * sin(camera.yaw * M_PI / 180.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            camera.x += velocity * cos(camera.yaw * M_PI / 180.0f);
            camera.z += velocity * sin(camera.yaw * M_PI / 180.0f);
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            camera.y += velocity;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            camera.y -= velocity;
    }

    void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        camera.yaw += xoffset;
        camera.pitch += yoffset;

        if (camera.pitch > 89.0f) camera.pitch = 89.0f;
        if (camera.pitch < -89.0f) camera.pitch = -89.0f;
    }

    void renderVoxel(int x, int y, int z, float r, float g, float b) {
        glColor3f(r, g, b);
        glPushMatrix();
        glTranslatef(x, y, z);

        // Draw cube faces
        glBegin(GL_QUADS);

        // Front face
        glVertex3f(0, 0, 1);
        glVertex3f(1, 0, 1);
        glVertex3f(1, 1, 1);
        glVertex3f(0, 1, 1);

        // Back face
        glVertex3f(1, 0, 0);
        glVertex3f(0, 0, 0);
        glVertex3f(0, 1, 0);
        glVertex3f(1, 1, 0);

        // Top face
        glVertex3f(0, 1, 0);
        glVertex3f(0, 1, 1);
        glVertex3f(1, 1, 1);
        glVertex3f(1, 1, 0);

        // Bottom face
        glVertex3f(0, 0, 0);
        glVertex3f(1, 0, 0);
        glVertex3f(1, 0, 1);
        glVertex3f(0, 0, 1);

        // Right face
        glVertex3f(1, 0, 0);
        glVertex3f(1, 1, 0);
        glVertex3f(1, 1, 1);
        glVertex3f(1, 0, 1);

        // Left face
        glVertex3f(0, 0, 0);
        glVertex3f(0, 0, 1);
        glVertex3f(0, 1, 1);
        glVertex3f(0, 1, 0);

        glEnd();
        glPopMatrix();
    }

    void renderWorld(const std::vector<std::vector<std::vector<int>>>& world, int width, int height, int depth) {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                for (int z = 0; z < depth; z++) {
                    if (world[x][y][z] != 0) {
                        float r, g, b;
                        switch (world[x][y][z]) {
                            case 1: r = 0.5f; g = 0.5f; b = 0.5f; break; // Stone
                            case 2: r = 0.0f; g = 0.8f; b = 0.0f; break; // Grass
                            case 3: r = 0.0f; g = 0.0f; b = 0.8f; break; // Water
                            case 4: r = 0.8f; g = 0.4f; b = 0.0f; break; // Mountain
                            case 5: r = 0.0f; g = 0.4f; b = 0.0f; break; // Tree
                            default: r = 0.8f; g = 0.8f; b = 0.8f; break; // Default
                        }
                        renderVoxel(x, y, z, r, g, b);
                    }
                }
            }
        }
    }
};

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    VoxelRenderer* renderer = static_cast<VoxelRenderer*>(glfwGetWindowUserPointer(window));
    renderer->mouseCallback(window, xpos, ypos);
}

int main(int argc, char* argv[]) {
    std::cout << "Simple World Viewer Demo\n";
    std::cout << "=======================\n\n";

    // Parse command line arguments
    int width = 32, height = 32, depth = 32;
    bool fullscreen = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --width W     Set world width (default: 32)\n";
            std::cout << "  --height H    Set world height (default: 32)\n";
            std::cout << "  --depth D     Set world depth (default: 32)\n";
            std::cout << "  --fullscreen  Start in fullscreen mode\n";
            std::cout << "  --help, -h    Show this help message\n";
            std::cout << "\nControls:\n";
            std::cout << "  WASD - Move\n";
            std::cout << "  Space - Up\n";
            std::cout << "  Shift - Down\n";
            std::cout << "  Mouse - Look around\n";
            std::cout << "  ESC - Exit\n";
            return 0;
        } else if (arg == "--width" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "--depth" && i + 1 < argc) {
            depth = std::stoi(argv[++i]);
        } else if (arg == "--fullscreen") {
            fullscreen = true;
        }
    }

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Simple World Viewer",
                                         fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    VoxelRenderer renderer;
    glfwSetWindowUserPointer(window, &renderer);

    // Generate simple world
    std::cout << "Generating world: " << width << "x" << height << "x" << depth << "\n";
    std::vector<std::vector<std::vector<int>>> world(width, std::vector<std::vector<int>>(height, std::vector<int>(depth, 0)));

    // Simple terrain generation
    for (int x = 0; x < width; x++) {
        for (int z = 0; z < depth; z++) {
            int terrainHeight = static_cast<int>(height * 0.3f + 10 * sin(x * 0.1f) * cos(z * 0.1f));
            terrainHeight = std::max(1, std::min(terrainHeight, height - 1));

            for (int y = 0; y < terrainHeight; y++) {
                if (y == terrainHeight - 1) {
                    world[x][y][z] = 2; // Grass
                } else if (y < terrainHeight - 3) {
                    world[x][y][z] = 1; // Stone
                } else {
                    world[x][y][z] = 4; // Mountain
                }
            }

            // Add some water
            if (terrainHeight < height * 0.2f) {
                for (int y = terrainHeight; y < height * 0.2f; y++) {
                    world[x][y][z] = 3; // Water
                }
            }
        }
    }

    std::cout << "World generated. Starting render loop...\n";
    std::cout << "Controls: WASD to move, Space/Shift for up/down, Mouse to look, ESC to exit\n\n";

    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        renderer.processInput(window, deltaTime);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Apply camera transformation
        glRotatef(-renderer.camera.pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-renderer.camera.yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-renderer.camera.x, -renderer.camera.y, -renderer.camera.z);

        // Enable depth testing
        glEnable(GL_DEPTH_TEST);

        // Render world
        renderer.renderWorld(world, width, height, depth);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#else
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Simple World Viewer Demo\n";
    std::cout << "=======================\n\n";
    std::cout << "This demo requires graphics support (ENABLE_GRAPHICS=ON).\n";
    std::cout << "Please rebuild with graphics enabled to use this demo.\n";
    return 0;
}
#endif
