#include <tempest/vertex.hpp>

namespace tempest::core
{
    void mesh::release()
    {
        owner->deallocate(underlying);
    }

    mesh mesh::create_submesh(std::size_t first_index, std::size_t index_count)
    {
        mesh m = *this;
        m.owner = nullptr;
        m.underlying = nullptr;
        m.indices = m.indices.subspan(first_index, index_count);

        return m;
    }
} // namespace tempest::core