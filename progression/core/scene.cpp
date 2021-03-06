#include "core/scene.hpp"
#include "core/animation_system.hpp"
#include "core/assert.hpp"
#include "core/lua.hpp"
#include "core/time.hpp"
#include "components/factory.hpp"
#include "components/animation_component.hpp"
#include "components/script_component.hpp"
#include "resource/image.hpp"
#include "resource/resource_manager.hpp"
#include "utils/json_parsing.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace Progression;

static void ParseFastfile( rapidjson::Value& v, Scene* scene )
{
    static FunctionMapper< void > mapping(
    {
        { "filename", []( rapidjson::Value& v )
            {
                PG_ASSERT( v.IsString() );
                std::string fname = v.GetString();
                ResourceManager::LoadFastFile( PG_RESOURCE_DIR "cache/fastfiles/" + fname );
            }
        }
    });

    mapping.ForEachMember( v );
}

static void ParseCamera( rapidjson::Value& v, Scene* scene )
{
    Camera& camera = scene->camera;
    static FunctionMapper< void, Camera& > mapping(
    {
        { "position",    []( rapidjson::Value& v, Camera& camera ) { camera.position    = ParseVec3( v ); } },
        { "rotation",    []( rapidjson::Value& v, Camera& camera ) { camera.rotation    = glm::radians( ParseVec3( v ) ); } },
        { "fov",         []( rapidjson::Value& v, Camera& camera ) { camera.fov         = glm::radians( ParseNumber< float >( v ) ); } },
        { "aspectRatio", []( rapidjson::Value& v, Camera& camera ) { camera.aspectRatio = ParseNumber< float >( v ); } },
        { "nearPlane",   []( rapidjson::Value& v, Camera& camera ) { camera.nearPlane   = ParseNumber< float >( v ); } },
        { "farPlane",    []( rapidjson::Value& v, Camera& camera ) { camera.farPlane    = ParseNumber< float >( v ); } },
    });

    mapping.ForEachMember( v, camera );

    camera.UpdateFrustum();
    camera.UpdateProjectionMatrix();
}

static void ParseDirectionalLight( rapidjson::Value& value, Scene* scene )
{
    static FunctionMapper< void, ShadowMap& > shadowMapping(
    {
        { "constantBias", []( rapidjson::Value& v, ShadowMap& m ) { m.constantBias = ParseNumber< float >( v ); } },
        { "slopeBias",    []( rapidjson::Value& v, ShadowMap& m ) { m.slopeBias    = ParseNumber< float >( v ); } },
        { "width",        []( rapidjson::Value& v, ShadowMap& m ) { m.width        = ParseNumber< uint32_t >( v ); } },
        { "height",       []( rapidjson::Value& v, ShadowMap& m ) { m.height       = ParseNumber< uint32_t >( v ); } },
    });

    static FunctionMapper< void, DirectionalLight& > mapping(
    {
        { "colorAndIntensity", []( rapidjson::Value& v, DirectionalLight& l ) { l.colorAndIntensity = ParseVec4( v ); } },
        { "direction",         []( rapidjson::Value& v, DirectionalLight& l )
            { l.direction = glm::vec4( glm::normalize( ParseVec3( v ) ), 0 ); }
        },
        { "shadowMap", []( rapidjson::Value& v, DirectionalLight& l )
            {
                l.shadowMap = std::make_shared< ShadowMap >();
                shadowMapping.ForEachMember( v, *l.shadowMap );
                bool ret = l.shadowMap->Init();
                PG_ASSERT( ret, "Could not create directional light shadow map" );
            }
        }
    });
    mapping.ForEachMember( value, scene->directionalLight );
}

static void ParsePointLight( rapidjson::Value& value, Scene* scene )
{
    static FunctionMapper< void, PointLight& > mapping(
    {
        { "colorAndIntensity", []( rapidjson::Value& v, PointLight& l ) { l.colorAndIntensity = ParseVec4( v ); } },
        { "positionAndRadius", []( rapidjson::Value& v, PointLight& l ) { l.positionAndRadius = ParseVec4( v ); } },
    });
    PointLight p;
    scene->pointLights.emplace_back( p );
    mapping.ForEachMember( value, scene->pointLights[scene->pointLights.size() - 1] );
}

