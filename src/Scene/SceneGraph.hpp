#pragma once

#include <ranges>
#include <vector>
#include <memory>
#include <string>
#include <stack>
#include <queue>
#include <ranges>
#include <optional>
#include <glm/glm.hpp>

namespace shuttle_engine {

    // Простой инстанс меша: индекс меша и индекс материала
    struct alignas(16) ModelInstance {
        uint32_t meshId = 0;
        uint32_t materialId = 0;
        uint32_t modelId = 0;
    };

    struct alignas(16) ModelMatrices {
        glm::mat4 model;
        glm::mat4 normal;
    };

    // Узел сцены (Node)
    class Node {
    public:
        Node(uint32_t id_, std::string name_)
            : id(id_), name(std::move(name_)) {}

        uint32_t id;
        std::string name;
        bool isStatic = false; // Флаг: статична ли нода

        // Трансформации: локальная и мировая
        glm::mat4 localTransform{1.0f};
        glm::mat4 worldTransform{1.0f};

        // Иерархия
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;

        // Список мешей, прикрепленных к этой ноде
        std::vector<ModelInstance> meshInstances;

        // Удобный метод добавления дочерней ноды
        void addChild(std::unique_ptr<Node> child) {
            child->parent = this;
            children.emplace_back(std::move(child));
        }
    };

    // ---------------------------
    // DFS Iterator (обход в глубину)
    // ---------------------------
    class DFSIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Node*;
        using difference_type = std::ptrdiff_t;
        using pointer = Node**;
        using reference = Node*&;

        DFSIterator() = default;
        explicit DFSIterator(Node* root) {
            if (root) stack_.push(root), current_ = root;
        }

        Node* operator*() const { return current_; }
        Node* operator->() const { return current_; }

        // Префиксный ++
        DFSIterator& operator++() {
            if (stack_.empty()) {
                current_ = nullptr;
                return *this;
            }

            Node* node = stack_.top();
            stack_.pop();

            // Положим детей в стек в обратном порядке,
            // чтобы первый ребёнок обрабатывался раньше
            for (auto & it : std::views::reverse(node->children)) {
                stack_.push(it.get());
            }

            if (!stack_.empty()) current_ = stack_.top();
            else current_ = nullptr;
            return *this;
        }

        DFSIterator operator++(int) { DFSIterator tmp = *this; ++(*this); return tmp; }

        // Сравнение с другим итератором
        bool operator==(const DFSIterator& other) const {
            return current_ == other.current_;
        }
        bool operator!=(const DFSIterator& other) const { return !(*this == other); }

        // Совместимость с std::default_sentinel_t (C++20 ranges)
        bool operator==(std::default_sentinel_t) const { return current_ == nullptr; }
        bool operator!=(std::default_sentinel_t) const { return current_ != nullptr; }

