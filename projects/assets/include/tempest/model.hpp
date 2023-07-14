#ifndef tempest_model_hpp
#define tempest_model_hpp

#include "tempest/model.hpp"
#include <tempest/mat.hpp>
#include <tempest/vertex.hpp>

#include <vector>
#include <string>
#include <memory>

namespace tempest::assets
{
    class model
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
            node* parent = nullptr;
            std::vector<std::unique_ptr<node>> children;
            std::unique_ptr<mesh> m = nullptr;
            tempest::math::mat<float, 4, 4> matrix;
            std::string name;
            bool visibile = true;

            node() : matrix(1.0f)
            {
            }
        };

        node* root = nullptr;
        std::vector<core::vertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    class model_factory
    {
      public:
        static std::unique_ptr<model> load(const std::string data);
    };
} // namespace tempest::assets::gltf


#endif // tempest_model_hpp