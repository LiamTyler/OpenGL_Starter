#include "resource/skinned_model.hpp"
#include "core/assert.hpp"
#include "core/time.hpp"
#include "graphics/vulkan.hpp"
#include "utils/logger.hpp"
#include "utils/serialize.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include <set>

static aiMatrix4x4 GLMMat4ToAi( const glm::mat4& mat )
{
    return aiMatrix4x4( mat[0][0], mat[0][1], mat[0][2], mat[0][3],
                        mat[1][0], mat[1][1], mat[1][2], mat[1][3],
                        mat[2][0], mat[2][1], mat[2][2], mat[2][3],
                        mat[3][0], mat[3][1], mat[3][2], mat[3][3] );
}

static glm::mat4 AiToGLMMat4( const aiMatrix4x4& in_mat )
{
    glm::mat4 tmp;
    tmp[0][0] = in_mat.a1;
    tmp[1][0] = in_mat.b1;
    tmp[2][0] = in_mat.c1;
    tmp[3][0] = in_mat.d1;

    tmp[0][1] = in_mat.a2;
    tmp[1][1] = in_mat.b2;
    tmp[2][1] = in_mat.c2;
    tmp[3][1] = in_mat.d2;

    tmp[0][2] = in_mat.a3;
    tmp[1][2] = in_mat.b3;
    tmp[2][2] = in_mat.c3;
    tmp[3][2] = in_mat.d3;

    tmp[0][3] = in_mat.a4;
    tmp[1][3] = in_mat.b4;
    tmp[2][3] = in_mat.c4;
    tmp[3][3] = in_mat.d4;
    return glm::transpose( tmp );
}

static glm::vec3 AiToGLMVec3( const aiVector3D& v )
{
    return { v.x, v.y, v.z };
}

static glm::quat AiToGLMQuat( const aiQuaternion& q )
{
    return { q.w, q.x, q.y, q.z };
}

namespace Progression
{

    aiNodeAnim* FindAINodeAnim( aiAnimation* pAnimation, const std::string& NodeName )
    {
        for ( uint32_t i = 0; i < pAnimation->mNumChannels; ++i )
        {
            aiNodeAnim* pNodeAnim = pAnimation->mChannels[i];
        
            if ( std::string( pNodeAnim->mNodeName.data ) == NodeName )
            {
                return pNodeAnim;
            }
        }
    
        return NULL;
    }

    static void ReadNodeHeirarchy( aiNode* pNode, uint32_t numAnimations, aiAnimation** animations, int depth = 0 )
    {
        std::string NodeName( pNode->mName.data );
        std::string tabbing( depth * 2, ' ' );
            
        glm::mat4 NodeTransformation( AiToGLMMat4( pNode->mTransformation ) );

        bool animFound = false;
        for ( auto i = 0u; i < numAnimations && !animFound; ++i )
        {
            aiNodeAnim* pNodeAnim = FindAINodeAnim( animations[i], NodeName );
            if ( pNodeAnim )
            {
                LOG( tabbing, "Node '", NodeName, "' has first animation component in animation: ", i );
                animFound = true;
            }
        }
        if ( !animFound )
        {
            LOG( tabbing, "Node '", NodeName, "' does not have animation component" );
        }
        LOG( tabbing, "NodeTransformation =" );
        LOG( tabbing, NodeTransformation[0] );
        LOG( tabbing, NodeTransformation[1] );
        LOG( tabbing, NodeTransformation[2] );
        LOG( tabbing, NodeTransformation[3] );
        
    
        for ( uint32_t i = 0; i < pNode->mNumChildren; i++ )
        {
            ReadNodeHeirarchy( pNode->mChildren[i], numAnimations, animations, depth + 1 );
        }
    }

    void SkinnedModel::ApplyPoseToJoints( uint32_t jointIdx, const glm::mat4& parentTransform, std::vector< glm::mat4 >& transformBuffer )
    {
        glm::mat4 currentTransform = parentTransform * transformBuffer[jointIdx];
        for ( const auto& child : skeleton.joints[jointIdx].children )
        {
            ApplyPoseToJoints( child, currentTransform, transformBuffer );
        }
        transformBuffer[jointIdx] = currentTransform * skeleton.joints[jointIdx].inverseBindTransform;
    }

