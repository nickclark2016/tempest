#ifndef tempest_editor_material_component_view_hpp
#define tempest_editor_material_component_view_hpp

#include <tempest/component_view.hpp>
#include <tempest/imgui_context.hpp>
#include <tempest/material.hpp>
#include <tempest/texture.hpp>

namespace tempest::editor
{
    class material_component_view : public component_view_factory
    {
      public:
        material_component_view(const core::material_registry& mat_reg, const core::texture_registry& tex_reg)
            : _mat_reg{&mat_reg}, _tex_reg{&tex_reg}
        {
        }

        bool create_view(tempest::ecs::archetype_registry& reg, tempest::ecs::entity ent) const override
        {
            if (auto mat_comp = reg.try_get<core::material_component>(ent))
            {
                using imgui = tempest::graphics::imgui_context;

                imgui::create_header("Material Component", [&]() {
                    imgui::create_table("##material_component_container", 2, [&]() {
                        auto mat = _mat_reg->get_material(mat_comp->material_id);
                        if (!mat)
                        {
                            imgui::next_row();
                            imgui::next_column();
                            imgui::label("Material not found");
                            return;
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Name");
                        imgui::next_column();
                        imgui::label(string_view{mat->get_name()});

                        // PBR properties
                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Base Color Texture");
                        imgui::next_column();
                        if (auto tex_id = mat->get_texture(core::material::base_color_texture_name); tex_id)
                        {
                            auto tex = _tex_reg->get_texture(tex_id.value());
                            if (tex)
                            {
                                if (tex->name.empty())
                                {
                                    imgui::label(to_string(tex_id.value()));
                                }
                                else
                                {
                                    imgui::label(tex->name);
                                }
                            }
                            else
                            {
                                imgui::label("Texture not found");
                            }
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Base Color");
                        imgui::next_column();
                        if (auto base_color_factor = mat->get_vec4(core::material::base_color_factor_name);
                            base_color_factor)
                        {
                            imgui::input_color("##base_color", base_color_factor.value(), false);
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Metallic Roughness Texture");
                        imgui::next_column();
                        if (auto mr_texture_id = mat->get_texture(core::material::metallic_roughness_texture_name);
                            mr_texture_id)
                        {
                            auto tex = _tex_reg->get_texture(mr_texture_id.value());
                            if (tex)
                            {
                                if (tex->name.empty())
                                {
                                    imgui::label(to_string(mr_texture_id.value()));
                                }
                                else
                                {
                                    imgui::label(tex->name);
                                }
                            }
                            else
                            {
                                imgui::label("Texture not found");
                            }
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Metallic Factor");
                        imgui::next_column();
                        if (auto metallic_factor = mat->get_scalar(core::material::metallic_factor_name);
                            metallic_factor)
                        {
                            imgui::input_float("##metallic_factor", metallic_factor.value());
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Roughness Factor");
                        imgui::next_column();
                        if (auto roughness_factor = mat->get_scalar(core::material::roughness_factor_name);
                            roughness_factor)
                        {
                            imgui::input_float("##roughness_factor", roughness_factor.value());
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Normal Texture");
                        imgui::next_column();
                        if (auto normal_texture_id = mat->get_texture(core::material::normal_texture_name);
                            normal_texture_id)
                        {
                            auto tex = _tex_reg->get_texture(normal_texture_id.value());
                            if (tex)
                            {
                                if (tex->name.empty())
                                {
                                    imgui::label(to_string(normal_texture_id.value()));
                                }
                                else
                                {
                                    imgui::label(tex->name);
                                }
                            }
                            else
                            {
                                imgui::label("Texture not found");
                            }
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Normal Scale");
                        imgui::next_column();
                        if (auto normal_scale = mat->get_scalar(core::material::normal_scale_name); normal_scale)
                        {
                            imgui::input_float("##normal_scale", normal_scale.value());
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Occlusion Texture");
                        imgui::next_column();
                        if (auto occlusion_texture_id = mat->get_texture(core::material::occlusion_texture_name);
                            occlusion_texture_id)
                        {
                            auto tex = _tex_reg->get_texture(occlusion_texture_id.value());
                            if (tex)
                            {
                                if (tex->name.empty())
                                {
                                    imgui::label(to_string(occlusion_texture_id.value()));
                                }
                                else
                                {
                                    imgui::label(tex->name);
                                }
                            }
                            else
                            {
                                imgui::label("Texture not found");
                            }
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Occlusion Strength");
                        imgui::next_column();
                        if (auto occlusion_strength = mat->get_scalar(core::material::occlusion_strength_name);
                            occlusion_strength)
                        {
                            imgui::input_float("##occlusion_strength", occlusion_strength.value());
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Emissive Texture");
                        imgui::next_column();
                        if (auto emissive_texture_id = mat->get_texture(core::material::emissive_texture_name);
                            emissive_texture_id)
                        {
                            auto tex = _tex_reg->get_texture(emissive_texture_id.value());
                            if (tex)
                            {
                                if (tex->name.empty())
                                {
                                    imgui::label(to_string(emissive_texture_id.value()));
                                }
                                else
                                {
                                    imgui::label(tex->name);
                                }
                            }
                            else
                            {
                                imgui::label("Texture not found");
                            }
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Emissive Color");
                        imgui::next_column();
                        if (auto emissive_factor = mat->get_vec3(core::material::emissive_factor_name); emissive_factor)
                        {
                            imgui::input_color("##emissive_factor", emissive_factor.value(), false);
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Alpha Mode");
                        imgui::next_column();
                        if (auto alpha_mode = mat->get_string(core::material::alpha_mode_name); alpha_mode)
                        {
                            imgui::label(string_view{alpha_mode.value()});
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Alpha Cutoff");
                        imgui::next_column();
                        if (auto alpha_cutoff = mat->get_scalar(core::material::alpha_cutoff_name); alpha_cutoff)
                        {
                            imgui::input_float("##alpha_cutoff", alpha_cutoff.value());
                        }
                        else
                        {
                            imgui::label("None");
                        }

                        imgui::next_row();
                        imgui::next_column();
                        imgui::label("Double Sided");
                        imgui::next_column();
                        if (auto double_sided = mat->get_bool(core::material::double_sided_name); double_sided)
                        {
                            imgui::checkbox("##double_sided", double_sided.value());
                        }
                        else
                        {
                            imgui::label("None");
                        }
                    });
                });
            }
            return false;
        }

      private:
        const core::material_registry* _mat_reg;
        const core::texture_registry* _tex_reg;
    };
} // namespace tempest::editor

#endif // tempest_editor_material_component_view_hpp