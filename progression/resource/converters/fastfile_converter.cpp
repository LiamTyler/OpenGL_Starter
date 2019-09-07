#include "core/feature_defines.hpp"
#include "graphics/graphics_api.hpp"
#include "lz4/lz4.h"
#include "memory_map/MemoryMapped.h"
#include "resource/converters/fastfile_converter.hpp"
#include "resource/shader.hpp"
#include "resource/texture.hpp"
#include "resource/model.hpp"
#include "utils/fileIO.hpp"
#include "utils/logger.hpp"
#include "utils/serialize.hpp"
#include "utils/timestamp.hpp"
#include <filesystem>
#include <fstream>

using namespace Progression;

bool ParseShaderCreateInfoFromFile( std::istream& in, ShaderCreateInfo& info )
{
    fileIO::ParseLineKeyVal( in, "name", info.name );
    fileIO::ParseLineKeyValOptional( in, "vertex", info.vertex );
    fileIO::ParseLineKeyValOptional( in, "geometry", info.geometry );
    fileIO::ParseLineKeyValOptional( in, "fragment", info.fragment );
    fileIO::ParseLineKeyValOptional( in, "compute", info.compute );
    if ( !info.vertex.empty() )
    {
        info.vertex = PG_RESOURCE_DIR + info.vertex;
    }
    if ( !info.geometry.empty() )
    {
        info.geometry = PG_RESOURCE_DIR + info.geometry;
    }
    if ( !info.fragment.empty() )
    {
        info.fragment = PG_RESOURCE_DIR + info.fragment;
    }
    if ( !info.compute.empty() )
    {
        info.compute = PG_RESOURCE_DIR + info.compute;
    }

    return true;
}

static std::unordered_map< std::string, PixelFormat > internalFormatMap = {
    { "R8_Uint", PixelFormat::R8_Uint },
    { "R16_Float", PixelFormat::R16_Float },
    { "R32_Float", PixelFormat::R32_Float },
    { "R8_G8_Uint", PixelFormat::R8_G8_Uint },
    { "R16_G16_Float", PixelFormat::R16_G16_Float },
    { "R32_G32_Float", PixelFormat::R32_G32_Float },
    { "R8_G8_B8_Uint", PixelFormat::R8_G8_B8_Uint },
    { "R16_G16_B16_Float", PixelFormat::R16_G16_B16_Float },
    { "R32_G32_B32_Float", PixelFormat::R32_G32_B32_Float },
    { "R8_G8_B8_A8_Uint", PixelFormat::R8_G8_B8_A8_Uint },
    { "R16_G16_B16_A16_Float", PixelFormat::R16_G16_B16_A16_Float },
    { "R32_G32_B32_A32_Float", PixelFormat::R32_G32_B32_A32_Float },
    { "R8_G8_B8_Uint_sRGB", PixelFormat::R8_G8_B8_Uint_sRGB },
    { "R8_G8_B8_A8_Uint_sRGB", PixelFormat::R8_G8_B8_A8_Uint_sRGB },
    { "R11_G11_B10_Float", PixelFormat::R11_G11_B10_Float },
    { "DEPTH32_Float", PixelFormat::DEPTH32_Float },
};

bool ParseTextureCreateInfoFromFile( std::istream& in, TextureCreateInfo& info )
{
    PG_UNUSED( in );
    PG_UNUSED( info );
    /*
    info.texDesc.type = Gfx::TextureType::TEXTURE2D;

    fileIO::ParseLineKeyVal( in, "name", info.name );
    fileIO::ParseLineKeyVal( in, "filename", info.filename );
    info.filename          = PG_RESOURCE_DIR + info.filename;
    fileIO::ParseLineKeyVal( in, "mipmapped", info.texDesc.mipmapped );
    if ( !info.texDesc.mipmapped && ( info.samplerDesc.minFilter != Gfx::FilterMode::NEAREST &&
                                      info.samplerDesc.minFilter != Gfx::FilterMode::LINEAR ) )
    {
        LOG_ERR( "Trying to use a mipmap min filter when there is no mip map on texture: ",
                 info.filename );
        return false;
    }
    */

    return true;
}

bool ParseMaterialFileFromFile( std::istream& in, std::string& mtlFileName )
{
    fileIO::ParseLineKeyVal( in, "filename", mtlFileName );
    mtlFileName = PG_RESOURCE_DIR + mtlFileName;
    return true;
}

bool ParseModelFromFile( std::istream& in, ModelCreateInfo& createInfo )
{
    fileIO::ParseLineKeyVal( in, "name", createInfo.name );
    fileIO::ParseLineKeyVal( in, "filename", createInfo.filename );
    createInfo.filename = PG_RESOURCE_DIR + createInfo.filename;
    
    return true;
}

