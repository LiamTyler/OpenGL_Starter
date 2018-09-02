#include "graphics/mesh.h"
#include "tinyobjloader/tiny_obj_loader.h"

namespace Progression {

    Mesh::Mesh() :
        numVertices_(0),
        numTriangles_(0),
        vertices_(nullptr),
        normals_(nullptr),
        uvs_(nullptr),
        indices_(nullptr)
    {
        glGenBuffers(vboName::TOTAL_VBOS, vbos_);
    }

    Mesh::Mesh(int numVerts, int numTris, glm::vec3* verts, glm::vec3* norms, glm::vec2* uvs, unsigned int* indices) :
        numVertices_(numVerts),
        numTriangles_(numTris),
        vertices_(verts),
        normals_(norms),
        uvs_(uvs),
        indices_(indices)
    {
        glGenBuffers(vboName::TOTAL_VBOS, vbos_);
    }

    Mesh::~Mesh() {
        glDeleteBuffers(vboName::TOTAL_VBOS, vbos_);
        if (vertices_)
            delete[] vertices_;
        if (normals_)
            delete[] normals_;
        if (uvs_)
            delete[] uvs_;
        if (indices_)
            delete[] indices_;
    }

    void Mesh::UploadToGPU(bool freeOnUpload) {
        glBindBuffer(GL_ARRAY_BUFFER, vbos_[vboName::VERTEX]);
        glBufferData(GL_ARRAY_BUFFER, numVertices_ * sizeof(glm::vec3), vertices_, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, vbos_[vboName::NORMAL]);
        glBufferData(GL_ARRAY_BUFFER, numVertices_ * sizeof(glm::vec3), normals_, GL_STATIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, vbos_[vboName::UV]);
        glBufferData(GL_ARRAY_BUFFER, numVertices_ * sizeof(glm::vec2), uvs_, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos_[vboName::INDEX]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * numTriangles_ * sizeof(unsigned int), indices_, GL_STATIC_DRAW);

        if (freeOnUpload) {
            delete[] vertices_;
            vertices_ = nullptr;
            delete[] normals_;
            normals_ = nullptr;
            delete[] uvs_;
            uvs_ = nullptr;
            delete[] indices_;
            indices_ = nullptr;
        }
    }

} // namespace Progression