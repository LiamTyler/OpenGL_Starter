#include "core/camera.hpp"

namespace Progression
{

Camera::Camera()
{
    UpdateFrustum();
    UpdateProjectionMatrix();
}

void Camera::UpdateFrustum()
{
    UpdateOrientationVectors();
    UpdateViewMatrix();
    m_frustum.Update( fov, nearPlane, farPlane, aspectRatio, position, m_currDir, m_currUp,
                     m_currRight );
}

void Camera::UpdateOrientationVectors()
{
    glm::mat4 rot( 1 );
    // TODO: glm can do rotation around an axis already
    rot         = glm::rotate( rot, rotation.y, glm::vec3( 0, 1, 0 ) );
    rot         = glm::rotate( rot, rotation.x, glm::vec3( 1, 0, 0 ) );
    m_currDir   = glm::vec3( rot * glm::vec4( 0, 0, -1, 0 ) );
    m_currUp    = glm::vec3( rot * glm::vec4( 0, 1, 0, 0 ) );
    m_currRight = glm::cross( m_currDir, m_currUp );
}

void Camera::UpdateViewMatrix()
{
    m_viewMatrix = glm::lookAt( position, position + m_currDir, m_currUp );
}

void Camera::UpdateProjectionMatrix()
{
    m_projectionMatrix        = glm::perspective( fov, aspectRatio, nearPlane, farPlane );
    // m_projectionMatrix[1][1] *= -1;
}

glm::mat4 Camera::GetV() const
{
    return m_viewMatrix;
}
glm::mat4 Camera::GetP() const
{
    return m_projectionMatrix;
}
glm::mat4 Camera::GetVP() const
{
    return m_projectionMatrix * m_viewMatrix;
}

glm::vec3 Camera::GetForwardDir() const
{
    return m_currDir;
}
glm::vec3 Camera::GetUpDir() const
{
    return m_currUp;
}
glm::vec3 Camera::GetRightDir() const
{
    return m_currRight;
}
Frustum Camera::GetFrustum() const
{
    return m_frustum;
}

} // namespace Progression