AssetStatus FastfileConverter::CheckDependencies()
{
    std::ifstream in( inputFile );
    if ( !in )
    {
        LOG_ERR( "Could not open the input resource file: '", inputFile, "'" );
        return ASSET_CHECKING_ERROR;
    }

    m_status = ASSET_UP_TO_DATE;

    if ( outputFile.empty() )
    {
        outputFile = PG_RESOURCE_DIR "cache/fastfiles/" +
                     std::filesystem::path( inputFile ).filename().string() + ".ff";
    }
    if ( !std::filesystem::exists( outputFile ) )
    {
        m_status = ASSET_OUT_OF_DATE;
    }

    Timestamp outTimestamp( outputFile );
    Timestamp newestFileTime( inputFile );

    m_status = outTimestamp <= newestFileTime ? ASSET_OUT_OF_DATE : ASSET_UP_TO_DATE;

    if ( m_status == ASSET_OUT_OF_DATE )
    {
        LOG( "OUT OF DATE: Fastfile file'", inputFile, "' has newer timestamp than saved FF" );
    }

    auto UpdateStatus = [this]( AssetStatus status ) {
        if ( status == ASSET_OUT_OF_DATE )
        {
            m_status = ASSET_OUT_OF_DATE;
        }
    };

    std::string line;
    while ( std::getline( in, line ) )
    {
        if ( line == "" || line[0] == '#' )
        {
            continue;
        }
        if ( line == "Shader" )
        {
            std::string tmp;
            ShaderConverter converter;
            if ( !ParseShaderCreateInfoFromFile( in, converter.createInfo ) )
            {
                LOG_ERR( "Error while parsing ShaderCreateInfo" );
                return ASSET_CHECKING_ERROR;
            }

            auto status = converter.CheckDependencies();
            if ( status == ASSET_CHECKING_ERROR )
            {
                LOG_ERR( "Error while checking dependencies on resource: '",
                         converter.createInfo.name, "'" );
                return ASSET_CHECKING_ERROR;
            }
            UpdateStatus( status );

            m_shaderConverters.emplace_back( std::move( converter ) );
        }
        else if ( line == "Texture" )
        {
            TextureConverter converter;
            if ( !ParseTextureCreateInfoFromFile( in, converter.createInfo ) )
            {
                LOG_ERR( "Error while parsing ShaderCreateInfo" );
                return ASSET_CHECKING_ERROR;
            }

            auto status = converter.CheckDependencies();
            if ( status == ASSET_CHECKING_ERROR )
            {
                LOG_ERR( "Error while checking dependencies on resource: '",
                         converter.createInfo.name, "'" );
                return ASSET_CHECKING_ERROR;
            }
            UpdateStatus( status );

            m_textureConverters.emplace_back( std::move( converter ) );
        }
        else if ( line == "MTLFile")
        {
            MaterialConverter converter;
            ParseMaterialFileFromFile( in, converter.inputFile );

            auto status = converter.CheckDependencies();
            if ( status == ASSET_CHECKING_ERROR )
            {
                LOG_ERR( "Error while checking dependencies on mtlFile '", converter.inputFile, "'" );
                return ASSET_CHECKING_ERROR;
            }
            UpdateStatus( status );

            m_materialFileConverters.emplace_back( std::move( converter ) );
        }
        else if ( line == "Model")
        {
            ModelConverter converter;
            ParseModelFromFile( in, converter.createInfo );

            auto status = converter.CheckDependencies();
            if ( status == ASSET_CHECKING_ERROR )
            {
                LOG_ERR( "Error while checking dependencies on model file '", converter.createInfo.filename, "'" );
                return ASSET_CHECKING_ERROR;
            }
            UpdateStatus( status );

            m_modelConverters.emplace_back( std::move( converter ) );
        }
        else
        {
            LOG_WARN( "Unrecognized line: ", line );
        }
    }

    in.close();

    return m_status;
}

