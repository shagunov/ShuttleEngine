//
// Created by Shagu on 25.05.2026.
//

#include "Terrain.hpp"
#include <glm/glm.hpp>

namespace shuttle_engine {
    HostMeshData TerrainGeometryGenerator::createFromHeightMap(const TerrainProperties& props, const Image1D16bit& heightMap) {
        HostMeshData mesh;
        uint32_t width = props.meshResolution.width;
        uint32_t height = props.meshResolution.height;

        if (width < 2 || height < 2) {
            throw std::runtime_error("Terrain resolution must be at least 2x2 for geometry generation.");
        }

        float gridCellSizeX = props.worldSize.x / static_cast<float>(width - 1);
        float gridCellSizeY = props.worldSize.y / static_cast<float>(height - 1);

        // ВАЖНО: Теперь у нас есть вершины сетки (width * height)
        // + центральные вершины для каждого квадрата ((width - 1) * (height - 1))
        uint32_t numQuadsX = width - 1;
        uint32_t numQuadsY = height - 1;
        uint32_t gridVertices = width * height;
        uint32_t centerVertices = numQuadsX * numQuadsY;
        uint32_t totalVertices = gridVertices + centerVertices;

        // Каждый квадрат теперь генерирует 4 треугольника (4 * 3 = 12 индексов)
        uint32_t totalIndices = numQuadsX * numQuadsY * 12;

        mesh.positions.resize(totalVertices);
        mesh.normals.resize(totalVertices);
        mesh.uvs.resize(totalVertices);
        mesh.tangents.resize(totalVertices);
        mesh.indices.reserve(totalIndices);

        std::vector<glm::vec3> normalAccumulator(totalVertices, glm::vec3(0.0f));
        std::vector<glm::vec3> tangentAccumulator(totalVertices, glm::vec3(0.0f));
        std::vector<glm::vec3> bitangentAccumulator(totalVertices, glm::vec3(0.0f));

        // 1. ГЕНЕРАЦИЯ БАЗОВЫХ ВЕРШИН СЕТКИ (С ДЖИТТЕРОМ И ИДЕАЛЬНЫМИ НОРМАЛЯМИ)
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t idx = y * width + x;

                // Джиттер для разрушения сетки
                float jitterX = glm::sin(x * 12.9898f + y * 78.233f) * (gridCellSizeX * 0.35f);
                float jitterZ = glm::cos(x * 45.123f + y * 95.789f) * (gridCellSizeY * 0.35f);
                if (x == 0 || x == width - 1) jitterX = 0.0f;
                if (y == 0 || y == height - 1) jitterZ = 0.0f;

                float u_sample = static_cast<float>(x) / static_cast<float>(width - 1);
                float v_sample = static_cast<float>(y) / static_cast<float>(height - 1);

                float heightValue = heightMap.sampleBilinear(u_sample, v_sample);
                float worldHeight = props.minHeight + heightValue * (props.maxHeight - props.minHeight);

                mesh.positions[idx] = glm::vec3(
                    (x * gridCellSizeX - props.worldSize.x / 2.0f) + jitterX,
                    worldHeight,
                    (y * gridCellSizeY - props.worldSize.y / 2.0f) + jitterZ
                );

                mesh.uvs[idx] = glm::vec2(
                    u_sample * props.textureRepeatFactor.x,
                    v_sample * props.textureRepeatFactor.y
                );

                // =================================================================
                // СУПЕР-ФИКС: ВЫЧИСЛЕНИЕ СГЛАЖЕННОЙ НОРМАЛИ ИЗ КАРТЫ ВЫСОТ
                // =================================================================
                float texelSizeX = 1.0f / static_cast<float>(width - 1);
                float texelSizeY = 1.0f / static_cast<float>(height - 1);

                // Сэмплируем соседние пиксели карты высот
                float hL = heightMap.sampleBilinear(glm::max(0.0f, u_sample - texelSizeX), v_sample);
                float hR = heightMap.sampleBilinear(glm::min(1.0f, u_sample + texelSizeX), v_sample);
                float hD = heightMap.sampleBilinear(u_sample, glm::max(0.0f, v_sample - texelSizeY));
                float hU = heightMap.sampleBilinear(u_sample, glm::min(1.0f, v_sample + texelSizeY));

                // Переводим высоту в мировые координаты
                float heightScale = props.maxHeight - props.minHeight;
                hL *= heightScale; hR *= heightScale; hD *= heightScale; hU *= heightScale;

                // Вычисляем вектор нормали (метод центральных разностей)
                // Чем больше разница высот соседей, тем сильнее отклоняется нормаль
                glm::vec3 normal;
                normal.x = -(hR - hL) / (2.0f * gridCellSizeX);
                normal.z = -(hU - hD) / (2.0f * gridCellSizeY);
                normal.y = 1.0f; // Вектор "вверх"

                mesh.normals[idx] = glm::normalize(normal);
            }
        }


        // =========================================================================
        // 2. ГЕНЕРАЦИЯ ЦЕНТРАЛЬНЫХ ВЕРШИН ДЛЯ КАЖДОГО КВАДРАТА (CENTROIDS)
        // =========================================================================
        for (uint32_t y = 0; y < numQuadsY; ++y) {
            for (uint32_t x = 0; x < numQuadsX; ++x) {
                // Индекс новой центральной вершины в общем массиве
                uint32_t centerIdx = gridVertices + (y * numQuadsX + x);

                // Индексы углов текущего квадрата
                uint32_t i0 = y * width + x;             // TopLeft
                uint32_t i1 = y * width + x + 1;         // TopRight
                uint32_t i2 = (y + 1) * width + x;       // BottomLeft
                uint32_t i3 = (y + 1) * width + x + 1;   // BottomRight

                // Позиция центра — это среднее арифметическое углов (идеально вписывается в искажения)
                mesh.positions[centerIdx] = (
                    mesh.positions[i0] +
                    mesh.positions[i1] +
                    mesh.positions[i2] +
                    mesh.positions[i3]
                ) * 0.25f;

                // UV центра — среднее арифметическое UV углов
                mesh.uvs[centerIdx] = (
                    mesh.uvs[i0] +
                    mesh.uvs[i1] +
                    mesh.uvs[i2] +
                    mesh.uvs[i3]
                ) * 0.25f;

            }
        }

        // =========================================================================
        // 3. ТРИАНГУЛЯЦИЯ СЕТКИ (РАЗБИЕНИЕ НА 4 ТРЕУГОЛЬНИКА С ОБХОДОМ CCW)
        // =========================================================================
        for (uint32_t y = 0; y < numQuadsY; ++y) {
            for (uint32_t x = 0; x < numQuadsX; ++x) {
                uint32_t i0 = y * width + x;             // TopLeft (TL)
                uint32_t i1 = y * width + x + 1;         // TopRight (TR)
                uint32_t i2 = (y + 1) * width + x;       // BottomLeft (BL)
                uint32_t i3 = (y + 1) * width + x + 1;   // BottomRight (BR)

                uint32_t iCenter = gridVertices + (y * numQuadsX + x); // Center

                // 1. Верхний треугольник: TL -> Center -> TR
                mesh.indices.push_back(i0);
                mesh.indices.push_back(iCenter);
                mesh.indices.push_back(i1);

                // 2. Правый треугольник: TR -> Center -> BR
                mesh.indices.push_back(i1);
                mesh.indices.push_back(iCenter);
                mesh.indices.push_back(i3);

                // 3. Нижний треугольник: BR -> Center -> BL
                mesh.indices.push_back(i3);
                mesh.indices.push_back(iCenter);
                mesh.indices.push_back(i2);

                // 4. Левый треугольник: BL -> Center -> TL
                mesh.indices.push_back(i2);
                mesh.indices.push_back(iCenter);
                mesh.indices.push_back(i0);
            }
        }

        // =========================================================================
        // 4. РАСЧЕТ НОРМАЛЕЙ, ТАНГЕНСОВ И БИТАНГЕНСОВ (АККУМУЛЯЦИЯ)
        // =========================================================================
        // Сюда изменения не требуются! Цикл идет по массиву индексов,
        // который мы заполнили выше. Он автоматически просчитает нормали
        // как для базовых, так и для центральных вершин.
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            uint32_t i0 = mesh.indices[i + 0];
            uint32_t i1 = mesh.indices[i + 1];
            uint32_t i2 = mesh.indices[i + 2];

            const glm::vec3& v0 = mesh.positions[i0];
            const glm::vec3& v1 = mesh.positions[i1];
            const glm::vec3& v2 = mesh.positions[i2];

            const glm::vec2& uv0 = mesh.uvs[i0];
            const glm::vec2& uv1 = mesh.uvs[i1];
            const glm::vec2& uv2 = mesh.uvs[i2];

            glm::vec3 edge1 = v1 - v0;
            glm::vec3 edge2 = v2 - v0;

            float du1 = uv1.x - uv0.x;
            float dv1 = uv1.y - uv0.y;
            float du2 = uv2.x - uv0.x;
            float dv2 = uv2.y - uv0.y;

            glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            normalAccumulator[i0] += faceNormal;
            normalAccumulator[i1] += faceNormal;
            normalAccumulator[i2] += faceNormal;

            float det = (du1 * dv2 - dv1 * du2);
            if (std::abs(det) > 1e-6f) {
                float f = 1.0f / det;

                glm::vec3 tangent;
                tangent.x = f * (dv2 * edge1.x - dv1 * edge2.x);
                tangent.y = f * (dv2 * edge1.y - dv1 * edge2.y);
                tangent.z = f * (dv2 * edge1.z - dv1 * edge2.z);
                tangent = glm::normalize(tangent);

                glm::vec3 bitangent;
                bitangent.x = f * (-du2 * edge1.x + du1 * edge2.x);
                bitangent.y = f * (-du2 * edge1.y + du1 * edge2.y);
                bitangent.z = f * (-du2 * edge1.z + du1 * edge2.z);
                bitangent = glm::normalize(bitangent);

                tangentAccumulator[i0] += tangent;
                tangentAccumulator[i1] += tangent;
                tangentAccumulator[i2] += tangent;

                bitangentAccumulator[i0] += bitangent;
                bitangentAccumulator[i1] += bitangent;
                bitangentAccumulator[i2] += bitangent;
            }
        }

        // =========================================================================
        // 5. ФИНАЛИЗАЦИЯ (ГРАМА-ШМИДТ И НАПРАВЛЕНИЕ W)
        // =========================================================================
        for (uint32_t i = 0; i < totalVertices; ++i) {
            glm::vec3 n = glm::normalize(normalAccumulator[i]);
            glm::vec3 t = glm::normalize(tangentAccumulator[i]);
            glm::vec3 b = glm::normalize(bitangentAccumulator[i]);

            if (glm::length(t) < 1e-6f) {
                t = glm::vec3(1.0f, 0.0f, 0.0f);
            }

            glm::vec3 orthonormalTangent = glm::normalize(t - glm::dot(t, n) * n);
            float w = (glm::dot(glm::cross(n, orthonormalTangent), b) < 0.0f) ? -1.0f : 1.0f;

            mesh.normals[i] = n;
            mesh.tangents[i] = glm::vec4(orthonormalTangent, w);
        }

        return mesh;
    }
}