#include "graphics/texture_manager.hpp"
#include "core/assert.hpp"
#include "graphics/graphics_api/sampler.hpp"
#include "graphics/graphics_api/texture.hpp"
#include "graphics/render_system.hpp"
#include "graphics/shader_c_shared/defines.h"
#include "graphics/vulkan.hpp"
#include "utils/logger.hpp"
#include <bitset>
#include <deque>

static uint16_t s_currentSlot;
static std::bitset< PG_MAX_NUM_TEXTURES > s_slotsInUse;
static std::deque< uint16_t > s_freeSlots;
static std::vector< VkWriteDescriptorSet > s_setWrites;
static std::vector< VkDescriptorImageInfo > s_imageInfos;
struct TexInfo
{
    VkImageView view;
    VkSampler sampler;
};

static std::vector< std::pair< uint16_t, TexInfo > > s_slotsAddedSinceLastUpdate;

namespace Progression
{
namespace Gfx
{
namespace TextureManager
{
    void Init()
    {
        s_setWrites.reserve( 256 );
        s_imageInfos.reserve( 256 );
        s_slotsAddedSinceLastUpdate.reserve( 256 );
        s_slotsInUse.reset();
        s_currentSlot = 0;
        s_freeSlots.clear();
    }

    void Shutdown()
    {
        s_setWrites.clear();
        s_imageInfos.clear();
        s_slotsAddedSinceLastUpdate.clear();
        s_slotsInUse.reset();
        s_currentSlot = 0;
        s_freeSlots.clear();
    }

    uint16_t GetOpenSlot( Texture* tex )
    {
        uint16_t openSlot;
        if ( s_freeSlots.empty() )
        {
            openSlot = s_currentSlot;
            ++s_currentSlot;
        }
        else
        {
            openSlot = s_freeSlots.back();
            s_freeSlots.pop_back();
        }
        PG_ASSERT( openSlot < PG_MAX_NUM_TEXTURES && !s_slotsInUse[openSlot] );
        s_slotsInUse[openSlot] = true;
        TexInfo info = { tex->GetView(), tex->GetSampler()->GetHandle() };
        s_slotsAddedSinceLastUpdate.emplace_back( openSlot, info );
        return openSlot;
    }

    void FreeSlot( uint16_t slot )
    {
        PG_ASSERT( s_slotsInUse[slot] );
        s_slotsInUse[slot] = false;
        s_freeSlots.push_front( slot );
    }

    void UpdateSampler( Texture* tex )
    {
        PG_ASSERT( tex && tex->GetShaderSlot() != PG_INVALID_TEXTURE_INDEX && s_slotsInUse[tex->GetShaderSlot()] );
        TexInfo info = { tex->GetView(), tex->GetSampler()->GetHandle() };
        s_slotsAddedSinceLastUpdate.emplace_back( tex->GetShaderSlot(), info );
    }

    void UpdateDescriptors( const DescriptorSet& textureDescriptorSet )
    {
        if ( s_slotsAddedSinceLastUpdate.empty() )
        {
            return;
        }

        s_setWrites.resize( s_slotsAddedSinceLastUpdate.size(), {} );
        s_imageInfos.resize( s_slotsAddedSinceLastUpdate.size() );
        for ( size_t i = 0; i < s_setWrites.size(); ++i )
        {
            s_imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            s_imageInfos[i].imageView   = s_slotsAddedSinceLastUpdate[i].second.view;
            s_imageInfos[i].sampler     = s_slotsAddedSinceLastUpdate[i].second.sampler;

            s_setWrites[i] = WriteDescriptorSet( textureDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &s_imageInfos[i], 1, static_cast< uint32_t >( s_slotsAddedSinceLastUpdate[i].first ) );
        }

        vkUpdateDescriptorSets( g_renderState.device.GetHandle(), static_cast< uint32_t >( s_setWrites.size() ), s_setWrites.data(), 0, nullptr );

        s_slotsAddedSinceLastUpdate.clear();
    }

} // namespace TextureManager
} // namespace Gfx
} // namespace Progression