ConverterStatus FastfileConverter::Convert()
{
    if ( !force && m_status == ASSET_UP_TO_DATE )
    {
        return CONVERT_SUCCESS;
    }

    std::ofstream out( outputFile, std::ios::binary );

    if ( !out )
    {
        LOG_ERR( "Failed to open fastfile '", outputFile, "' for write" );
        return CONVERT_ERROR;
    }

    auto OnError = [&]( std::string ffiFile )
    {
        out.close();
        std::filesystem::remove( outputFile );
        std::filesystem::remove( ffiFile );
        return CONVERT_ERROR;
    };

    uint32_t numShaders = static_cast< uint32_t >( m_shaderConverters.size() );
    serialize::Write( out, numShaders );
    std::vector< char > buffer( 4 * 1024 * 1024 );
    for ( auto& converter : m_shaderConverters )
    {
        if ( converter.Convert() != CONVERT_SUCCESS )
        {
            LOG_ERR( "Error while converting shader" );
            return OnError( converter.outputFile );
        }

        // TODO: Dont write and then read back the same data, just write to both files
        std::ifstream in( converter.outputFile, std::ios::binary );
        if ( !in )
        {
            LOG_ERR( "Error opening intermediate file '", converter.outputFile, "'" );
            return OnError( converter.outputFile );
        }
        in.seekg( 0, in.end );
        size_t length = in.tellg();
        in.seekg( 0, in.beg );
        buffer.resize( length );
        in.read( &buffer[0], length );

        serialize::Write( out, buffer.data(), length );
    }

    uint32_t numTextures = static_cast< uint32_t >( m_textureConverters.size() );
    serialize::Write( out, numTextures );
    for ( auto& converter : m_textureConverters )
    {
        if ( converter.Convert() != CONVERT_SUCCESS )
        {
            LOG_ERR( "Error while converting texture" );
            return OnError( converter.outputFile );
        }

        std::ifstream in( converter.outputFile, std::ios::binary );
        if ( !in )
        {
            LOG_ERR( "Error opening intermediate file '", converter.outputFile, "'" );
            return OnError( converter.outputFile );
        }
        in.seekg( 0, in.end );
        size_t length = in.tellg();
        in.seekg( 0, in.beg );
        buffer.resize( length );
        in.read( &buffer[0], length );

        serialize::Write( out, buffer.data(), length );
    }

    uint32_t numMaterials = static_cast< uint32_t >( m_materialFileConverters.size() );
    serialize::Write( out, numMaterials );
    for ( auto& converter : m_materialFileConverters )
    {
        if ( converter.Convert() != CONVERT_SUCCESS )
        {
            LOG_ERR( "Error while converting material" );
            return OnError( converter.outputFile );
        }

        std::ifstream in( converter.outputFile, std::ios::binary );
        if ( !in )
        {
            LOG_ERR( "Error opening intermediate file '", converter.outputFile, "'" );
            return OnError( converter.outputFile );
        }
        in.seekg( 0, in.end );
        size_t length = in.tellg();
        in.seekg( 0, in.beg );
        buffer.resize( length );
        in.read( &buffer[0], length );

        serialize::Write( out, buffer.data(), length );
    }

    uint32_t numModels = static_cast< uint32_t >( m_modelConverters.size() );
    serialize::Write( out, numModels );
    for ( auto& converter : m_modelConverters )
    {
        if ( converter.Convert() != CONVERT_SUCCESS )
        {
            LOG_ERR( "Error while converting model" );
            return OnError( converter.outputFile );
        }

        std::ifstream in( converter.outputFile, std::ios::binary );
        if ( !in )
        {
            LOG_ERR( "Error opening intermediate file '", converter.outputFile, "'" );
            return OnError( converter.outputFile );
        }
        in.seekg( 0, in.end );
        size_t length = in.tellg();
        in.seekg( 0, in.beg );
        buffer.resize( length );
        in.read( &buffer[0], length );

        serialize::Write( out, buffer.data(), length );
    }

    out.close();

#if USING( LZ4_COMPRESSED_FASTFILES )
    LOG( "Compressing with LZ4..." );
    MemoryMapped memMappedFile;
    if ( !memMappedFile.open( outputFile, MemoryMapped::WholeFile, MemoryMapped::Normal ) )
    {
        LOG_ERR( "Could not open fastfile:", outputFile );
        return CONVERT_ERROR;
    }

    char* src = (char*) memMappedFile.getData();
    const int srcSize = (int) memMappedFile.size();

    const int maxDstSize = LZ4_compressBound( srcSize );

    char* compressedData = (char*) malloc( maxDstSize );
    const int compressedDataSize = LZ4_compress_default( src, compressedData, srcSize, maxDstSize );

    memMappedFile.close();

    if ( compressedDataSize <= 0 )
    {
        LOG_ERR("Error while trying to compress the fastfile. LZ4 returned: ", compressedDataSize );
        return CONVERT_ERROR;
    }

    if ( compressedDataSize > 0 )
    {
        LOG( "Compressed file size ratio: ", (float) compressedDataSize / srcSize );
    }

    out.open( outputFile, std::ios::binary );

    if ( !out )
    {
        LOG_ERR( "Failed to open fastfile '", outputFile, "' for writing compressed results" );
        return CONVERT_ERROR;
    }

    serialize::Write( out, srcSize );
    serialize::Write( out, compressedData, compressedDataSize );

    out.close();
#endif // #if USING( LZ4_COMPRESSED_FASTFILES )

    return CONVERT_SUCCESS;
}
