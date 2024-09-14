#ifndef tempest_editor_asset_database_view_hpp
#define tempest_editor_asset_database_view_hpp

#include <tempest/asset_database.hpp>

namespace tempest::editor
{
    class asset_database_view
    {
      public:
        void update(tempest::assets::asset_database& db);
    };
}

#endif // tempest_editor_asset_database_view_hpp