    static void FindJointChildren( aiNode* node, std::unordered_map< std::string, uint32_t >& jointMapping, std::vector< Joint >& joints )
    {
        std::string name( node->mName.data );
        if ( jointMapping.find( name ) != jointMapping.end() )
        {
            for ( uint32_t i = 0; i < node->mNumChildren; i++ )
            {
                std::string childName( node->mChildren[i]->mName.data );
                // PG_ASSERT( jointMapping.find( childName ) != jointMapping.end(), "AI bone has a non-bone node as a child" );
                if  ( jointMapping.find( childName ) != jointMapping.end() )
                {
                    joints[jointMapping[name]].children.push_back( jointMapping[childName] );
                }
            }
        }
        for ( uint32_t i = 0; i < node->mNumChildren; i++ )
        {
            FindJointChildren( node->mChildren[i], jointMapping, joints );
        }
    }

    static void BuildAINodeMap( aiNode* pNode, std::unordered_map< std::string, aiNode* >& nodes )
    {
        std::string name( pNode->mName.data );
        PG_ASSERT( nodes.find( name ) == nodes.end(), "Map of AI nodes already contains name '" + name + "'" );
        nodes[name] = pNode;
    
        for ( uint32_t i = 0; i < pNode->mNumChildren; i++ )
        {
            BuildAINodeMap( pNode->mChildren[i], nodes );
        }
    }

    static void BuildAIAnimNodeMap( aiNode* pNode, aiAnimation* pAnimation, std::unordered_map< std::string, aiNodeAnim* >& animNodes )
    {
        std::string nodeName( pNode->mName.data );
        aiNodeAnim* pNodeAnim = FindAINodeAnim( pAnimation, nodeName );
    
        if ( pNodeAnim )
        {
            PG_ASSERT( animNodes.find( nodeName ) == animNodes.end(), "Map of AI animation nodes already contains name '" + nodeName + "'" );
            animNodes[nodeName] = pNodeAnim;
        }
    
        for ( uint32_t i = 0; i < pNode->mNumChildren; i++ )
        {
            BuildAIAnimNodeMap( pNode->mChildren[i], pAnimation, animNodes );
        }
    }

    static std::set< float > GetAllAnimationTimes( aiAnimation* pAnimation )
    {
        std::set< float > times;
        for ( uint32_t i = 0; i < pAnimation->mNumChannels; ++i )
        {
            aiNodeAnim* p = pAnimation->mChannels[i];
        
            for ( uint32_t i = 0; i < p->mNumPositionKeys; ++i )
            {
                times.insert( static_cast< float >( p->mPositionKeys[i].mTime ) );
            }
            for ( uint32_t i = 0; i < p->mNumRotationKeys; ++i )
            {
                times.insert( static_cast< float >( p->mRotationKeys[i].mTime ) );
            }
            for ( uint32_t i = 0; i < p->mNumPositionKeys; ++i )
            {
                times.insert( static_cast< float >( p->mScalingKeys[i].mTime ) );
            }
        }

        return times;
    }

    static void AnalyzeMemoryEfficiency( aiAnimation* pAnimation, const std::set< float >& times )
    {
        double efficient = 0;
        for ( uint32_t i = 0; i < pAnimation->mNumChannels; ++i )
        {
            aiNodeAnim* p = pAnimation->mChannels[i];
            efficient += p->mNumPositionKeys * sizeof( glm::vec3 ) + p->mNumRotationKeys * sizeof( glm::quat ) + p->mNumScalingKeys * sizeof( glm::vec3 );
        }
        double easy = static_cast< double >( pAnimation->mNumChannels * times.size() * ( sizeof( glm::vec3 ) + sizeof( glm::quat ) + sizeof( glm::vec3 ) ) );

        efficient /= ( 2 << 20 );
        easy      /= ( 2 << 20 );
        LOG( "Efficient method = ", efficient, "MB, easy method = ", easy, "MB, ratio = ", easy / efficient );
    }