static void ParseSpotLight( rapidjson::Value& value, Scene* scene )
{
    static FunctionMapper< void, SpotLight& > mapping(
    {
        { "colorAndIntensity",  []( rapidjson::Value& v, SpotLight& l ) { l.colorAndIntensity = ParseVec4( v ); } },
        { "positionAndRadius",  []( rapidjson::Value& v, SpotLight& l ) { l.positionAndRadius = ParseVec4( v ); } },
        { "directionAndCutoff", []( rapidjson::Value& v, SpotLight& l )
            {
                l.directionAndCutoff = ParseVec4( v );
                glm::vec3 d          = glm::normalize( glm::vec3( l.directionAndCutoff ) );
                l.directionAndCutoff = glm::vec4( d, glm::radians( l.directionAndCutoff.w ) );
            }
        },
    });
    SpotLight p;
    scene->spotLights.emplace_back( p );
    mapping.ForEachMember( value, scene->spotLights[scene->spotLights.size() - 1] );
}

static void ParseEntity( rapidjson::Value& v, Scene* scene )
{
    auto e = scene->registry.create();
    for ( auto it = v.MemberBegin(); it != v.MemberEnd(); ++it )
    {
        ParseComponent( it->value, e, scene->registry, it->name.GetString() );
    }
}

static void ParseBackgroundColor( rapidjson::Value& v, Scene* scene )
{
    PG_ASSERT( v.HasMember( "color" ) );
    auto& member           = v["color"];
    scene->backgroundColor = ParseVec4( member );
}

static void ParseAmbientColor( rapidjson::Value& v, Scene* scene )
{
    PG_ASSERT( v.HasMember( "color" ) );
    auto& member        = v["color"];
    scene->ambientColor = ParseVec3( member );
}

static void ParseSkybox( rapidjson::Value& v, Scene* scene )
{
    PG_ASSERT( v.IsString() );
    scene->skybox = ResourceManager::Get< Image >( v.GetString() );
    PG_ASSERT( scene->skybox, "Could not find skybox with name '" + std::string( v.GetString() ) + "'" );
}

namespace Progression
{

Scene::~Scene()
{
    auto view = registry.view< Animator >();

    for ( auto entity : view )
    {
        registry.remove< Animator >( entity );
    }

    if ( directionalLight.shadowMap )
    {
        directionalLight.shadowMap->Free();
    }

    for ( auto& l : pointLights )
    {
        if ( l.shadowMap )
        {
            l.shadowMap->Free();
        }
    }
    for ( auto& l : spotLights )
    {
        if ( l.shadowMap )
        {
            l.shadowMap->Free();
        }
    }
}

Scene* Scene::Load( const std::string& filename )
{
    Scene* scene = new Scene;
    scene->directionalLight.colorAndIntensity.w = 0; // turn off the directional light to start

    auto document = ParseJSONFile( filename );
    if ( document.IsNull() )
    {
        delete scene;
        return nullptr;
    }

    static FunctionMapper< void, Scene* > mapping(
    {
        { "AmbientColor",     ParseAmbientColor },
        { "BackgroundColor",  ParseBackgroundColor },
        { "Camera",           ParseCamera },
        { "Entity",           ParseEntity },
        { "DirectionalLight", ParseDirectionalLight },
        { "PointLight",       ParsePointLight },
        { "SpotLight",        ParseSpotLight },
        { "Fastfile",         ParseFastfile },
        { "Skybox",           ParseSkybox },
    });

    mapping.ForEachMember( document, std::move( scene ) );

    scene->registry.on_construct< Animator >().connect< &AnimationSystem::OnAnimatorConstruction >();
    scene->registry.on_destroy< Animator >().connect< &AnimationSystem::OnAnimatorDestruction >();

    return scene;
}

void Scene::Start()
{
    g_LuaState["registry"] = &registry;
    g_LuaState["scene"] = this;
    registry.view< ScriptComponent >().each([]( const entt::entity e, ScriptComponent& comp )
    {
        for ( int i = 0; i < comp.numScripts; ++i )
        {
            auto startFn = comp.scripts[i].env["Start"];
            if ( startFn.valid() )
            {
                comp.scripts[i].env["entity"] = e;
                CHECK_SOL_FUNCTION_CALL( startFn() );
            }
        }
    });
}

void Scene::Update()
{
    auto luaTimeNamespace = g_LuaState["Time"].get_or_create< sol::table >();
    luaTimeNamespace["dt"] = Time::DeltaTime();
    g_LuaState["registry"] = &registry;
    registry.view< ScriptComponent >().each([]( const entt::entity e, ScriptComponent& comp )
    {
        for ( int i = 0; i < comp.numScriptsWithUpdate; ++i )
        {
            comp.scripts[i].env["entity"] = e;
            CHECK_SOL_FUNCTION_CALL( comp.scripts[i].updateFunc.second() );
        }
    });

    AnimationSystem::Update( this );
}

} // namespace Progression
