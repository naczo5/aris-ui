// aris-ui desktop runner — standalone GLFW testbed for UI prototyping.
//
// Demonstrates all MVP deliverables:
//   1. standalone window + frame loop (caller-driven, no IO inversion)
//   2. SDF rounded-rect renderer (radius/border/shadow driven by live sliders)
//   3. GL state guard (host background pass keeps its own pipeline state)
//   4. Button / Toggle / Slider with smooth hover & click transitions
//   5. two-column flexbox panel powered by Yoga
#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "common/AppSupport.hpp"
#include "ui/aris_ui.hpp"
#include "atlas_generator/atlas_gen.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "stb_image_write.h"

// ---------------------------------------------------------------------------
// Host-owned background pass: an animated gradient drawn with the runner's own
// shader/buffers. Its state must survive ctx.Render() untouched — the
// GlStateGuard guarantees that.
// ---------------------------------------------------------------------------
namespace hostbg {

const char* kVs = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uRes;
out vec2 vUv;
void main() {
    vUv = aPos;
    vec2 clip = (aPos * uRes) * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
}
)";

const char* kFs = R"(
#version 330 core
in vec2 vUv;
uniform float uTime;
out vec4 frag;
void main() {
    vec3 a = vec3(0.11, 0.12, 0.17);
    vec3 b = vec3(0.17, 0.13, 0.26);
    float g = vUv.x * 0.5 + vUv.y * 0.5;
    vec3 c = mix(a, b, g) + 0.03 * sin(uTime * 0.5 + g * 6.2831);
    frag = vec4(c, 1.0);
}
)";

struct Pass {
    unsigned int program = 0;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int uRes = -1, uTime = -1;

    void Init() {
        auto compile = [](GLenum type, const char* src) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            return s;
        };
        GLuint vs = compile(GL_VERTEX_SHADER, kVs);
        GLuint fs = compile(GL_FRAGMENT_SHADER, kFs);
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        uRes = glGetUniformLocation(program, "uRes");
        uTime = glGetUniformLocation(program, "uTime");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        const float quad[] = {0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    void Draw(float w, float h, float time) {
        glUseProgram(program);
        glBindVertexArray(vao);
        glUniform2f(uRes, w, h);
        glUniform1f(uTime, time);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glUseProgram(0);
    }
};

} // namespace hostbg

struct FrameInput {
    std::vector<ui::InputEvent> events;
    double mouseX = 0, mouseY = 0;
    int leftState = GLFW_RELEASE;

    void Pump(GLFWwindow* window) {
        events.clear();
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        if (x != mouseX || y != mouseY) {
            mouseX = x;
            mouseY = y;
            events.push_back(ui::MouseMoveEvent{ui::Vec2((float)x, (float)y)});
        }
        const int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
        if (state != leftState) {
            leftState = state;
            events.push_back(ui::MouseButtonEvent{
                ui::MouseButton::Left, state == GLFW_PRESS, ui::Vec2((float)x, (float)y)});
        }
        // Scroll: accumulated by callback below.
    }
};

static FrameInput g_input;
static double g_scrollX = 0, g_scrollY = 0;

static void ScrollCallback(GLFWwindow*, double x, double y) {
    g_scrollX += x;
    g_scrollY += y;
}

static void KeyCallback(GLFWwindow*, int key, int, int action, int) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        g_input.events.push_back(ui::KeyEvent{key, action == GLFW_PRESS, action == GLFW_REPEAT});
    }
}

static void ErrorCallback(int, const char* msg) {
    std::fprintf(stderr, "[glfw] %s\n", msg);
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1280, 760, "aris-ui runner", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (gladLoadGL((GLADloadfunc)glfwGetProcAddress) == 0) {
        std::fprintf(stderr, "failed to load OpenGL\n");
        return 1;
    }

    // --- font + atlas (offline generation, cached) ---
    std::string fontPath = argc > 1 ? argv[1] : aris_demo::FindSystemFont();
    if (fontPath.empty()) {
        std::fprintf(stderr, "no system font found; pass one: aris_runner <font.ttf>\n");
        return 1;
    }
    const std::string prefix = "atlas/ui";
    std::string err;
    if (!aris_demo::BootstrapAtlas(fontPath, prefix, err)) {
        std::fprintf(stderr, "atlas generation failed: %s\n", err.c_str());
        return 1;
    }
    ui::FontAtlas font;
    if (!font.Load(prefix + ".png", prefix + ".json")) return 1;

    ui::Context ctx;
    ctx.SetTheme(ui::Theme::Default());
    ctx.SetFont(&font);

    hostbg::Pass background;
    background.Init();

    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);

    // demo state
    aris_demo::DemoState demo;

    double lastTime = glfwGetTime();
    double fpsTimer = lastTime;
    int frames = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const double now = glfwGetTime();
        const float dt = (float)(now - lastTime);
        lastTime = now;

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.08f, 0.08f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- host pipeline: animated gradient background ---
        glEnable(GL_DEPTH_TEST);   // deliberate state the UI must preserve
        background.Draw((float)fbw, (float)fbh, (float)now);
        glDisable(GL_DEPTH_TEST);

        // --- UI frame (caller-driven) ---
        g_input.Pump(window);
        if (g_scrollX != 0 || g_scrollY != 0) {
            g_input.events.push_back(ui::MouseScrollEvent{
                ui::Vec2((float)g_scrollX, (float)g_scrollY),
                ui::Vec2((float)g_input.mouseX, (float)g_input.mouseY)});
            g_scrollX = g_scrollY = 0;
        }

        ctx.BeginFrame(dt, (float)fbw, (float)fbh);
        bool blockedAny = false;
        for (const ui::InputEvent& e : g_input.events) {
            if (ctx.ProcessInput(e)) blockedAny = true;
        }
        (void)blockedAny;

        aris_demo::BuildDemoUi(ctx, demo);

        ctx.Render();

        // Debug aid: ARIS_DUMP=<frames> saves framebuffers as dump_<n>.png
        // (flipped to top-down) for headless visual verification.
        if (const char* dumpFrames = std::getenv("ARIS_DUMP")) {
            static int dumpTarget = std::atoi(dumpFrames);
            static int frameNo = 0;
            if (dumpTarget > 0 && ++frameNo == dumpTarget) {
                std::vector<unsigned char> px((size_t)fbw * fbh * 4);
                glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                std::vector<unsigned char> flipped(px.size());
                const int stride = fbw * 4;
                for (int y = 0; y < fbh; ++y) {
                    std::memcpy(&flipped[(size_t)y * stride], &px[(size_t)(fbh - 1 - y) * stride], stride);
                }
                const std::string path = "dump_" + std::to_string(frameNo) + ".png";
                stbi_write_png(path.c_str(), fbw, fbh, 4, flipped.data(), stride);
                std::printf("frame dumped to %s\n", path.c_str());
            }
        }

        glfwSwapBuffers(window);

        // fps title
        ++frames;
        if (now - fpsTimer >= 1.0) {
            char title[128];
            std::snprintf(title, sizeof(title), "aris-ui runner — %d fps — %s", frames,
                          std::filesystem::path(fontPath).filename().string().c_str());
            glfwSetWindowTitle(window, title);
            frames = 0;
            fpsTimer = now;
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}