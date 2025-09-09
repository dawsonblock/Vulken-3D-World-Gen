#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>

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
    
    void renderVoxelCube(float x, float y, float z, float r, float g, float b) {
        glColor3f(r, g, b);
        glPushMatrix();
        glTranslatef(x, y, z);
        
        // Draw cube using immediate mode (simple but works)
        glBegin(GL_QUADS);
        
        // Front face
        glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        
        // Back face
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        
        // Top face
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        
        // Bottom face
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
        
        // Right face
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        
        // Left face
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        
        glEnd();
        glPopMatrix();
    }
    
    void renderWorld() {
        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Set up camera
        glLoadIdentity();
        glRotatef(camera.pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(camera.yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camera.x, -camera.y, -camera.z);
        
        // Render a sample world (simple grid pattern)
        for (int x = 0; x < 64; x += 2) {
            for (int z = 0; z < 64; z += 2) {
                // Generate simple terrain height
                float height = 10.0f + 5.0f * sin(x * 0.1f) * cos(z * 0.1f);
                
                for (int y = 0; y < height; y++) {
                    float r, g, b;
                    
                    // Color based on height and type
                    if (y < 5) {
                        r = 0.6f; g = 0.4f; b = 0.2f; // Stone
                    } else if (y < height - 1) {
                        r = 0.3f; g = 0.6f; b = 0.2f; // Dirt
                    } else {
                        r = 0.2f; g = 0.8f; b = 0.2f; // Grass
                    }
                    
                    renderVoxelCube(x, y, z, r, g, b);
                }
                
                // Add some trees randomly
                if ((x + z) % 10 == 0 && height > 8) {
                    for (int ty = 0; ty < 4; ty++) {
                        renderVoxelCube(x, height + ty, z, 0.4f, 0.2f, 0.1f); // Tree trunk
                    }
                    // Tree crown
                    renderVoxelCube(x, height + 4, z, 0.1f, 0.7f, 0.1f);
                    renderVoxelCube(x+1, height + 4, z, 0.1f, 0.7f, 0.1f);
                    renderVoxelCube(x-1, height + 4, z, 0.1f, 0.7f, 0.1f);
                    renderVoxelCube(x, height + 4, z+1, 0.1f, 0.7f, 0.1f);
                    renderVoxelCube(x, height + 4, z-1, 0.1f, 0.7f, 0.1f);
                }
            }
        }
    }
};

int main() {
    std::cout << "🎮 VoxelVK Simple World Viewer" << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move around" << std::endl;
    std::cout << "  Space - Move up" << std::endl;
    std::cout << "  Shift - Move down" << std::endl;
    std::cout << "  Mouse - Look around" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;
    std::cout << std::endl;
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1024, 768, "VoxelVK Demo World", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Set up OpenGL
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.5f, 0.7f, 0.9f, 1.0f); // Sky blue background
    
    // Set up projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 1024.0/768.0, 0.1, 1000.0);
    glMatrixMode(GL_MODELVIEW);
    
    VoxelRenderer renderer;
    
    // Set up mouse callback
    glfwSetWindowUserPointer(window, &renderer);
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
        VoxelRenderer* renderer = static_cast<VoxelRenderer*>(glfwGetWindowUserPointer(window));
        renderer->mouseCallback(window, xpos, ypos);
    });
    
    std::cout << "🚀 Starting demo world viewer..." << std::endl;
    
    float lastFrame = 0.0f;
    
    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        renderer.processInput(window, deltaTime);
        renderer.renderWorld();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    std::cout << "👋 Demo world viewer closed." << std::endl;
    
    glfwTerminate();
    return 0;
}
