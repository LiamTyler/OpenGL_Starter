#pragma once

#include "core/input_types.hpp"
#include "core/math.hpp"

struct lua_State;
void RegisterLuaFunctions_Input( lua_State* L );

namespace Progression
{
namespace Input
{

    void Init();
    void Free();

    // should be called once per frame to update inputs
    void PollEvents();

    // Get[Key/MouseButton]Down returns true the first frame the Key/MouseButton is pressed
    // Get[Key/MouseButton]Up returns true the first frame the Key/MouseButton is released
    // Get[Key/MouseButton]Held returns true all frames the Key/MouseButton is pressed, including the first frame
    bool GetKeyDown( Key k );
    bool GetKeyUp( Key k );
    bool GetKeyHeld( Key k );
    bool GetMouseButtonDown( MouseButton b );
    bool GetMouseButtonUp( MouseButton b );
    bool GetMouseButtonHeld( MouseButton b ); 
    glm::vec2 GetMousePosition();
    glm::vec2 GetMouseChange();
    glm::vec2 GetScrollChange();

} // namespace Input
} // namespace Progression
