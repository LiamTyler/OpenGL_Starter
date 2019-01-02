#pragma once

#include "core/config.hpp"
#include "core/game_object.hpp"
#include "graphics/model.hpp"
#include "graphics/material.hpp"
#include "graphics/shader.hpp"
#include "graphics/skybox.hpp"
#include "graphics/texture2D.hpp"
#include <unordered_set>

namespace Progression {

    class ResourceManager {
    public:
        ResourceManager() = delete;
        ~ResourceManager() = delete;

        static void Init(const config::Config& config);
        static void Free();

        static void LoadResourceFile(const std::string& relativePath);
        
        static std::shared_ptr<Model> GetModel(const std::string& name, bool shallowCopy = false) {
            if (models_.find(name) == models_.end()) {
                return nullptr;
            }
			return models_[name];
        }

        static std::shared_ptr<Material> GetMaterial(const std::string& name, bool shallowCopy = false) {
            if (materials_.find(name) == materials_.end()) {
                return nullptr;
            }
			return materials_[name];
        }

        static std::shared_ptr<Shader> GetShader(const std::string& name) {
            if (shaders_.find(name) != shaders_.end())
                return shaders_[name];
            else
                return nullptr;
        }

        static std::shared_ptr<Texture2D> GetTexture2D(const std::string& name) {
            if (textures2D_.find(name) != textures2D_.end())
                return textures2D_[name];
            else
                return nullptr;
        }

        static std::shared_ptr<Skybox> GetSkybox(const std::string& name) {
            if (skyboxes_.find(name) != skyboxes_.end())
                return skyboxes_[name];
            else
                return nullptr;
        }

        static std::shared_ptr<Model> LoadModel(const std::string& relativePath, bool addToManager=true);
        static std::shared_ptr<Texture2D> LoadTexture2D(const std::string& relativePath, bool addToManager=true);
        static std::shared_ptr<Skybox> LoadSkybox(const std::string& name, const std::vector<std::string>& textures, bool addToManager=true);

        static std::shared_ptr<Shader> AddShader(Shader&& shader, const std::string& name);
        static std::shared_ptr<Material> AddMaterial(Material& material, const std::string& name);

        static std::shared_ptr<Model> LoadOBJ(const std::string& fullPath);
        static std::shared_ptr<Model> LoadPGModel(const std::string& fullPath);
        static bool ConvertOBJToPGModel(const std::string& fullPathToOBJ, const std::string& fullPathToMaterialDir, const std::string& fullOutputPath);

    private:
        static std::unordered_map<std::string, std::shared_ptr<Model>> models_;
        static std::unordered_map<std::string, std::shared_ptr<Material>> materials_;
        static std::unordered_map<std::string, std::shared_ptr<Texture2D>> textures2D_;
        static std::unordered_map<std::string, std::shared_ptr<Shader>> shaders_;
        static std::unordered_map<std::string, std::shared_ptr<Skybox>> skyboxes_;
    };

} // namespace Progression
