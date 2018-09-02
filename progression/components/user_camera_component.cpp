#include "components/user_camera_component.h"
#include "core/input.h"

namespace Progression {

    UserCameraComponent::UserCameraComponent(Camera* camera, float ms, float ts, float ma) :
        Component(camera),
        moveSpeed(ms),
        turnSpeed(ts),
        maxAngle(glm::radians(ma)),
        velocity(glm::vec3(0))
    {
    }

    void UserCameraComponent::Start() {

    }

    void UserCameraComponent::Update(float dt) {
        Camera& camera = *(Camera*)gameObject;

        if (Input::GetKeyDown(PG_K_W)) {
            velocity.z = 1;
        }
        else if (Input::GetKeyDown(PG_K_S)) {
            velocity.z = -1;
        }
        else if (Input::GetKeyDown(PG_K_D)) {
            velocity.x = 1;
        }
        else if (Input::GetKeyDown(PG_K_A)) {
            velocity.x = -1;
        }

        if (Input::GetKeyUp(PG_K_W)) {
            velocity.z = 0;
        }
        else if (Input::GetKeyUp(PG_K_S)) {
            velocity.z = 0;
        }
        else if (Input::GetKeyUp(PG_K_D)) {
            velocity.x = 0;
        }
        else if (Input::GetKeyUp(PG_K_A)) {
            velocity.x = 0;
        }

        glm::ivec2 dMouse = -Input::GetMouseChange();
        glm::vec3 dRotation(glm::vec3(dMouse.y, dMouse.x, 0));

        camera.transform.rotation += dRotation * turnSpeed;
        camera.transform.rotation.x = std::fmax(-maxAngle, std::fmin(maxAngle, camera.transform.rotation.x));
        camera.UpdateOrientationVectors();
        
        camera.transform.position += dt * moveSpeed *
            (velocity.x * camera.GetRightDir() + velocity.y * camera.GetUpDir() + velocity.z * camera.GetForwardDir());
        camera.UpdateViewMatrix();
    }

    void UserCameraComponent::Stop() {

    }

} // namespace Progression