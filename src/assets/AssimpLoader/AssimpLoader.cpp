//
// Created by Shagu on 25.05.2026.
//

#include "AssimpLoader.hpp"

#include <filesystem>

#include "assimp/postprocess.h"
#include "assimp/Scene.h"
#include "ImageLoader/Image.hpp"
#include <meshoptimizer.h>

namespace shuttle_engine {
    namespace {
        struct TempVertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv;
        };

        // 1. Конвертация матрицы Assimp в GLM
        glm::mat4 convertMatrix(const aiMatrix4x4& from) {
            glm::mat4 to;
            to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
            to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
            to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
            to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
            return to;
        }

        // 2. Загрузка геометрии меша
        HostMeshData processMesh(const aiMesh* mesh) {
            HostMeshData rawMesh;

            const uint32_t numVertices = mesh->mNumVertices;
            std::vector<TempVertex> tempVertices(numVertices);
            std::vector<uint32_t> tempIndices;
            tempIndices.reserve(mesh->mNumFaces * 3);

            // =========================================================================
            // ШАГ А: Сбор сырых вершин из Assimp в плоский массив
            // =========================================================================
            for (uint32_t i = 0; i < numVertices; ++i) {
                tempVertices[i].pos = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

                if (mesh->HasNormals()) {
                    tempVertices[i].normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
                } else {
                    tempVertices[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
                }

                if (mesh->HasTextureCoords(0)) {
                    tempVertices[i].uv = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
                } else {
                    tempVertices[i].uv = glm::vec2(0.0f, 0.0f);
                }
            }

            // Сбор индексов
            for (uint32_t i = 0; i < mesh->mNumFaces; ++i) {
                const aiFace& face = mesh->mFaces[i];
                if (face.mNumIndices == 3) { // Берем только треугольники
                    tempIndices.push_back(face.mIndices[0]);
                    tempIndices.push_back(face.mIndices[1]);
                    tempIndices.push_back(face.mIndices[2]);
                }
            }

            // =========================================================================
            // ШАГ Б: Дедупликация вершин (Deduplication & Remapping)
            // =========================================================================
            std::vector<unsigned int> remap(numVertices);
            size_t uniqueVertexCount = meshopt_generateVertexRemap(
                remap.data(),
                tempIndices.data(),
                tempIndices.size(),
                tempVertices.data(),
                numVertices,
                sizeof(TempVertex)
            );

            std::vector<TempVertex> uniqueVertices(uniqueVertexCount);
            std::vector<uint32_t> uniqueIndices(tempIndices.size());

            meshopt_remapVertexBuffer(uniqueVertices.data(), tempVertices.data(), numVertices, sizeof(TempVertex), remap.data());
            meshopt_remapIndexBuffer(uniqueIndices.data(), tempIndices.data(), tempIndices.size(), remap.data());

            // =========================================================================
            // ШАГ В: Оптимизация порядка индексов под кэш вершин GPU (Vertex Cache)
            // =========================================================================
            meshopt_optimizeVertexCache(uniqueIndices.data(), uniqueIndices.data(), uniqueIndices.size(), uniqueVertexCount);

            // =========================================================================
            // ШАГ Г: Оптимизация порядка вершин в памяти под кэш выборки (Vertex Fetch)
            // =========================================================================
            // Это переупорядочит уникальные вершины так, чтобы они читались GPU строго линейно
            meshopt_optimizeVertexFetch(uniqueVertices.data(), uniqueIndices.data(), uniqueIndices.size(), uniqueVertices.data(), uniqueVertexCount, sizeof(TempVertex));

            // =========================================================================
            // ШАГ Д: Генерация тангентов на финальных, оптимизированных вершинах
            // =========================================================================
            // Мы передаем указатели на финальный массив uniqueVertices прямо с нужным шагом (stride)!
            std::vector<glm::vec4> finalTangents(uniqueVertexCount);

            meshopt_generateTangents(
                reinterpret_cast<float*>(finalTangents.data()),      // result: массив vec4
                uniqueIndices.data(),                                // indices
                uniqueIndices.size(),                                // index_count
                &uniqueVertices[0].pos.x,                            // vertex_positions
                uniqueVertexCount,                                   // vertex_count
                sizeof(TempVertex),                                  // stride для позиций
                &uniqueVertices[0].normal.x,                         // vertex_normals
                sizeof(TempVertex),                                  // stride для нормалей
                &uniqueVertices[0].uv.x,                             // vertex_uvs
                sizeof(TempVertex),                                  // stride для UV
                0                                                    // options (0 по умолчанию)
            );

            // =========================================================================
            // ШАГ Е: Перелив в выходные структуры GeometryStore и расчет AABB
            // =========================================================================
            rawMesh.positions.resize(uniqueVertexCount);
            rawMesh.attributes.resize(uniqueVertexCount);
            rawMesh.indices = std::move(uniqueIndices); // Просто перемещаем индексы без копирования

            for (size_t i = 0; i < uniqueVertexCount; ++i) {
                // Разделяем на два независимых потока для прохода теней
                rawMesh.positions[i].position = uniqueVertices[i].pos;

                rawMesh.attributes[i].normal = uniqueVertices[i].normal;
                rawMesh.attributes[i].uv = uniqueVertices[i].uv;
                rawMesh.attributes[i].tangent = finalTangents[i];

                // Накапливаем габариты меша для фрустум-куллинга
                rawMesh.localAABB.extend(uniqueVertices[i].pos);
            }

            return rawMesh;
        }

        // 3. Загрузка текстуры (встроенной в GLB или внешней с диска)
        std::optional<HostImageData> loadTexture(aiScene const* scene, aiMaterial const* mat, aiTextureType type, const std::string& baseDir, vk::Format format) {
            aiString texturePath;
            if (mat->GetTexture(type, 0, &texturePath) != AI_SUCCESS) {
                return std::nullopt;
            }

            if (const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath.C_Str())) {
                if (embeddedTexture->mHeight == 0) {
                    // Сжатый формат (PNG/JPG) в памяти GLB
                    std::vector const memBuffer(
                        reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
                        reinterpret_cast<const unsigned char*>(embeddedTexture->pcData) + embeddedTexture->mWidth
                    );
                    return loadImageFromMemory(memBuffer, format);
                }
                // Несжатый формат (RGBA8) напрямую из GLB
                HostImageData rawImage;
                rawImage.width = embeddedTexture->mWidth;
                rawImage.height = embeddedTexture->mHeight;
                rawImage.imageFormat = format;
                size_t bufferSize = rawImage.width * rawImage.height * 4;
                rawImage.data.assign(
                    reinterpret_cast<const uint8_t*>(embeddedTexture->pcData),
                    reinterpret_cast<const uint8_t*>(embeddedTexture->pcData) + bufferSize
                );
                return rawImage;
            }
            // Обычный файл на диске
            std::string fullPath = baseDir + "/" + texturePath.C_Str();
            return loadImageFromFile(fullPath, format);
        }

