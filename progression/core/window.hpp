#pragma once

#include "utils/noncopyable.hpp"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"
#include <string>

namespace Progression
{

struct WindowCreateInfo
{
    std::string title = "Untitled";
    int width         = 1280;
    int height        = 720;
    bool visible      = true;
    bool debugContext = true;
    bool vsync        = false;
};

class Window : public NonCopyable
{
public:
    Window() = default;
    ~Window();

    void Init( const struct WindowCreateInfo& createInfo );
    void Shutdown();
    void StartFrame();
    void EndFrame();

    GLFWwindow* GetGLFWHandle() const { return m_window; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    void SetRelativeMouse( bool b );
    bool IsRelativeMouse() const { return m_relativeMouse; }
    void SetTitle( const std::string& title );

protected:
    GLFWwindow* m_window = nullptr;
    std::string m_title  = "";
    int m_width          = 0;
    int m_height         = 0;
    bool m_visible       = false;
    bool m_relativeMouse = false;
};

void InitWindowSystem( const WindowCreateInfo& info );
void ShutdownWindowSystem();
Window* GetMainWindow();

} // namespace Progression

struct lua_State;
void RegisterLuaFunctions_Window( lua_State* L );