    static void ParseMaterials( const std::string& filename, SkinnedModel* model, const aiScene* scene )
    {
        std::string::size_type slashIndex = filename.find_last_of( "/" );
        std::string dir;

        if ( slashIndex == std::string::npos)
        {
            dir = ".";
        }
        else if ( slashIndex == 0 )
        {
            dir = "/";
        }
        else
        {
            dir = filename.substr( 0, slashIndex );
        }

        model->materials.resize( scene->mNumMaterials );
        for ( uint32_t mtlIdx = 0; mtlIdx < scene->mNumMaterials; ++mtlIdx )
        {
            const aiMaterial* pMaterial = scene->mMaterials[mtlIdx];
            model->materials[mtlIdx] = std::make_shared< Material >();
            aiString name;
            aiColor3D color;
            pMaterial->Get( AI_MATKEY_NAME, name );
            model->materials[mtlIdx]->name = name.C_Str();

            color = aiColor3D( 0.f, 0.f, 0.f );
            pMaterial->Get( AI_MATKEY_COLOR_AMBIENT, color );
            model->materials[mtlIdx]->Ka = { color.r, color.g, color.b };
            color = aiColor3D( 0.f, 0.f, 0.f );
            pMaterial->Get( AI_MATKEY_COLOR_DIFFUSE, color );
            model->materials[mtlIdx]->Kd = { color.r, color.g, color.b };
            color = aiColor3D( 0.f, 0.f, 0.f );
            pMaterial->Get( AI_MATKEY_COLOR_SPECULAR, color );
            model->materials[mtlIdx]->Ks = { color.r, color.g, color.b };
            color = aiColor3D( 0.f, 0.f, 0.f );
            pMaterial->Get( AI_MATKEY_COLOR_EMISSIVE, color );
            model->materials[mtlIdx]->Ke = { color.r, color.g, color.b };
            color = aiColor3D( 0.f, 0.f, 0.f );
            float Ns;
            pMaterial->Get( AI_MATKEY_SHININESS, Ns );
            model->materials[mtlIdx]->Ns = Ns;
            LOG( "Material[", mtlIdx, "].Ka = ", model->materials[mtlIdx]->Ka );
            LOG( "Material[", mtlIdx, "].Kd = ", model->materials[mtlIdx]->Kd );
            LOG( "Material[", mtlIdx, "].Ks = ", model->materials[mtlIdx]->Ks );
            LOG( "Material[", mtlIdx, "].Ke = ", model->materials[mtlIdx]->Ke );
            LOG( "Material[", mtlIdx, "].Ns = ", model->materials[mtlIdx]->Ns );

            if ( pMaterial->GetTextureCount( aiTextureType_DIFFUSE ) > 0 )
            {
                PG_ASSERT( pMaterial->GetTextureCount( aiTextureType_DIFFUSE ) == 1, "Can't have more than 1 diffuse texture per material" );
                aiString path;

                if ( pMaterial->GetTexture( aiTextureType_DIFFUSE, 0, &path, NULL, NULL, NULL, NULL, NULL ) == AI_SUCCESS )
                {
                    std::string p( path.data );
                
                    if ( p.substr(0, 2) == ".\\" )
                    {                    
                        p = p.substr(2, p.size() - 2);
                    }
                               
                    std::string fullPath = dir + "/" + p;
                    LOG( "Material[", mtlIdx, "] has diffuse texture '", fullPath, "'" );
                }
            }
        }
    }