        // 4. Загрузка материала
        HostMaterialData processMaterial(aiMaterial const* mat, aiScene const* scene, const std::string& baseDir) {
            HostMaterialData rawMat;

            aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
            if (aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &baseColor) == AI_SUCCESS) {
                rawMat.materialProperties.baseColorFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
            }

            float metallic = 1.0f;
            if (aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic) == AI_SUCCESS) {
                rawMat.materialProperties.metallicFactor = metallic;
            }

            float roughness = 1.0f;
            if (aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS) {
                rawMat.materialProperties.roughnessFactor = roughness;
            }

            rawMat.albedoTexture = loadTexture(scene, mat, aiTextureType_BASE_COLOR, baseDir, vk::Format::eR8G8B8A8Srgb);
            if (!rawMat.albedoTexture.has_value()) {
                rawMat.albedoTexture = loadTexture(scene, mat, aiTextureType_DIFFUSE, baseDir, vk::Format::eR8G8B8A8Srgb);
            }

            rawMat.ormTexture = loadTexture(scene, mat, aiTextureType_GLTF_METALLIC_ROUGHNESS, baseDir, vk::Format::eR8G8B8A8Unorm);
            if (!rawMat.ormTexture.has_value()) {
                std::optional<HostImageData> const ambientTexture = loadTexture(scene, mat, aiTextureType_AMBIENT_OCCLUSION, baseDir, vk::Format::eR8G8B8A8Unorm);
                std::optional<HostImageData> const roughnessTexture = loadTexture(scene, mat, aiTextureType_DIFFUSE_ROUGHNESS, baseDir, vk::Format::eR8G8B8A8Unorm);
                std::optional<HostImageData> const metallicTexture = loadTexture(scene, mat, aiTextureType_METALNESS, baseDir, vk::Format::eR8G8B8A8Unorm);

                rawMat.ormTexture = uniteSeparatedTexturesArm(
                    ambientTexture,
                    roughnessTexture,
                    metallicTexture
                );
            }

            rawMat.normalTexture = loadTexture(scene, mat, aiTextureType_NORMALS, baseDir, vk::Format::eR8G8B8A8Unorm);
            rawMat.emissiveTexture = loadTexture(scene, mat, aiTextureType_EMISSIVE, baseDir, vk::Format::eR8G8B8A8Srgb);

            return rawMat;
        }

        // 5. Рекурсивный обход дерева нод
        HostNode processNode(aiNode const* node, aiScene const* scene) {
            HostNode rawNode;
            rawNode.name = node->mName.C_Str();
            rawNode.localTransform = convertMatrix(node->mTransformation);

            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                uint32_t meshIdx = node->mMeshes[i];
                aiMesh* aiMesh = scene->mMeshes[meshIdx];

                HostMeshInstance instance;
                instance.meshIndex = meshIdx;
                instance.materialIndex = aiMesh->mMaterialIndex;

                rawNode.meshes.push_back(instance);
            }

            for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                rawNode.children.push_back(processNode(node->mChildren[i], scene));
            }

            return rawNode;
        }

        // 6. Главная функция сборки сцены внутри анонимного пространства
        HostSceneData processScene(aiScene const* scene, const std::string& baseDir) {
            HostSceneData sceneData;

            // Загружаем материалы
            sceneData.materials.reserve(scene->mNumMaterials);
            for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
                sceneData.materials.push_back(processMaterial(scene->mMaterials[i], scene, baseDir));
            }

            // Загружаем меши
            sceneData.meshes.reserve(scene->mNumMeshes);
            for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
                sceneData.meshes.push_back(processMesh(scene->mMeshes[i]));
            }

            // Строим иерархию нод
            if (scene->mRootNode) {
                sceneData.rootNode = processNode(scene->mRootNode, scene);
            }

            return sceneData;
        }
    }

    // Реализация основного метода класса
    HostSceneData AssimpLoader::loadScene(const std::string &filename) {
        // 1. Настраиваем угол сглаживания нормалей (80 градусов — золотой стандарт для архитектуры)
        importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.0f);

        // 2. Читаем файл с оптимизированным набором флагов
        const aiScene* scene = importer.ReadFile(filename,
            aiProcess_Triangulate |           // Обязательно: только треугольники
            aiProcess_GenSmoothNormals |      // Обязательно: генерируем сглаженные нормали
            aiProcess_FlipUVs |               // Обязательно: переворачиваем UV под Vulkan (Y смотрит вниз)
            aiProcess_GenBoundingBoxes        // Обязательно: генерируем AABB для каждого меша
        );

        // 3. Проверка на ошибки загрузки
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            throw std::runtime_error("[AssimpLoader] Failed to load file: " + filename + " (" + importer.GetErrorString() + ")");
        }

        // Извлекаем базовую директорию с помощью std::filesystem (избегаем ошибок с путями)
        std::string baseDir = std::filesystem::path(filename).parent_path().string();

        // 4. Запускаем наш оптимизированный парсинг сцены (где работает meshoptimizer)
        auto rawSceneData = processScene(scene, baseDir);

        // 5. Очищаем ресурсы Assimp
        importer.FreeScene();

        return rawSceneData;
    }

} // shuttle_engine