//
// Created by Shagu on 25.05.2026.
//

#ifndef HELLOTRIANGLE_RAWRENDERDATA_HPP
#define HELLOTRIANGLE_RAWRENDERDATA_HPP
#include <glm/glm.hpp>

#include "IncludeVulkan.hpp"
#include "memory/DeviceAllocator/DeviceAllocator.hpp"

namespace shuttle_engine {

    // Структуры, которые мы используем в GeometryStore
    struct PositionAttribute {
        alignas(16) glm::vec3 position;
    };

    struct NormalTangentUvAttribute {
        alignas(16) glm::vec3 normal;
        alignas(16) glm::vec2 uv;
        alignas(16) glm::vec4 tangent;
    };

    // HostVertex — удобная упаковка, используется внутри загрузчика/optimizer
    struct HostVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent;
    };

    // "Сырой" меш, который возвращает AssimpLoader
    struct HostMesh {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec4> tangents; // если Assimp не дал — можно генерировать
        std::vector<uint32_t> indices;

        // Утилиты:
        [[nodiscard]] size_t vertexCount() const { return positions.size(); }
        [[nodiscard]] size_t indexCount()  const { return indices.size(); }

        // Возвращает упакованный массив HostVertex (для meshoptimizer / дальнейшей обработки)
        [[nodiscard]] std::vector<HostVertex> packVertices() const {
            std::vector<HostVertex> out;
            out.reserve(positions.size());
            for (size_t i = 0; i < positions.size(); ++i) {
                HostVertex v{};
                v.position = positions[i];
                v.normal   = (i < normals.size()) ? normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
                v.uv       = (i < uvs.size()) ? uvs[i] : glm::vec2(0.0f);
                v.tangent  = (i < tangents.size()) ? tangents[i] : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                out.push_back(v);
            }
            return out;
        }

        // Заполняет буфер NormalTangentUvAttribute из packed vertices
        static std::vector<NormalTangentUvAttribute> makeAttributeBuffer(const std::vector<HostVertex>& verts) {
            std::vector<NormalTangentUvAttribute> out; out.reserve(verts.size());
            for (auto &v : verts) {
                out.push_back({ v.normal, v.uv, v.tangent });
            }
            return out;
        }
    };

    // --- Ограничивающий бокс для Frustum Culling ---
    struct AABB {
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

        void extend(const glm::vec3& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
    };

    // --- Контейнер для данных меша на CPU ---
    struct HostMeshData {
        // Раздельные массивы для разных буферов на GPU
        std::vector<PositionAttribute> positions;
        std::vector<NormalTangentUvAttribute> attributes;
        std::vector<uint32_t> indices; // Индексы (общие для обоих буферов)

        AABB localAABB; // Локальный ограничивающий бокс меша (для куллинга)
    };

    struct HostMaterialProperties {
        // 16 байт
        glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f}; // Белый цвет, полная непрозрачность

        // 4 + 4 + 4 + 4 = 16 байт
        float metallicFactor{0.0f};           // По умолчанию диэлектрик (не металл)
        float roughnessFactor{1.0f};          // По умолчанию матовый (безопасное PBR-значение)
        float occlusionStrength{1.0f};        // Максимальное влияние AO
        float emissiveStrength{0.0f};         // По умолчанию свечение выключено

        // 12 + 4 = 16 байт
        glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f}; // Черный цвет (нет свечения)
        float padding{0.0f};                        // Явное обнуление паддинга
    };


    struct MipInfo {
        uint32_t width;
        uint32_t height;
        size_t offset;
        size_t size;
    };

    struct HostImageData {
        uint32_t width{}, height{};
        vk::Format imageFormat = vk::Format::eR8G8B8A8Unorm;
        std::vector<uint8_t> data; // Все уровни подряд
        std::vector<MipInfo> mipChain;

        // Конструктор по умолчанию (уже есть)
        HostImageData() = default;

        // Конструктор перемещения
        HostImageData(HostImageData&& other) noexcept
            : width(other.width),
              height(other.height),
              imageFormat(other.imageFormat),
              data(std::move(other.data)), // Ключевой момент!
              mipChain(std::move(other.mipChain)) {}

        // Оператор присваивания перемещением
        HostImageData& operator=(HostImageData&& other) noexcept {
            if (this != &other) {
                width = other.width;
                height = other.height;
                imageFormat = other.imageFormat;
                data = std::move(other.data); // Ключевой момент!
                mipChain = std::move(other.mipChain);
                // Очищаем источник (не обязательно, но хорошая практика)
                other.width = 0; other.height = 0; other.data.clear();
            }
            return *this;
        }

        HostImageData(HostImageData const& other) = default;
        HostImageData& operator=(HostImageData const& other) = default;

        // Дополнительные методы
        [[nodiscard]] bool isEmpty() const { return data.empty(); }
    };



    struct HostMaterialData {

        HostMaterialProperties materialProperties;

        std::optional<HostImageData> albedoTexture;
        std::optional<HostImageData> normalTexture;
        std::optional<HostImageData> ormTexture;
        std::optional<HostImageData> emissiveTexture;
        std::optional<HostImageData> heightTexture;
    };

    struct HostMeshInstance {
        uint32_t meshIndex{0};
        uint32_t materialIndex{0};
    };

    struct HostNode {
        std::string name;
        glm::mat4 localTransform;
        std::vector<HostMeshInstance> meshes;
        std::vector<HostNode> children;
    };

    struct HostDirectionalLight {
        glm::vec4 direction{0.5f, 1.0f, 0.3f, 1.0f}; // Направление (куда светит солнце, вниз и немного вбок)
        glm::vec4 color{1.0f, 0.95f, 0.9f, 1.0f};       // Цвет света (чуть желтоватый, как настоящее солнце)
    };

    struct HostSceneData {
        std::vector<HostMeshData> meshes;
        std::vector<HostMaterialData> materials;

        HostNode rootNode;

        HostDirectionalLight sunLight;

        glm::vec4 ambientLight{0.2f, 0.5f, 1.0f, 0.1f};

        void merge(const HostSceneData& other, const glm::mat4& transform = glm::mat4(1.0f));
        void addTerrain(const HostMeshData& mesh, const HostMaterialData& material, const glm::mat4& transform = glm::mat4(1.0f));

    private:
        static void offsetNodeIndices(HostNode& node, uint32_t meshOffset, uint32_t materialOffset);
    };

}

#endif //HELLOTRIANGLE_RAWRENDERDATA_HPP
