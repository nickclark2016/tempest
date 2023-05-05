#ifndef tempest_gltfmodel_hpp
#define tempest_gltfmodel_hpp

#include "tempest/model.hpp"
#include <tempest/mat.hpp>
#include <tinygltf/tiny_gltf.h>
#include <tempest/vertex.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tempest::assets::gltf
{
    class gltfmodel : public model
    {
      public:
        struct primitive
        {
            std::uint32_t first_index;
            std::uint32_t index_count;
            std::int32_t material_index;
        };

        struct mesh
        {
            std::vector<primitive> primitives;
        };

        struct node
        {
            node* parent;
            std::vector<node*> children;
            mesh* m;
            tempest::math::mat<float, 4, 4> matrix;
            std::string name;
            bool visibile = true;

            node() : matrix(1.0f)
            {
            }

            ~node()
            {
                for (auto& child : children)
                {
                    delete child;
                }
            }
        };

        bool load_from_binary(const std::vector<unsigned char>& binary_data) override;
        bool load_from_ascii(const std::string& ascii_data) override;

        std::vector<node*> nodes;
        std::vector<core::vertex> vertices;
        std::vector<std::uint32_t> indices;

      private:
        tinygltf::Model _model;
        tinygltf::TinyGLTF _loader;

        void load_node(const tinygltf::Node& input_node, const tinygltf::Model& input, gltfmodel::node* parent,
                       std::vector<std::uint32_t>& index_buffer, std::vector<core::vertex>& vertex_buffer);
    };
} // namespace tempest::assets::gltf

#endif // tempest_gltfmodel_hpp