    private:
        std::stack<Node*> stack_;
        Node* current_ = nullptr;
    };

    // ---------------------------
    // BFS Iterator (обход в ширину)
    // ---------------------------
    class BFSIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Node*;
        using difference_type = std::ptrdiff_t;
        using pointer = Node**;
        using reference = Node*&;

        BFSIterator() = default;
        explicit BFSIterator(Node* root) {
            if (root) q_.push(root), current_ = root;
        }

        Node* operator*() const { return current_; }
        Node* operator->() const { return current_; }

        BFSIterator& operator++() {
            if (q_.empty()) { current_ = nullptr; return *this; }

            Node* node = q_.front();
            q_.pop();
            for (const auto& child : node->children) q_.push(child.get());

            if (!q_.empty()) current_ = q_.front();
            else current_ = nullptr;
            return *this;
        }

        BFSIterator operator++(int) { BFSIterator tmp = *this; ++(*this); return tmp; }

        bool operator==(const BFSIterator& other) const { return current_ == other.current_; }
        bool operator!=(const BFSIterator& other) const { return !(*this == other); }

        bool operator==(std::default_sentinel_t) const { return current_ == nullptr; }
        bool operator!=(std::default_sentinel_t) const { return current_ != nullptr; }

    private:
        std::queue<Node*> q_;
        Node* current_ = nullptr;
    };

    // ---------------------------
    // Views / Traversers — совместимы с range-based for и std::ranges
    // ---------------------------
    class DFSTraverser : public std::ranges::view_interface<DFSTraverser> {
    public:
        explicit DFSTraverser(Node* root = nullptr) : root_(root) {}
        [[nodiscard]] DFSIterator begin() const { return DFSIterator(root_); }
        [[nodiscard]] std::default_sentinel_t end() const noexcept { return std::default_sentinel; }
    private:
        Node* root_;
    };

    class BFSTraverser : public std::ranges::view_interface<BFSTraverser> {
    public:
        explicit BFSTraverser(Node* root = nullptr) : root_(root) {}
        [[nodiscard]] BFSIterator begin() const { return BFSIterator(root_); }
        [[nodiscard]] std::default_sentinel_t end() const noexcept { return std::default_sentinel; }
    private:
        Node* root_;
    };

    // ---------------------------
    // SceneGraph — хранит корень и плоский кеш, методы управления
    // ---------------------------
    class SceneGraph {
    public:
        SceneGraph() {
            root = std::make_unique<Node>(nextNodeId++, "Root");
            allNodesCache.push_back(root.get());
        }

        // Добавить ноду в граф (возвращает сырой указатель на созданную ноду)
        Node* addNode(std::unique_ptr<Node>&& newNode, Node* parent = nullptr) {
            if (!newNode) return nullptr;
            newNode->id = nextNodeId++;
            Node* raw = newNode.get();

            if (parent) {
                parent->addChild(std::move(newNode));
            } else {
                root->addChild(std::move(newNode));
            }

            allNodesCache.push_back(raw);
            return raw;
        }

        // Поиск по ID (линейный — для большинства сценариев этого достаточно)
        [[nodiscard]] Node* getNodeById(uint32_t id) const {
            for (Node* n : allNodesCache) if (n->id == id) return n;
            return nullptr;
        }

        [[nodiscard]] Node* getRoot() const { return root.get(); }

        // Range-совместимые представления
        [[nodiscard]] DFSTraverser dfs() const { return DFSTraverser(root.get()); }
        [[nodiscard]] BFSTraverser bfs() const { return BFSTraverser(root.get()); }

        // Обновляет worldTransform для всех нод (parent -> child)
        void updateWorldTransforms() const {
            // Итеративный обход (стек): гарантированно от родителя к детям
            std::stack<std::pair<Node*, glm::mat4>> st;
            st.emplace(root.get(), root->localTransform);

            while (!st.empty()) {
                auto [node, parentWorld] = st.top();
                st.pop();

                if (node->parent == nullptr) {
                    node->worldTransform = node->localTransform;
                } else {
                    node->worldTransform = parentWorld * node->localTransform;
                }

                // добавляем детей
                for (const auto& childPtr : node->children) {
                    st.emplace(childPtr.get(), node->worldTransform);
                }
            }
        }

        // Собирает плоский буфер ModelData (повторяем для каждого meshInstance в ноде)
        const std::vector<glm::mat4>& collectModelMatrices() {
            modelMatricesCache.clear();
            // DFS/stack подойдет; используем DFS view
            for (Node* node : dfs()) {
                if (node->meshInstances.empty()) continue;
                // Формируем модельную матрицу для ноды
                glm::mat4 model = node->worldTransform;
                // Для каждого meshInstance в ноде добавляем одну запись модели
                for (size_t i = 0; i < node->meshInstances.size(); ++i) {
                    modelMatricesCache.push_back(model);
                }
            }
            return modelMatricesCache;
        }

        // Альтернатива: собрать ModelData (model + normalMatrix)
        [[nodiscard]] std::vector<glm::mat4> collectModelDataAsMatrices() const {
            std::vector<glm::mat4> out;
            out.reserve(1024);
            for (Node const * node : dfs()) {
                if (node->meshInstances.empty()) continue;
                glm::mat4 model = node->worldTransform;
                for (size_t i = 0; i < node->meshInstances.size(); ++i) {
                    out.push_back(model);
                }
            }
            return out;
        }

    private:
        std::unique_ptr<Node> root;
        std::vector<Node*> allNodesCache;
        std::vector<glm::mat4> modelMatricesCache;
        uint32_t nextNodeId = 1;
    };

} // namespace shuttle_engine