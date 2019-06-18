#pragma once

#include "core/common.hpp"
#include "utils/noncopyable.hpp"
#include <vector>

namespace Progression {

    class Mesh : public NonCopyable {
    public:
        Mesh();
        ~Mesh();

        Mesh(Mesh&& mesh);
        Mesh& operator=(Mesh&& mesh);

		void uploadToGpu(bool freeCPUCopy = true);
		void free(bool gpuCopy = true, bool cpuCopy = true);

        unsigned int getNumVertices() const { return numVertices_; }
        unsigned int getNumIndices() const { return numIndices_; }


		std::vector<glm::vec3> vertices;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;
		std::vector<unsigned int> indices;
        GLuint vertexBuffer;
        GLuint indexBuffer;
        GLuint vao;

	private:
        unsigned int numVertices_   = 0;
        unsigned int numIndices_    = 0;
        unsigned int normalOffset_  = (unsigned int) -1;
        unsigned int textureOffset_ = (unsigned int) -1;
    };

} // namespace Progression