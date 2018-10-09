#include "progression.h"
#include <chrono>

#define BLOCK_SIZE 16
using namespace Progression;

std::string rootDirectory;

class LightBallComponent : public Component {
public:
    LightBallComponent(GameObject* obj, GameObject* _ball) :
        Component(obj),
        ball(_ball)
    {
    }

    ~LightBallComponent() = default;
    void Start() {}
    void Stop() {}

    void Update() {
        gameObject->transform = ball->transform;
        gameObject->boundingBox.setCenter(gameObject->transform.position);
    }


    GameObject* ball;
};

class BounceComponent : public Component {
public:
    BounceComponent(GameObject* obj, const glm::vec3 startVel = glm::vec3(0, 0, 0)) :
        Component(obj),
        velocity(startVel)
    {
    }

    ~BounceComponent() = default;
    void Start() {}
    void Stop() {}

    void Update() {
        float dt = 1.0f / 30.0f;
        //float dt = Time::deltaTime();
        velocity.y += -9.81f * dt;
        gameObject->transform.position += velocity * dt;
        if (gameObject->transform.position.y < gameObject->transform.scale.x) {
            gameObject->transform.position.y = gameObject->transform.scale.x;
            velocity.y *= -.97;
        }
        gameObject->boundingBox.setCenter(gameObject->transform.position);
    }

    glm::vec3 velocity;
};

