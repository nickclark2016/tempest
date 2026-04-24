#ifndef tempest_assets_entity_hierarchy_hpp
#define tempest_assets_entity_hierarchy_hpp

#include <tempest/int.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::assets
{
    struct entity_hierarchy
    {
        struct entity_record
        {
            vector<pair<size_t, vector<byte>>> components; // type_hash -> serialized component data
            vector<size_t> child_indices;
        };

        vector<entity_record> records;
        size_t root_index{0};
    };
} // namespace tempest::assets

#endif // tempest_assets_entity_hierarchy_hpp
