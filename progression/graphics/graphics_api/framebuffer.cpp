#include "graphics/graphics_api/framebuffer.hpp"
#include "core/assert.hpp"

namespace Progression
{
namespace Gfx
{

    void Framebuffer::Free()
    {
        PG_ASSERT( m_handle != VK_NULL_HANDLE );
        vkDestroyFramebuffer( m_device, m_handle, nullptr );
        m_handle = VK_NULL_HANDLE;
    }
        
    VkFramebuffer Framebuffer::GetHandle() const
    {
        return m_handle;
    }
    
    Framebuffer::operator bool() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

} // namespace Gfx
} // namespace Progression
