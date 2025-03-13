#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <ctime>

// Shader sources
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    
    out vec3 FragPos;
    out vec3 Normal;
    
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    
    void main() {
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    
    in vec3 FragPos;
    in vec3 Normal;
    
    uniform vec3 viewPos;
    uniform vec3 lightDir;
    uniform vec3 materialColor;
    
    void main() {
        // Normalize normal vector
        vec3 norm = normalize(Normal);
        
        // Base color
        vec3 baseColor = materialColor;
        
        // Ambient lighting
        float ambientStrength = 0.3;
        vec3 ambient = ambientStrength * baseColor;
        
        // Diffuse lighting
        vec3 lightDirection = normalize(lightDir);
        float diff = max(dot(norm, lightDirection), 0.0);
        vec3 diffuse = diff * baseColor;
        
        // Specular lighting
        float specularStrength = 0.5;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 halfwayDir = normalize(lightDirection + viewDir);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), 64.0);
        vec3 specular = specularStrength * spec * vec3(1.0);
        
        // Final color
        vec3 result = ambient + diffuse + specular;
        
        // Apply gamma correction
        result = pow(result, vec3(1.0/2.2));
        
        FragColor = vec4(result, 1.0);
    }
)";

// Camera variables
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// Mouse control variables
float lastX = 400.0f;
float lastY = 300.0f;
bool firstMouse = true;
bool mousePressed = false;
float zoom = 45.0f;

// Object rotation variables
float rotationX = 0.0f;
float rotationY = 0.0f;

// Rendering settings
bool showWireframe = false;

// Light and material settings
glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
glm::vec3 materialColor = glm::vec3(0.9f, 0.9f, 0.95f);

// Function prototypes
GLuint compileShaders();
bool loadOBJ(const char* path, 
             std::vector<glm::vec3>& out_vertices, 
             std::vector<glm::vec3>& out_normals, 
             std::vector<glm::vec2>& out_uvs);
void calculateSmoothNormals(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals);

// Callback functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

int main(int argc, char* argv[]) {
    // Check if OBJ file path is provided
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_obj_file>" << std::endl;
        return -1;
    }

    const char* objFilePath = argv[1];

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8); // Enable high-quality anti-aliasing

    // Create window
    GLFWwindow* window = glfwCreateWindow(1200, 800, "OBJ Viewer", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetKeyCallback(window, key_callback);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Print basic controls to console
    std::cout << "========== OBJ Viewer Controls ==========\n";
    std::cout << "Mouse Drag: Rotate model\n";
    std::cout << "Scroll: Zoom in/out\n";
    std::cout << "W: Toggle wireframe\n";
    std::cout << "Esc: Exit\n";
    std::cout << "========================================\n";

    // Compile shaders
    GLuint shaderProgram = compileShaders();
    if (shaderProgram == 0) {
        return -1;
    }

    // Load OBJ file
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;

    std::cout << "Loading OBJ file: " << objFilePath << std::endl;
    if (!loadOBJ(objFilePath, vertices, normals, uvs)) {
        std::cerr << "Failed to load OBJ file: " << objFilePath << std::endl;
        return -1;
    }
    std::cout << "OBJ file loaded successfully. Vertices: " << vertices.size() << std::endl;

    // Calculate smooth normals if none provided
    if (normals.empty() || normals.size() != vertices.size()) {
        normals.clear();
        normals.resize(vertices.size(), glm::vec3(0.0f));
        calculateSmoothNormals(vertices, normals);
    }

    // Prepare data for GPU
    GLuint VAO, VBO, NBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &NBO);

    glBindVertexArray(VAO);

    // Position attribute
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glBindBuffer(GL_ARRAY_BUFFER, NBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(1);

    // Calculate center and scale for model
    glm::vec3 center(0.0f);
    float maxDistance = 0.0f;

    for (const auto& vertex : vertices) {
        center += vertex;
    }
    center /= vertices.size();

    for (const auto& vertex : vertices) {
        float distance = glm::length(vertex - center);
        if (distance > maxDistance) {
            maxDistance = distance;
        }
    }

    // Enable depth testing and multisampling
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    
    // Enable backface culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    
    // Smooth line rendering for wireframe
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    // Background color (dark gray)
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader program
        glUseProgram(shaderProgram);

        // Set uniform values
        GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");
        GLint lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
        GLint materialColorLoc = glGetUniformLocation(shaderProgram, "materialColor");
        
        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
        GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

        glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
        glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
        glUniform3fv(materialColorLoc, 1, glm::value_ptr(materialColor));

        // Create transformations
        glm::mat4 model = glm::mat4(1.0f);
        
        // Apply rotations to the model
        model = glm::rotate(model, glm::radians(rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Scale the model to fit the view
        model = glm::scale(model, glm::vec3(1.0f / maxDistance));
        // Center the model
        model = glm::translate(model, -center);

        // Fixed camera position looking at the center
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), cameraUp);
        
        // Get window size
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspectRatio = (float)width / (float)height;

        glm::mat4 projection = glm::perspective(glm::radians(zoom), aspectRatio, 0.1f, 100.0f);

        // Set matrices
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

        // Draw the model
        glBindVertexArray(VAO);
        
        if (showWireframe) {
            // Wireframe rendering
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.0f);
        } else {
            // Solid rendering
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        
        glDrawArrays(GL_TRIANGLES, 0, vertices.size());
        
        // Reset polygon mode for next frame if needed
        if (showWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        
        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &NBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

GLuint compileShaders() {
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Check for compilation errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    // Link shaders
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return 0;
    }

    // Delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

struct VertexKey {
    glm::vec3 position;
    
    bool operator<(const VertexKey& other) const {
        if (position.x != other.position.x) return position.x < other.position.x;
        if (position.y != other.position.y) return position.y < other.position.y;
        return position.z < other.position.z;
    }
};

bool loadOBJ(const char* path, 
             std::vector<glm::vec3>& out_vertices, 
             std::vector<glm::vec3>& out_normals, 
             std::vector<glm::vec2>& out_uvs) {
    
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
    std::vector<glm::vec3> temp_normals;
    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            temp_vertices.push_back(vertex);
        } else if (prefix == "vt") {
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            temp_uvs.push_back(uv);
        } else if (prefix == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        } else if (prefix == "f") {
            std::string vertex1, vertex2, vertex3;
            iss >> vertex1 >> vertex2 >> vertex3;
            
            // Handle faces with more than 3 vertices (triangulation)
            std::string vertex4;
            iss >> vertex4;
            bool isQuad = !vertex4.empty();
            
            std::vector<std::string> face_vertices;
            face_vertices.push_back(vertex1);
            face_vertices.push_back(vertex2);
            face_vertices.push_back(vertex3);
            
            if (isQuad) {
                // For quads, add another triangle
                face_vertices.push_back(vertex1); // Reuse vertex1
                face_vertices.push_back(vertex3); // Reuse vertex3
                face_vertices.push_back(vertex4);
            }
            
            // Process each triangle in the face
            for (size_t v = 0; v < face_vertices.size(); v += 3) {
                for (size_t i = 0; i < 3; i++) {
                    std::string vertex = face_vertices[v + i];
                    std::istringstream viss(vertex);
                    std::string token;
                    
                    // Parse vertex index
                    std::getline(viss, token, '/');
                    int vertexIndex = std::stoi(token);
                    vertexIndices.push_back(vertexIndex);
                    
                    // Parse texture coordinate index (if present)
                    if (std::getline(viss, token, '/')) {
                        if (!token.empty()) {
                            int uvIndex = std::stoi(token);
                            uvIndices.push_back(uvIndex);
                        }
                    }
                    
                    // Parse normal index (if present)
                    if (std::getline(viss, token, '/')) {
                        if (!token.empty()) {
                            int normalIndex = std::stoi(token);
                            normalIndices.push_back(normalIndex);
                        }
                    }
                }
            }
        }
    }

    // Process vertex indices (OBJ uses 1-based indexing)
    for (size_t i = 0; i < vertexIndices.size(); i++) {
        if (vertexIndices[i] > 0 && vertexIndices[i] <= temp_vertices.size()) {
            glm::vec3 vertex = temp_vertices[vertexIndices[i] - 1];
            out_vertices.push_back(vertex);
            
            // Process texture coordinates if available
            if (!uvIndices.empty() && i < uvIndices.size() && 
                uvIndices[i] > 0 && uvIndices[i] <= temp_uvs.size()) {
                out_uvs.push_back(temp_uvs[uvIndices[i] - 1]);
            } else if (!temp_uvs.empty()) {
                out_uvs.push_back(glm::vec2(0.0f, 0.0f));
            }
            
            // Process normals if available
            if (!normalIndices.empty() && i < normalIndices.size() && 
                normalIndices[i] > 0 && normalIndices[i] <= temp_normals.size()) {
                out_normals.push_back(temp_normals[normalIndices[i] - 1]);
            }
        }
    }

    return true;
}

