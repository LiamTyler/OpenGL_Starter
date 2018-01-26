#include "include/utils.h"
#include "include/camera.h"
#include "include/glsl_shader.h"

using namespace std;

int main(int arc, char** argv) {
    // initialize SDL and GLEW and set up window
    SDL_Window* window = InitAndWindow("Starter Project", 100, 100, 800, 600);
    cout << "vendor: " << glGetString(GL_VENDOR) << endl;
    cout << "renderer: " << glGetString(GL_RENDERER) << endl;
    cout << "version: " << glGetString(GL_VERSION) << endl;

    // set up the particle system
    Camera camera = Camera(vec3(0, 0, 5), vec3(0, 0, -1), vec3(0, 1, 0));
    GLSLShader shader;
    shader.LoadFromFile(GL_VERTEX_SHADER,   "shaders/plain_shader.vert");
    shader.LoadFromFile(GL_FRAGMENT_SHADER, "shaders/plain_shader.frag");
    shader.CreateAndLinkProgram();
    shader.Enable();
    shader.AddAttribute("pos");
    shader.AddUniform("model");
    shader.AddUniform("view");
    shader.AddUniform("proj");
    static const GLfloat quad_verts[] = {
        -.5, .5, 0,
        -.5, -.5, 0,
        .5, -.5, 0,
        .5, -.5, 0,
        .5, .5, 0,
        -.5, .5, 0,
    };
    GLuint vao;
    GLuint vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(shader["pos"]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(shader["pos"], 3, GL_FLOAT, GL_FALSE, 0, 0);

    bool quit = false;
    SDL_Event event;
    float currTime = 0;
    float prevTime = 0;
    float fpsTime = 0;
    unsigned int frameCounter = 0;
    SDL_SetRelativeMouseMode(SDL_TRUE);
    while (!quit) {
        // Process all input events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                // key down events (wont repeat if holding key down)
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                        camera.VelZ(1.0f);
                        break;
                    case SDLK_s:
                        camera.VelZ(-1.0f);
                        break;
                    case SDLK_a:
                        camera.VelX(-1.0f);
                        break;
                    case SDLK_d:
                        camera.VelX(1.0f);
                        break;
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_p:
                        break;
                    case SDLK_SPACE:
                        break;
                }
            } else if (event.type == SDL_KEYUP) {
                // handle key up events
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                    case SDLK_s:
                        camera.VelZ(0.0f);
                        break;
                    case SDLK_a:
                    case SDLK_d:
                        camera.VelX(0.0f);
                        break;
                    case SDLK_SPACE:
                        break;
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                // handle mouse events
                float dx = event.motion.xrel;
                float dy = event.motion.yrel;
                camera.RotateX(-dy);
                camera.RotateY(-dx);
                camera.UpdateAxis();
            }
        }

        currTime = SDL_GetTicks() / 1000.0f;
        float dt = currTime - prevTime;
        camera.Update(dt);

        // shader.Enable();
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 model(1);
        mat4 v = camera.View();
        mat4 p = camera.Proj();
        glUniformMatrix4fv(shader["model"], 1, GL_FALSE, value_ptr(model));
        glUniformMatrix4fv(shader["view"], 1,  GL_FALSE, value_ptr(v));
        glUniformMatrix4fv(shader["proj"], 1,  GL_FALSE, value_ptr(p));

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // swap windows and update time
        prevTime = currTime;
        ++frameCounter;
        if (currTime > fpsTime + 1) {
            cout << "FPS: " << frameCounter << endl;
            frameCounter = 0;
            fpsTime = currTime;
        }

        SDL_GL_SwapWindow(window);
    }

    // Clean up
    SDL_Quit();

    return 0;
}