    bool SkinnedModel::Load( ResourceCreateInfo* baseInfo )
    {
        PG_ASSERT( baseInfo );
        SkinnedModelCreateInfo* createInfo = static_cast< SkinnedModelCreateInfo* >( baseInfo );
        name = createInfo->name;
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile( createInfo->filename.c_str(), aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices );
        if ( !scene )
        {
            LOG_ERR( "Error parsing FBX file '", createInfo->filename.c_str(), "': ", importer.GetErrorString() );
            return false;
        }

        meshes.resize( scene->mNumMeshes );       
        uint32_t numVertices = 0;
        uint32_t numIndices = 0;    
        for ( size_t i = 0 ; i < meshes.size(); i++ )
        {
            meshes[i].materialIndex = scene->mMeshes[i]->mMaterialIndex;
            meshes[i].numIndices  = scene->mMeshes[i]->mNumFaces * 3;
            meshes[i].startVertex = numVertices;
            meshes[i].startIndex  = numIndices;
        
            numVertices += scene->mMeshes[i]->mNumVertices;
            numIndices  += meshes[i].numIndices;
        }
        vertices.reserve( numVertices );
        normals.reserve( numVertices );
        uvs.reserve( numVertices );
        indices.reserve( numIndices );
        blendWeights.resize( numVertices );

        std::unordered_map< std::string, uint32_t > jointNameToIndexMap;
        
        std::unordered_map< std::string, aiNode* > aiNodeMap;
        BuildAINodeMap( scene->mRootNode, aiNodeMap );
        // Assimp doesn't seem to provide the root bone in the actual bone list, but animations are provided the scene
        // node that would be the root bone. Store a placeholder for it now, so that spot [0] is reserved, and find
        // the actual root bone after parsing the other bones.
        Joint rootJointPlaceholder;
        rootJointPlaceholder.name = "___ROOT_BONE_PLACEHOLDER___";
        skeleton.joints.push_back( rootJointPlaceholder );
        for ( size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx )
        {
            const aiMesh* paiMesh = scene->mMeshes[meshIdx];
            const aiVector3D Zero3D( 0.0f, 0.0f, 0.0f );

            for ( uint32_t vIdx = 0; vIdx < paiMesh->mNumVertices ; ++vIdx )
            {
                const aiVector3D* pPos      = &( paiMesh->mVertices[vIdx] );
                const aiVector3D* pNormal   = &( paiMesh->mNormals[vIdx] );
                const aiVector3D* pTexCoord = paiMesh->HasTextureCoords( 0 ) ? &( paiMesh->mTextureCoords[0][vIdx] ) : &Zero3D;

                vertices.emplace_back( pPos->x, pPos->y, pPos->z );
                normals.emplace_back( pNormal->x, pNormal->y, pNormal->z );
                uvs.emplace_back( pTexCoord->x, pTexCoord->y );
            }

            for ( uint32_t boneIdx = 0; boneIdx < paiMesh->mNumBones; ++boneIdx )
            {
                uint32_t jointIndex = 0;
                std::string jointName( paiMesh->mBones[boneIdx]->mName.data );

                if ( jointNameToIndexMap.find( jointName ) == jointNameToIndexMap.end() )
                {
                    jointIndex = static_cast< uint32_t >( skeleton.joints.size() );
                    Joint newJoint;
                    newJoint.name                 = jointName;
                    newJoint.inverseBindTransform = AiToGLMMat4( paiMesh->mBones[boneIdx]->mOffsetMatrix );
                    skeleton.joints.push_back( newJoint );
                    jointNameToIndexMap[jointName] = jointIndex;
                }
                else
                {
                    jointIndex = jointNameToIndexMap[jointName];
                }
        
                for ( unsigned int weightIdx = 0; weightIdx < paiMesh->mBones[boneIdx]->mNumWeights; ++weightIdx )
                {
                    uint32_t vertexID = meshes[meshIdx].startVertex + paiMesh->mBones[boneIdx]->mWeights[weightIdx].mVertexId;
                    float weight      = paiMesh->mBones[boneIdx]->mWeights[weightIdx].mWeight;
                    blendWeights[vertexID].AddJointData( jointIndex, weight );
                }
            }

            for ( size_t iIdx = 0 ; iIdx < paiMesh->mNumFaces ; ++iIdx )
            {
                const aiFace& face = paiMesh->mFaces[iIdx];
                PG_ASSERT( face.mNumIndices == 3 );
                indices.push_back( face.mIndices[0] );
                indices.push_back( face.mIndices[1] );
                indices.push_back( face.mIndices[2] );
            }
        }

        // Find actual root bone and children if there is a skeleton
        if ( skeleton.joints.size() == 1 )
        {
            skeleton.joints.clear();
        }
        else
        {
            for ( uint32_t joint = 1; joint < (uint32_t) skeleton.joints.size(); ++joint )
            {
                aiNode* parent = aiNodeMap[skeleton.joints[joint].name]->mParent;
                PG_ASSERT( parent );
                std::string parentName( parent->mName.data );
                if ( jointNameToIndexMap.find( parentName ) == jointNameToIndexMap.end() )
                {
                    Joint& rootBone = skeleton.joints[0];
                    rootBone.name = parentName;
                    rootBone.inverseBindTransform = glm::mat4( 1 );
                    while ( parent != NULL )
                    {
                        rootBone.inverseBindTransform = AiToGLMMat4( parent->mTransformation ) * rootBone.inverseBindTransform;
                        parent = parent->mParent;
                    }
                    for ( uint32_t i = 1; i < (uint32_t) skeleton.joints.size(); ++i )
                    {
                        std::string boneParent = aiNodeMap[skeleton.joints[i].name]->mParent->mName.data;
                        if ( rootBone.name == boneParent )
                        {
                            rootBone.children.push_back( i );
                        }
                    }
                    rootBone.inverseBindTransform = glm::inverse( rootBone.inverseBindTransform );
                    break;
                }
            }
            FindJointChildren( scene->mRootNode, jointNameToIndexMap, skeleton.joints );
            jointNameToIndexMap[skeleton.joints[0].name] = 0;
        }

        //ReadNodeHeirarchy( scene->mRootNode, scene->mNumAnimations, scene->mAnimations );
        //LOG( "" );

        animations.resize( scene->mNumAnimations );
        for ( uint32_t animIdx = 0; animIdx < scene->mNumAnimations; ++animIdx )
        {
            Animation& pgAnim         = animations[animIdx];
            aiAnimation* aiAnim       = scene->mAnimations[animIdx];
            pgAnim.name               = aiAnim->mName.data;
            pgAnim.duration           = static_cast< float >( aiAnim->mDuration );
            PG_ASSERT( pgAnim.duration > 0 );
            pgAnim.ticksPerSecond     = static_cast< float >( aiAnim->mTicksPerSecond );
            if ( aiAnim->mTicksPerSecond == 0 )
            {
                LOG_WARN( "Animation '", aiAnim->mName.C_Str(), "' does not specify TicksPerSecond. Using default of 30" );
                pgAnim.ticksPerSecond = 30;
            }

            std::set< float > keyFrameTimes = GetAllAnimationTimes( aiAnim );
            AnalyzeMemoryEfficiency( aiAnim, keyFrameTimes );
            std::unordered_map< std::string, aiNodeAnim* > aiAnimNodeMap;
            BuildAIAnimNodeMap( scene->mRootNode, aiAnim, aiAnimNodeMap );
            PG_ASSERT( aiAnimNodeMap.size() == skeleton.joints.size(), "Animation does not have the same skeleton as model" );

            pgAnim.keyFrames.resize( keyFrameTimes.size() );
            int frameIdx = 0;
            for ( const auto& time : keyFrameTimes )
            {
                pgAnim.keyFrames[frameIdx].time = time;
                pgAnim.keyFrames[frameIdx].jointSpaceTransforms.resize( skeleton.joints.size() );
                for ( uint32_t joint = 0; joint < (uint32_t) skeleton.joints.size(); ++joint )
                {
                    aiNodeAnim* animNode       = aiAnimNodeMap[skeleton.joints[joint].name];
                    JointTransform& jTransform = pgAnim.keyFrames[frameIdx].jointSpaceTransforms[joint];
                    uint32_t i;
                    float dt;

                    for ( i = 0; i < animNode->mNumPositionKeys - 1 && time > (float) animNode->mPositionKeys[i + 1].mTime; ++i );
                    if ( i + 1 == animNode->mNumPositionKeys )
                    {
                        jTransform.position = AiToGLMVec3( animNode->mPositionKeys[i].mValue );
                    }
                    else
                    {
                        glm::vec3 currentPos = AiToGLMVec3( animNode->mPositionKeys[i].mValue );
                        glm::vec3 nextPos    = AiToGLMVec3( animNode->mPositionKeys[i + 1].mValue );
                        dt                   = ( time - (float) animNode->mPositionKeys[i].mTime ) / ( (float) animNode->mPositionKeys[i + 1].mTime - (float) animNode->mPositionKeys[i].mTime );
                        jTransform.position  = currentPos + dt * ( nextPos - currentPos );
                    }

                    for ( i = 0; i < animNode->mNumRotationKeys - 1 && time > (float) animNode->mRotationKeys[i + 1].mTime; ++i );
                    if ( i + 1 == animNode->mNumRotationKeys )
                    {
                        jTransform.rotation = AiToGLMQuat( animNode->mRotationKeys[i].mValue );
                    }
                    else
                    {
                        glm::quat currentRot = glm::normalize( AiToGLMQuat( animNode->mRotationKeys[i].mValue ) );
                        glm::quat nextRot    = glm::normalize( AiToGLMQuat( animNode->mRotationKeys[i + 1].mValue ) );
                        dt                   = ( time - (float) animNode->mRotationKeys[i].mTime ) / ( (float) animNode->mRotationKeys[i + 1].mTime - (float) animNode->mRotationKeys[i].mTime );
                        jTransform.rotation  = glm::normalize( glm::slerp( currentRot, nextRot, dt ) );
                    }
                    

                    for ( i = 0; i < animNode->mNumScalingKeys - 1 && time > (float) animNode->mScalingKeys[i + 1].mTime; ++i );
                    if ( i + 1 == animNode->mNumScalingKeys )
                    {
                        jTransform.scale = AiToGLMVec3( animNode->mScalingKeys[i].mValue );
                    }
                    else
                    {
                        glm::vec3 currentScale = AiToGLMVec3( animNode->mScalingKeys[i].mValue );
                        glm::vec3 nextScale    = AiToGLMVec3( animNode->mScalingKeys[i + 1].mValue );
                        dt                     = ( time - (float) animNode->mScalingKeys[i].mTime ) / ( (float) animNode->mScalingKeys[i + 1].mTime - (float) animNode->mScalingKeys[i].mTime );
                        jTransform.scale       = currentScale + dt * ( nextScale - nextScale );
                    }
                    
                }
                ++frameIdx;
            }
        }

        ParseMaterials( createInfo->filename, this, scene );

        // renormalize the blend weights to 1
        for ( size_t i = 0; i < blendWeights.size(); ++i )
        {
            auto& data = blendWeights[i];
            float sum = data.weights[0] + data.weights[1] + data.weights[2] + data.weights[3];
            PG_ASSERT( sum > 0 );
            data.weights /= sum;
        }
        RecalculateAABB();

        if ( createInfo->optimize )
        {
            Optimize();
        }
        if ( createInfo->createGpuCopy )
        {
            UploadToGpu();
        }
        if ( createInfo->freeCpuCopy )
        {
            FreeGeometry();
        }

        return true;
    }

