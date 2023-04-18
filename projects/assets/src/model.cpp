#include <tempest/model.hpp>
#include "gltfmodel.hpp"

namespace tempest::assets
{
    std::unique_ptr<model> model_factory::load(std::string data)
    {
        auto model = std::make_unique<gltf::gltfmodel>();
        model->load_from_ascii(data);

        return model;
    }
}