// argv[1] = path of the root directory
int main(int argc, char* argv[]) {
    srand(time(NULL));

    rootDirectory = "C:/Users/Tyler/Documents/Progression/";

    auto& conf = PG::config::Config(rootDirectory + "configs/default.yaml");

    PG::EngineInitialize(conf);

    Shader deferredShader = Shader(rootDirectory + "resources/shaders/deferred_phong.vert", rootDirectory + "resources/shaders/deferred_phong.frag");
    Shader combineShader = Shader(rootDirectory + "resources/shaders/deferred_combine.vert", rootDirectory + "resources/shaders/deferred_combine.frag");
    Shader lightVolumeShader = Shader(rootDirectory + "resources/shaders/lightVolume.vert", rootDirectory + "resources/shaders/lightVolume.frag");
    Shader instanceShader = Shader(rootDirectory + "resources/shaders/deferred_instance.vert", rootDirectory + "resources/shaders/deferred_instance.frag");
    
    float quadVerts[] = {
        -1, 1,
        -1, -1,
        1, -1,

        -1, 1,
        1, -1,
        1, 1
    };
    GLuint quadVAO;
    GLuint quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(combineShader["vertex"]);
    glVertexAttribPointer(combineShader["vertex"], 2, GL_FLOAT, GL_FALSE, 0, 0);

    auto scene = Scene::Load(rootDirectory + "resources/scenes/scene1.pgscn");

    auto camera = scene->GetCamera();
    camera->AddComponent<UserCameraComponent>(new UserCameraComponent(camera, 15));

    auto ballModel = ResourceManager::GetModel("metalBall");
    auto planeModel = ResourceManager::GetModel("metalFloor");

    int X, Z, startX, startZ;
    float DX, DZ;

    bool single = false;
    bool fourH = false;
    bool oneK = false;
    bool fourK = false;
    bool tenK = true;

    if (single) {
        X = 1;
        Z = 1;
        startX = 0;
        startZ = 0;
        DX = 10;
        DZ = 10;
    }
    if (fourH) {
        X = 20;
        Z = 20;
        startX = -X;
        startZ = -Z;
        DX = 1;
        DZ = 1;
    }
    if (oneK) {
        X = 32;
        Z = 32;
        startX = -X;
        startZ = -Z;
        DX = 1;
        DZ = 1;
    }
    if (fourK) {
        X = 64;
        Z = 64;
        startX = -X;
        startZ = -Z;
        DX = 1;
        DZ = 1;
    }
    if (tenK) {
        X = 100;
        Z = 100;
        startX = -X;
        startZ = -Z;
        DX = 1;
        DZ = 1;
    }

    float intensity = 2;
    float cutOffIntensity = 0.03;
    for (float x = 0; x < X; x += DX) {
        for (float z = 0; z < Z; z += DZ) {
            float randHeight = 3 + 2 * (rand() / static_cast<float>(RAND_MAX));
            glm::vec3 pos = glm::vec3(startX + 2 * x, randHeight, startZ + 2 * z);
            GameObject* ballObj = new GameObject(Transform(pos, glm::vec3(0), glm::vec3(.5)));
            ballObj->AddComponent<ModelRenderer>(new ModelRenderer(ballObj, ballModel.get()));
            ballObj->AddComponent<BounceComponent>(new BounceComponent(ballObj));
            ballObj->boundingBox.Encompass(BoundingBox(glm::vec3(-1), glm::vec3(1)), ballObj->transform);

            glm::vec3 randColor = glm::vec3((rand() / static_cast<float>(RAND_MAX)), (rand() / static_cast<float>(RAND_MAX)), (rand() / static_cast<float>(RAND_MAX)));
            PG::Light* pl = new Light(PG::Light::Type::POINT, pos, randColor, intensity);
            pl->AddComponent<LightBallComponent>(new LightBallComponent(pl, ballObj));
            float lightRadius = std::sqrtf(intensity / cutOffIntensity);
            // lightRadius = 4;
            pl->boundingBox.Encompass(BoundingBox(glm::vec3(-1), glm::vec3(1)), Transform(pl->transform.position, glm::vec3(0), glm::vec3(lightRadius)));

            scene->AddGameObject(ballObj);
            scene->AddLight(pl);
        }
    }
    std::cout << "light Radius = " << std::sqrtf(intensity / cutOffIntensity) << std::endl;
    
    GameObject* planeObj = scene->GetGameObject("floor");
    planeObj->AddComponent<ModelRenderer>(new ModelRenderer(planeObj, planeModel.get()));
    float e = 100;
    planeObj->boundingBox = BoundingBox(glm::vec3(-e, -.1, -e), glm::vec3(e, .1, e));
    
    auto skybox = scene->getSkybox();

    GLuint lightVolumeVAO = graphics::CreateVAO();
    GLuint* lightVolumeVBOs = ballModel->meshes[0]->getBuffers();

    glBindBuffer(GL_ARRAY_BUFFER, lightVolumeVBOs[0]);
    glEnableVertexAttribArray(lightVolumeShader["vertex"]);
    glVertexAttribPointer(lightVolumeShader["vertex"], 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lightVolumeVBOs[3]);

    GLuint gBuffer = graphics::CreateFrameBuffer();
    GLuint gPosition = graphics::Create2DTexture(Window::getWindowSize().x, Window::getWindowSize().y, GL_RGBA32F);
    GLuint gNormal = graphics::Create2DTexture(Window::getWindowSize().x, Window::getWindowSize().y, GL_RGBA32F);
    GLuint gDiffuse = graphics::Create2DTexture(Window::getWindowSize().x, Window::getWindowSize().y, GL_RGBA);
    GLuint gSpecularExp = graphics::Create2DTexture(Window::getWindowSize().x, Window::getWindowSize().y, GL_RGBA32F);

    graphics::AttachColorTexturesToFBO({ gPosition, gNormal, gDiffuse, gSpecularExp });

    GLuint rboDepth = graphics::CreateRenderBuffer(Window::getWindowSize().x, Window::getWindowSize().y);
    graphics::AttachRenderBufferToFBO(rboDepth);
    graphics::FinalizeFBO();

    graphics::BindFrameBuffer();

    // Note: After changing the input mode, should poll for events again
    Window::SetRelativeMouse(true);
    PG::Input::PollEvents();

    graphics::ToggleDepthTesting(true);
    Time::Restart();

    Shader lineShader = Shader(rootDirectory + "resources/shaders/line_shader.vert", rootDirectory + "resources/shaders/line_shader.frag");

    float bbData[] = {
        -1, -1, -1,
        1, -1, -1,
        1, -1, -1,
        1, -1, 1,
        1, -1, 1,
        -1, -1, 1,
        -1, -1, 1,
        -1, -1, -1,

        -1, -1, -1,
        -1, 1, -1,
        1, -1, -1,
        1, 1, -1,
        1, -1, 1,
        1, 1, 1,
        -1, -1, 1,
        -1, 1, 1,

        -1, 1, -1,
        1, 1, -1,
        1, 1, -1,
        1, 1, 1,
        1, 1, 1,
        -1, 1, 1,
        -1, 1, 1,
        -1, 1, -1
    };

    GLuint bbVao = graphics::CreateVAO();
    GLuint bbVbo;
    glGenBuffers(1, &bbVbo);
    glBindBuffer(GL_ARRAY_BUFFER, bbVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bbData), bbData, GL_STATIC_DRAW);

    glEnableVertexAttribArray(lineShader["vertex"]);
    glVertexAttribPointer(lineShader["vertex"], 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bbVbo);

    Shader computeShader;
    computeShader.AttachShaderFromFile(GL_COMPUTE_SHADER, rootDirectory + "resources/shaders/compute.glsl");
    computeShader.CreateAndLinkProgram();
    std::cout << "compute shader uniforms" << std::endl;
    computeShader.AutoDetectVariables();

    {
        std::cout << "vendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "version: " << glGetString(GL_VERSION) << std::endl;
        glm::ivec3 work_group_cnt;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_group_cnt[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_group_cnt[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_group_cnt[2]);

        std::cout << "max global work group count: " << work_group_cnt.x << ", " << work_group_cnt.y
            << ", " << work_group_cnt.z << std::endl;

        glm::ivec3 work_group_size;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_group_size[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_group_size[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_group_size[2]);

        std::cout << "max global work group size: " << work_group_size.x << ", " << work_group_size.y
            << ", " << work_group_size.z << std::endl;

        int work_group_invocations;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_group_invocations);
        std::cout << "max local work group invocations: " << work_group_invocations << std::endl;
    }


    GLuint computeOutput;
    glGenTextures(1, &computeOutput);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, computeOutput);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Window::getWindowSize().x, Window::getWindowSize().y, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindImageTexture(0, computeOutput, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // point
    GLuint lightSSBO;
    glGenBuffers(1, &lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(glm::vec4) * scene->GetNumPointLights(), NULL, GL_STREAM_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, lightSSBO);
    GLint bufMask = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    glm::vec4* lightBuf = (glm::vec4*) glMapBufferRange( GL_SHADER_STORAGE_BUFFER, 0, 2 * sizeof(glm::vec4) * scene->GetNumPointLights(), bufMask);
    for( int i = 0; i < scene->GetNumPointLights(); i++) {
        float cutOffIntensity = 0.03;
        float lightRadius = std::sqrtf(scene->GetPointLights()[i]->intensity / cutOffIntensity);
        // lightBuffer[2 * i + 0] = camera->GetV() * glm::vec4(scene->GetPointLights()[i]->transform.position, 1);
        lightBuf[2 * i + 0] = camera->GetV() * glm::vec4(scene->GetPointLights()[i]->transform.position, 1);
        lightBuf[2 * i + 0].w = lightRadius;
        lightBuf[2 * i + 1] = glm::vec4(scene->GetPointLights()[i]->intensity * scene->GetPointLights()[i]->color, 1);
    }
    glUnmapBuffer( GL_SHADER_STORAGE_BUFFER );


    GLuint instanceVBO;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * 10000, NULL, GL_STREAM_DRAW);

    GLuint instanceVAO;
    glGenVertexArrays(1, &instanceVAO);
    glBindVertexArray(instanceVAO);
    GLuint* vbos = ballModel->meshes[0]->getBuffers();

    glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
    glEnableVertexAttribArray(instanceShader["vertex"]);
    glVertexAttribPointer(instanceShader["vertex"], 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
    glEnableVertexAttribArray(instanceShader["normal"]);
    glVertexAttribPointer(instanceShader["normal"], 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vbos[2]);
    glEnableVertexAttribArray(instanceShader["inTexCoord"]);
    glVertexAttribPointer(instanceShader["inTexCoord"], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos[3]);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    glEnableVertexAttribArray(instanceShader["MV"] + 0);
    glEnableVertexAttribArray(instanceShader["MV"] + 1);
    glEnableVertexAttribArray(instanceShader["MV"] + 2);
    glEnableVertexAttribArray(instanceShader["MV"] + 3);

    glVertexAttribPointer(instanceShader["MV"] + 0, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), 0);
    glVertexAttribPointer(instanceShader["MV"] + 1, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*) (4 * sizeof(float)));
    glVertexAttribPointer(instanceShader["MV"] + 2, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*) (8 * sizeof(float)));
    glVertexAttribPointer(instanceShader["MV"] + 3, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*) (12 * sizeof(float)));
    
    glVertexAttribDivisor(instanceShader["vertex"], 0);
    glVertexAttribDivisor(instanceShader["normal"], 0);
    glVertexAttribDivisor(instanceShader["inTexCoord"], 0);
    glVertexAttribDivisor(instanceShader["MV"] + 0, 1);
    glVertexAttribDivisor(instanceShader["MV"] + 1, 1);
    glVertexAttribDivisor(instanceShader["MV"] + 2, 1);
    glVertexAttribDivisor(instanceShader["MV"] + 3, 1);

    int counter = 0;
    double sphereTotal = 0, lightTotal = 0, computeTotal = 0, updateTotal = 0, finishTotal = 0;
    // Game loop
    while (!PG::EngineShutdown) {
        PG::Window::StartFrame();
        PG::Input::PollEvents();

        if (PG::Input::GetKeyDown(PG::PG_K_ESC))
            PG::EngineShutdown = true;

        scene->Update();

        // RenderSystem::UpdateLights(scene, camera);

        graphics::BindFrameBuffer(gBuffer);

        graphics::SetClearColor(glm::vec4(0));
        graphics::Clear();

        deferredShader.Enable();
        RenderSystem::UploadCameraProjection(deferredShader, *camera);

        const auto& gameObjects = scene->GetGameObjects();
        
        for (const auto& obj : scene->GetGameObjects()) {
            glm::mat4 M = obj->transform.GetModelMatrix();
            glm::mat4 MV = camera->GetV() * M;
            glm::mat4 normalMatrix = glm::transpose(glm::inverse(MV));
            glUniformMatrix4fv(deferredShader["modelViewMatrix"], 1, GL_FALSE, glm::value_ptr(MV));
            glUniformMatrix4fv(deferredShader["normalMatrix"], 1, GL_FALSE, glm::value_ptr(normalMatrix));
            for (const auto& mr : obj->GetComponent<ModelRenderer>()->meshRenderers) {
                glBindVertexArray(mr->vao);
                RenderSystem::UploadMaterial(deferredShader, *mr->material);
                glDrawElements(GL_TRIANGLES, mr->mesh->getNumTriangles() * 3, GL_UNSIGNED_INT, 0);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        graphics::Clear();

        // blit the deferred depth buffer to the main screen
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, Window::getWindowSize().x, Window::getWindowSize().y, 0, 0, Window::getWindowSize().x, Window::getWindowSize().y, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glm::mat4 VP = camera->GetP() * camera->GetV();
        glm::mat4 V = camera->GetV();
        const auto& pointLights = scene->GetPointLights();
        
        glm::vec4* lightBuffer = (glm::vec4*) glMapBufferRange( GL_SHADER_STORAGE_BUFFER, 0, 2 * sizeof(glm::vec4) * scene->GetNumPointLights(), bufMask);
        for (int i = 0; i < pointLights.size(); ++i) {
            float lightRadius = std::sqrtf(scene->GetPointLights()[i]->intensity / cutOffIntensity);
            //lightRadius = 6;
            //lightBuffer[2 * i + 0] = camera->GetV() * glm::vec4(pointLights[i]->transform.position, lightRadius);
            lightBuffer[2 * i + 0] = V * glm::vec4(pointLights[i]->transform.position, 1);
            lightBuffer[2 * i + 0].w = lightRadius;
            lightBuffer[2 * i + 1] = glm::vec4(pointLights[i]->intensity * pointLights[i]->color, 1);
        }
        glUnmapBuffer( GL_SHADER_STORAGE_BUFFER );

        computeShader.Enable();
        glBindImageTexture(0, computeOutput, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(1, gPosition, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(2, gNormal, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
        glBindImageTexture(3, gDiffuse, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
        glBindImageTexture(4, gSpecularExp, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

        glUniform2i(computeShader["screenSize"], Window::getWindowSize().x, Window::getWindowSize().y);
        glUniform1i(computeShader["numPointLights"], scene->GetNumPointLights());
        glUniformMatrix4fv(computeShader["invProjMatrix"], 1, GL_FALSE, glm::value_ptr(glm::inverse(camera->GetP())));

        glDispatchCompute(Window::getWindowSize().x / BLOCK_SIZE, Window::getWindowSize().y / BLOCK_SIZE, 1);
        // glDispatchCompute(80, 45, 1);

        // make sure writing to image has finished before read
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        // glMemoryBarrier(GL_ALL_BARRIER_BITS);

        graphics::ToggleDepthBufferWriting(false);
        graphics::ToggleDepthTesting(false);

        combineShader.Enable();
        glBindVertexArray(quadVAO);
        graphics::Bind2DTexture(computeOutput, combineShader["computeOutput"], 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        graphics::ToggleDepthTesting(true);
        graphics::ToggleDepthBufferWriting(true);

        skybox->Render(*camera);

        //graphics::ToggleDepthBufferWriting(false);
        //graphics::ToggleDepthTesting(false);
        
        /*
        lineShader.Enable();
        glUniform3fv(lineShader["color"], 1, glm::value_ptr(glm::vec3(0, 1, 0)));
        glBindVertexArray(bbVao);
        int drawn = 0;
        
        //Frustum frustum(*camera);
        //glm::mat4 M = planeObj->boundingBox.GetModelMatrix();
        //glUniformMatrix4fv(lineShader["MVP"], 1, GL_FALSE, glm::value_ptr(VP * M));
        //glDrawArrays(GL_LINES, 0, 24);
        
        for (const auto& obj : scene->GetPointLights()) {
            if (frustum.boxInFrustum(obj->boundingBox)) {
                drawn++;
                glm::mat4 M = obj->boundingBox.GetModelMatrix();
                glUniformMatrix4fv(lineShader["MVP"], 1, GL_FALSE, glm::value_ptr(VP * M));
                // glDrawArrays(GL_LINES, 0, 24);
            }
        }
        */
        
        PG::Window::EndFrame();
    }
    /*std::cout << "Average scene update time: " << updateTotal / counter << std::endl;
    std::cout << "Average sphere rendering time: " << sphereTotal / counter << std::endl;
    std::cout << "Average light upload time: " << lightTotal / counter << std::endl;
    std::cout << "Average compute shader time: " << computeTotal / counter << std::endl;
    std::cout << "Average final stages time: " << finishTotal / counter << std::endl;*/

    PG::EngineQuit();

    return 0;
}