    void SkinnedModel::Move( std::shared_ptr< Resource > dst )
    {
        PG_ASSERT( std::dynamic_pointer_cast< SkinnedModel >( dst ) );
        SkinnedModel* dstPtr = (SkinnedModel*) dst.get();
        *dstPtr              = std::move( *this );
    }

    bool SkinnedModel::Serialize( std::ofstream& out ) const
    {
        uint32_t numMaterials = static_cast< uint32_t >( materials.size() );
        serialize::Write( out, numMaterials );
        for ( uint32_t i = 0; i < numMaterials; ++i )
        {
            if ( !materials[i]->Serialize( out ) )
            {
                LOG( "Could not write material: ", i, ", of model: ", name, " to fastfile" );
                return false;
            }
        }
        uint32_t numVertices     = static_cast< uint32_t >( vertices.size() );
        uint32_t numUVs          = static_cast< uint32_t >( uvs.size() );
        uint32_t numBlendWeights = static_cast< uint32_t >( blendWeights.size() );
        uint32_t numIndices      = static_cast< uint32_t >( indices.size() );
        serialize::Write( out, numVertices );
        serialize::Write( out, numUVs );
        serialize::Write( out, numBlendWeights );
        serialize::Write( out, numIndices );
        serialize::Write( out, (char*) vertices.data(),     numVertices * sizeof( glm::vec3 ) );
        serialize::Write( out, (char*) normals.data(),      numVertices * sizeof( glm::vec3 ) );
        serialize::Write( out, (char*) uvs.data(),          numUVs * sizeof( glm::vec2 ) );
        serialize::Write( out, (char*) blendWeights.data(), numBlendWeights * 2 * sizeof( glm::vec4 ) );
        serialize::Write( out, (char*) indices.data(),      numIndices * sizeof( uint32_t ) );

        serialize::Write( out, aabb.min );
        serialize::Write( out, aabb.max );
        serialize::Write( out, aabb.extent );
        serialize::Write( out, meshes );
        skeleton.Serialize( out );

        return !out.fail();
    }

