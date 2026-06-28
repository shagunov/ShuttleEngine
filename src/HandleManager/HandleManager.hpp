#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <limits>

namespace shuttle_engine::Core {

    struct MeshID { uint32_t id = std::numeric_limits<std::uint32_t>::max(); };
    struct MaterialID { uint32_t id = std::numeric_limits<uint32_t>::max(); };
    struct TextureID { uint32_t id = std::numeric_limits<uint32_t>::max(); };

    class HandleManager {
    public:
        // --- Mesh ---
        MeshID registerMesh(const std::string& name) {
            uint32_t id = nextMeshId++;
            meshNames[name] = { id };
            idToMeshName[id] = name;
            return { id };
        }
        MeshID getMeshId(const std::string& name) const {
            auto it = meshNames.find(name);
            return (it != meshNames.end()) ? it->second : MeshID{};
        }

        // --- Material ---
        MaterialID registerMaterial(const std::string& name) {
            uint32_t id = nextMaterialId++;
            materialNames[name] = { id };
            idToMaterialName[id] = name;
            return { id };
        }
        MaterialID getMaterialId(const std::string& name) const {
            auto it = materialNames.find(name);
            return (it != materialNames.end()) ? it->second : MaterialID{};
        }

        // --- Texture ---
        TextureID registerTexture(const std::string& name) {
            uint32_t id = nextTextureId++;
            textureNames[name] = { id };
            idToTextureName[id] = name;
            return { id };
        }
        TextureID getTextureId(const std::string& name) const {
            auto it = textureNames.find(name);
            return (it != textureNames.end()) ? it->second : TextureID{};
        }

        // --- Debug Helpers ---
        std::string getMeshName(MeshID id) const { return idToMeshName.at(id.id); }
        std::string getMaterialName(MaterialID id) const { return idToMaterialName.at(id.id); }
        std::string getTextureName(TextureID id) const { return idToTextureName.at(id.id); }

    private:
        uint32_t nextMeshId = 0;
        uint32_t nextMaterialId = 0;
        uint32_t nextTextureId = 0;

        std::unordered_map<std::string, MeshID> meshNames;
        std::unordered_map<uint32_t, std::string> idToMeshName;

        std::unordered_map<std::string, MaterialID> materialNames;
        std::unordered_map<uint32_t, std::string> idToMaterialName;

        std::unordered_map<std::string, TextureID> textureNames;
        std::unordered_map<uint32_t, std::string> idToTextureName;
    };
}
