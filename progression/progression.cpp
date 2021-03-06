#include "progression.hpp"
#include <ctime>

namespace PG = Progression;

namespace Progression
{

bool g_engineShutdown = false;
bool g_converterMode  = false;

bool EngineInitialize( std::string config_name )
{
    if ( config_name == "" )
    {
        config_name = PG_ROOT_DIR "configs/default.toml";
    }

    auto conf = config::Config( config_name );
    if ( !conf )
    {
        LOG_ERR( "Failed to load the config file: ", config_name );
        exit( EXIT_FAILURE );
    }

    // logger
    {
        auto logConfig = conf->get_table( "logger" );
        PG_ASSERT( logConfig );
        PG_ASSERT( logConfig->contains( "file" ) );
        PG_ASSERT( logConfig->contains( "useColors" ) );

        std::string filename = *logConfig->get_as< std::string >( "file" );
        if ( filename != "" )
        {
            filename = PG_ROOT_DIR + filename;
        }
        bool colors = *logConfig->get_as< bool >( "useColors" );

        g_Logger.Init( filename, colors );
    }

    RegisterTypesAndFunctionsToLua( g_LuaState );

    Random::SetSeed( time( NULL ) );

    // window
    {
        auto winConfig = conf->get_table( "window" );
        PG_ASSERT( winConfig );
        PG_ASSERT( winConfig->contains( "title" ) );
        PG_ASSERT( winConfig->contains( "width" ) );
        PG_ASSERT( winConfig->contains( "height" ) );
        PG_ASSERT( winConfig->contains( "visible" ) );
        PG_ASSERT( winConfig->contains( "vsync" ) );
        WindowCreateInfo winCreate;
        winCreate.title   = *winConfig->get_as< std::string >( "title" );
        winCreate.width   = *winConfig->get_as< int >( "width" );
        winCreate.height  = *winConfig->get_as< int >( "height" );
        winCreate.visible = *winConfig->get_as< bool >( "visible" );
        winCreate.vsync   = *winConfig->get_as< bool >( "vsync" );
        InitWindowSystem( winCreate );
    }
    Time::Reset();
    Input::Init();
    Gfx::TextureManager::Init();
    if ( !Gfx::VulkanInit() )
    {
        LOG_ERR( "Could not initialize vulkan" );
        return false;
    }
    ResourceManager::Init();
    if ( !g_converterMode )
    {
        if ( !ResourceManager::LoadFastFile( PG_RESOURCE_DIR "cache/fastfiles/engine_required_resources.ff" ) )
        {
            return false;
        }
        if ( !RenderSystem::Init() )
        {
            LOG_ERR( "Could not initialize the rendering system" );
            return false;
        }
        AnimationSystem::Init();
    }

    return true;
}

void EngineQuit()
{
    if ( !g_converterMode )
    {
        AnimationSystem::Shutdown();
        RenderSystem::Shutdown();
    }
    ResourceManager::Shutdown();
    Gfx::VulkanShutdown();
    Gfx::TextureManager::Shutdown();
    Input::Free();
    ShutdownWindowSystem();
    g_Logger.Shutdown();
}

} // namespace Progression
