#ifndef tempest_model_hpp
#define tempest_model_hpp

#include <vector>
#include <string>
#include <memory>

namespace tempest::assets
{
    class model
    {
      public:
        virtual bool load_from_binary(const std::vector<unsigned char>& binary_data) = 0;
        virtual bool load_from_ascii(const std::string& ascii_data) = 0;
    };

    class model_factory
    {
      public:
        static std::unique_ptr<model> load(const std::string data);
    };
} // namespace tempest::assets::gltf


#endif // tempest_model_hpp