    bool SkinnedModel::Deserialize( char*& buffer )
    {
        serialize::Read( buffer, name );
        bool freeCpuCopy;
        bool createGpuCopy;
        serialize::Read( buffer, freeCpuCopy );
        serialize::Read( buffer, createGpuCopy );

        uint32_t numMaterials;
        serialize::Read( buffer, numMaterials );
        for ( uint32_t i = 0; i < numMaterials; ++i )
        {
            materials[i]->Deserialize( buffer );
        }

        uint32_t numVertices, numUVs, numBlendWeights, numIndices;
        serialize::Read( buffer, numVertices );
        serialize::Read( buffer, numUVs );
        serialize::Read( buffer, numBlendWeights );
        serialize::Read( buffer, numIndices );
        if ( freeCpuCopy )
        {
            using namespace Progression::Gfx;
            if ( m_gpuDataCreated )
            {
                vertexBuffer.Free();
                indexBuffer.Free();
            }
            m_gpuDataCreated = true;
            size_t totalVertexSize = 2 * numVertices * sizeof( glm::vec3 ) + numUVs * sizeof( glm::vec2 ) + numBlendWeights * 2 * sizeof( glm::vec4 );
            vertexBuffer = Gfx::g_renderState.device.NewBuffer( totalVertexSize, buffer, BUFFER_TYPE_VERTEX, MEMORY_TYPE_DEVICE_LOCAL );
            buffer += totalVertexSize;
            indexBuffer  = Gfx::g_renderState.device.NewBuffer( numIndices * sizeof( uint32_t ), buffer, BUFFER_TYPE_INDEX, MEMORY_TYPE_DEVICE_LOCAL );
            buffer += numIndices * sizeof( uint32_t );

            m_numVertices       = numVertices;
            m_normalOffset      = m_numVertices * sizeof( glm::vec3 );
            m_uvOffset          = m_normalOffset + numVertices * sizeof( glm::vec3 );
            m_blendWeightOffset = m_uvOffset + numUVs * sizeof( glm::vec2 );
        }
        else
        {
            vertices.resize( numVertices );
            normals.resize( numVertices );
            uvs.resize( numUVs );
            blendWeights.resize( numBlendWeights );
            indices.resize( numIndices );

            serialize::Read( buffer, vertices );
            serialize::Read( buffer, normals );
            serialize::Read( buffer, uvs );
            serialize::Read( buffer, blendWeights );
            serialize::Read( buffer, indices );

            if ( createGpuCopy )
            {
                UploadToGpu();
            }
        }

        serialize::Read( buffer, aabb.min );
        serialize::Read( buffer, aabb.max );
        serialize::Read( buffer, aabb.extent );
        serialize::Read( buffer, meshes );
        skeleton.Deserialize( buffer );

        return true;
    }

