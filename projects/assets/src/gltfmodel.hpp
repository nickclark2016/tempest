#ifndef tempest_gltfmodel_hpp
#define tempest_gltfmodel_hpp

#include <tempest/model.hpp>
#include <tinygltf/tiny_gltf.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tempest::assets::gltf
{
    class gltfmodel : public model
    {
      public:
        bool load_from_binary(const std::vector<unsigned char>& binary_data);
        bool load_from_ascii(const std::string& ascii_data);

      private:
        tinygltf::Model _model;
        tinygltf::TinyGLTF _loader;

        void load_node(const tinygltf::Node& input_node, const tinygltf::Model& input, model::node* parent,
                       std::vector<std::uint32_t>& index_buffer, std::vector<core::vertex>& vertex_buffer);
    };
} // namespace tempest::assets::gltf

#endif // tempest_gltfmodel_hpp