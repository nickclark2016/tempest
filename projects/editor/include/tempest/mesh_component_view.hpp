#ifndef tempest_editor_mesh_component_view_hpp
#define tempest_editor_mesh_component_view_hpp

#include <tempest/component_view.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/vertex.hpp>

namespace tempest::editor
{
    class mesh_component_view : public component_view_factory
    {
      public:
        mesh_component_view(const core::mesh_registry& mesh_reg) : _mesh_reg{&mesh_reg}
        {
        }

        bool create_view(tempest::ecs::registry& reg, tempest::ecs::entity ent) const override
        {
            if (auto mesh_comp = reg.try_get<core::mesh_component>(ent))
            {
                using imgui = tempest::graphics::imgui_context;

                imgui::create_header("Mesh Component", [&]() {
                    imgui::create_table("##mesh_component_container", 2, [&]() {
                        auto mesh = _mesh_reg->find(mesh_comp->mesh_id);
                        if (!mesh)
                        {
                            imgui::next_row();
                            imgui::next_column();
                            imgui::label("Mesh not found");
                            return;
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Name");
                        imgui::next_column();
                        imgui::label(string_view{mesh->name});

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Vertex Count");
                        imgui::next_column();
                        imgui::label(static_cast<uint32_t>(mesh->vertices.size()));

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Index Count");
                        imgui::next_column();
                        imgui::label(static_cast<uint32_t>(mesh->indices.size()));

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Triangle Count");
                        imgui::next_column();
                        imgui::label(static_cast<uint32_t>(mesh->num_triangles()));

                        imgui::next_row();
                        imgui::next_column();
                        imgui::checkbox("Has Normals", mesh->has_normals);

                        imgui::next_row();
                        imgui::next_column();
                        imgui::checkbox("Has Tangents", mesh->has_tangents);

                        imgui::next_row();
                        imgui::next_column();
                        imgui::checkbox("Has Colors", mesh->has_colors);

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Mesh ID");
                        imgui::next_column();
                        imgui::label(to_string(mesh_comp->mesh_id));
                    });
                });
            }
            return false;
        }

      private:
        const core::mesh_registry* _mesh_reg;
    };
} // namespace tempest::editor

#endif // tempest_editor_mesh_component_view_hpp