    void SkinnedModel::RecalculateNormals()
    {
        normals.resize( vertices.size(), glm::vec3( 0 ) );

        for ( size_t i = 0; i < indices.size(); i += 3 )
        {
            glm::vec3 v1 = vertices[indices[i + 0]];
            glm::vec3 v2 = vertices[indices[i + 1]];
            glm::vec3 v3 = vertices[indices[i + 2]];
            glm::vec3 n = glm::cross( v2 - v1, v3 - v1 );
            normals[indices[i + 0]] += n;
            normals[indices[i + 1]] += n;
            normals[indices[i + 2]] += n;
        }

        for ( auto& normal : normals )
        {
            normal = glm::normalize( normal );
        }
    }

    void SkinnedModel::RecalculateAABB()
    {
        if ( vertices.empty() )
        {
            aabb.min = aabb.max = glm::vec3( 0 );
            return;
        }

        aabb.min = vertices[0];
        aabb.max = vertices[0];
        for ( const auto& vertex : vertices )
        {
            aabb.min = glm::min( aabb.min, vertex );
            aabb.max = glm::max( aabb.max, vertex );
        }
    }

    void SkinnedModel::UploadToGpu()
    {
        using namespace Gfx;

        if ( m_gpuDataCreated )
        {
            vertexBuffer.Free();
            indexBuffer.Free();
        }
        m_gpuDataCreated = true;
        std::vector< float > vertexData( 3 * vertices.size() + 3 * normals.size() + 2 * uvs.size() + 8 * blendWeights.size() );
        char* dst = (char*) vertexData.data();
        memcpy( dst, vertices.data(), vertices.size() * sizeof( glm::vec3 ) );
        dst += vertices.size() * sizeof( glm::vec3 );
        memcpy( dst, normals.data(), normals.size() * sizeof( glm::vec3 ) );
        dst += normals.size() * sizeof( glm::vec3 );
        memcpy( dst, uvs.data(), uvs.size() * sizeof( glm::vec2 ) );
        dst += uvs.size() * sizeof( glm::vec2 );
        memcpy( dst, blendWeights.data(), blendWeights.size() * 2 * sizeof( glm::vec4 ) );
        vertexBuffer = Gfx::g_renderState.device.NewBuffer( vertexData.size() * sizeof( float ), vertexData.data(), BUFFER_TYPE_VERTEX, MEMORY_TYPE_DEVICE_LOCAL );
        indexBuffer  = Gfx::g_renderState.device.NewBuffer( indices.size() * sizeof ( uint32_t ), indices.data(), BUFFER_TYPE_INDEX, MEMORY_TYPE_DEVICE_LOCAL );

        m_numVertices           = static_cast< uint32_t >( vertices.size() );
        m_normalOffset          = m_numVertices * sizeof( glm::vec3 );
        m_uvOffset              = m_normalOffset + m_numVertices * sizeof( glm::vec3 );
        m_blendWeightOffset     = static_cast< uint32_t >( m_uvOffset + uvs.size() * sizeof( glm::vec2 ) );
    }