void calculateSmoothNormals(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals) {
    // Map to store vertex position to vertex indices
    std::map<VertexKey, std::vector<size_t>> vertexMap;
    
    // First, identify identical vertices
    for (size_t i = 0; i < vertices.size(); i++) {
        VertexKey key = {vertices[i]};
        vertexMap[key].push_back(i);
    }
    
    // Calculate face normals first
    for (size_t i = 0; i < vertices.size(); i += 3) {
        if (i + 2 < vertices.size()) {
            glm::vec3 v0 = vertices[i];
            glm::vec3 v1 = vertices[i + 1];
            glm::vec3 v2 = vertices[i + 2];
            
            // Calculate face normal
            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;
            glm::vec3 faceNormal = glm::cross(edge1, edge2);
            
            // Accumulate this normal for each vertex of the triangle
            normals[i] += faceNormal;
            normals[i + 1] += faceNormal;
            normals[i + 2] += faceNormal;
        }
    }
    
    // Now, average normals for identical vertices
    for (const auto& entry : vertexMap) {
        if (entry.second.size() > 1) {
            // Accumulate all normals
            glm::vec3 avgNormal(0.0f);
            for (size_t idx : entry.second) {
                avgNormal += normals[idx];
            }
            
            // Normalize and assign back
            avgNormal = glm::normalize(avgNormal);
            for (size_t idx : entry.second) {
                normals[idx] = avgNormal;
            }
        } else if (entry.second.size() == 1) {
            // Just normalize single normals
            size_t idx = entry.second[0];
            normals[idx] = glm::normalize(normals[idx]);
        }
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_W:
                showWireframe = !showWireframe;
                std::cout << "Wireframe: " << (showWireframe ? "ON" : "OFF") << std::endl;
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, true);
                break;
        }
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!mousePressed) {
        return;
    }
    
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        return;
    }

    float xoffset = xpos - lastX;
    float yoffset = ypos - lastY;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.5f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    rotationY += xoffset;
    rotationX += yoffset;

    // Restrict rotation angles
    if (rotationX > 89.0f) rotationX = 89.0f;
    if (rotationX < -89.0f) rotationX = -89.0f;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cameraPos.z -= yoffset * 0.2f;
    
    // Limit camera distance
    if (cameraPos.z < 0.5f) cameraPos.z = 0.5f;
    if (cameraPos.z > 10.0f) cameraPos.z = 10.0f;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mousePressed = true;
            firstMouse = true;
        } else if (action == GLFW_RELEASE) {
            mousePressed = false;
        }
    }
}