    void SkinnedModel::FreeGeometry( bool cpuCopy, bool gpuCopy )
    {
        if ( cpuCopy )
        {
            m_numVertices = static_cast< uint32_t >( vertices.size() );
            vertices      = std::vector< glm::vec3 >();
            normals       = std::vector< glm::vec3 >();
            uvs           = std::vector< glm::vec2 >();
            indices       = std::vector< uint32_t >();
            blendWeights  = std::vector< BlendWeight >();
        }

        if ( gpuCopy )
        {
            vertexBuffer.Free();
            indexBuffer.Free();
            m_numVertices = 0;
            m_normalOffset = m_uvOffset = m_blendWeightOffset = ~0u;
        }
    }

    void SkinnedModel::Optimize()
    {
    }

    uint32_t SkinnedModel::GetNumVertices() const
    {
        return m_numVertices;
    }

    uint32_t SkinnedModel::GetVertexOffset() const
    {
        return 0;
    }

    uint32_t SkinnedModel::GetNormalOffset() const
    {
        return m_normalOffset;
    }

    uint32_t SkinnedModel::GetUVOffset() const
    {
        return m_uvOffset;
    }

    uint32_t SkinnedModel::GetBlendWeightOffset() const
    {
        return m_blendWeightOffset;
    }

    Gfx::IndexType SkinnedModel::GetIndexType() const
    {
        return Gfx::IndexType::UNSIGNED_INT;
    }
 
    glm::mat4 JointTransform::GetLocalTransformMatrix() const
    {
        glm::mat4 T = glm::translate( glm::mat4( 1 ), position );
        glm::mat4 R = glm::toMat4( rotation );
        glm::mat4 S = glm::scale( glm::mat4( 1 ), scale );
        return T * R * S;
    }

    JointTransform JointTransform::Interpolate( const JointTransform& end, float t )
    {
        JointTransform ret;
        ret.position = ( 1.0f - t ) * position + t * end.position;
        ret.rotation = glm::normalize( glm::slerp( rotation, end.rotation, t ) );
        ret.scale    = ( 1.0f - t ) * scale + t * end.scale;

        return ret;
    }
        
    void Skeleton::Serialize( std::ofstream& outFile ) const
    {
        serialize::Write( outFile, joints.size() );
        for ( const auto& joint : joints )
        {
            serialize::Write( outFile, joint.name );
            serialize::Write( outFile, joint.inverseBindTransform );
            serialize::Write( outFile, joint.children );
        }
    }

    void Skeleton::Deserialize( char*& buffer )
    {
        size_t numJoints;
        serialize::Read( buffer, numJoints );
        joints.resize( numJoints );
        for ( size_t i = 0; i < numJoints; ++i )
        {
            serialize::Read( buffer, joints[i].name );
            serialize::Read( buffer, joints[i].inverseBindTransform );
            serialize::Read( buffer, joints[i].children );
        }
    }

} // namespace Progression
