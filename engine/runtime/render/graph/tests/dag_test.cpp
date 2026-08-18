#include <gtest/gtest.h>

#include <tempest/render_graph/dag.hpp>
#include <tempest/utility.hpp>

namespace tempest::render_graph
{
    TEST(dag_test, linear_pipeline_ordering)
    {
        auto compiler = dag_compiler{};

        const auto tex1 = compiler.register_texture(rg_texture_desc{.name = "Tex1"});
        const auto tex2 = compiler.register_texture(rg_texture_desc{.name = "Tex2"});

        // Pass A: writes Tex1
        compiler.add_pass(pass_node{
            .name = "PassA",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = tex1,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, tex1},
        });

        // Pass B: reads Tex1, writes Tex2
        compiler.add_pass(pass_node{
            .name = "PassB",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = tex1,
                    .type = access_type::read,
                },
                texture_access{
                    .texture = tex2,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, tex2},
        });

        // Pass C: reads Tex2 (Sink)
        compiler.add_pass(pass_node{
            .name = "PassC",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = tex2,
                    .type = access_type::read,
                },
            },
            .is_sink = true,
        });

        const auto result = compiler.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        ASSERT_EQ(dag.sorted_pass_indices.size(), 3U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U); // PassA
        EXPECT_EQ(dag.sorted_pass_indices[1], 1U); // PassB
        EXPECT_EQ(dag.sorted_pass_indices[2], 2U); // PassC

        // Check lifetimes
        ASSERT_TRUE(dag.texture_lifetimes.contains(tex1.id));
        EXPECT_EQ(dag.texture_lifetimes.find(tex1.id)->second.first_pass, 0U); // PassA
        EXPECT_EQ(dag.texture_lifetimes.find(tex1.id)->second.last_pass, 1U);  // PassB

        ASSERT_TRUE(dag.texture_lifetimes.contains(tex2.id));
        EXPECT_EQ(dag.texture_lifetimes.find(tex2.id)->second.first_pass, 1U); // PassB
        EXPECT_EQ(dag.texture_lifetimes.find(tex2.id)->second.last_pass, 2U);  // PassC
    }

    TEST(dag_test, dead_code_culling)
    {
        auto compiler = dag_compiler{};

        const auto t_used = compiler.register_texture(rg_texture_desc{.name = "UsedTex"});
        const auto t_unused1 = compiler.register_texture(rg_texture_desc{.name = "UnusedTex1"});
        const auto t_unused2 = compiler.register_texture(rg_texture_desc{.name = "UnusedTex2"});

        // Active chain: Pass 0 -> Pass 1 (Sink)
        compiler.add_pass(pass_node{
            .name = "ActiveProducer",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = t_used,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, t_used},
        });

        compiler.add_pass(pass_node{
            .name = "ActiveSink",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = t_used,
                    .type = access_type::read,
                },
            },
            .is_sink = true,
        });

        // Dead chain: Pass 2 -> Pass 3 (Neither is a sink and output is unread)
        compiler.add_pass(pass_node{
            .name = "DeadPassA",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = t_unused1,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, t_unused1},
        });

        compiler.add_pass(pass_node{
            .name = "DeadPassB",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = t_unused1,
                    .type = access_type::read,
                },
                texture_access{
                    .texture = t_unused2,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, t_unused2},
            .is_sink = false,
        });

        const auto result = compiler.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        ASSERT_EQ(dag.sorted_pass_indices.size(), 2U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U); // ActiveProducer
        EXPECT_EQ(dag.sorted_pass_indices[1], 1U); // ActiveSink

        // Unused texture should not have lifetimes computed
        EXPECT_FALSE(dag.texture_lifetimes.contains(t_unused1.id));
        EXPECT_FALSE(dag.texture_lifetimes.contains(t_unused2.id));
    }

    TEST(dag_test, mark_sink_preserves_pass)
    {
        auto compiler = dag_compiler{};

        const auto buf = compiler.register_buffer(rg_buffer_desc{.size = 1024, .name = "Readback"});

        // A pass that reads a buffer and writes to CPU memory (marked as sink)
        compiler.add_pass(pass_node{
            .name = "ReadbackSinkPass",
            .buffer_accesses = vector<buffer_access>{
                init_list,
                buffer_access{
                    .buffer = buf,
                    .type = access_type::read,
                },
            },
            .is_sink = true,
        });

        const auto result = compiler.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        ASSERT_EQ(dag.sorted_pass_indices.size(), 1U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U);
    }

    TEST(dag_test, conditional_pass_fallback_aliasing)
    {
        auto compiler = dag_compiler{};

        const auto base_tex = compiler.register_texture(rg_texture_desc{.name = "BaseTex"});
        const auto bloom_tex = compiler.register_texture(rg_texture_desc{.name = "BloomTex"});

        // Pass 0: Base producer
        compiler.add_pass(pass_node{
            .name = "BaseProducer",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = base_tex,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, base_tex},
        });

        // Pass 1: Bloom pass (DISABLED) with fallback bloom_tex -> base_tex
        auto bloom_fallbacks = flat_unordered_map<rg_texture_id, rg_texture_id>{};
        bloom_fallbacks[bloom_tex] = base_tex;

        compiler.add_pass(pass_node{
            .name = "BloomPass",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = base_tex,
                    .type = access_type::read,
                },
                texture_access{
                    .texture = bloom_tex,
                    .type = access_type::write,
                },
            },
            .texture_outputs = vector<rg_texture_id>{init_list, bloom_tex},
            .texture_fallbacks = tempest::move(bloom_fallbacks),
            .enable_condition = [] { return false; }, // DISABLED!
        });

        // Pass 2: Tonemap pass reads bloom_tex (Sink)
        compiler.add_pass(pass_node{
            .name = "TonemapPass",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{
                    .texture = bloom_tex,
                    .type = access_type::read,
                },
            },
            .is_sink = true,
        });

        const auto result = compiler.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        // Pass 1 should be omitted, so only Pass 0 and Pass 2 run
        ASSERT_EQ(dag.sorted_pass_indices.size(), 2U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U); // BaseProducer
        EXPECT_EQ(dag.sorted_pass_indices[1], 2U); // TonemapPass

        // Verify that bloom_tex was remapped to base_tex
        ASSERT_TRUE(dag.resolved_texture_aliases.contains(bloom_tex));
        EXPECT_EQ(dag.resolved_texture_aliases.find(bloom_tex)->second, base_tex);

        // Verify base_tex lifetime extends across Pass 0 (order 0) to Pass 2 (order 1)
        ASSERT_TRUE(dag.texture_lifetimes.contains(base_tex.id));
        EXPECT_EQ(dag.texture_lifetimes.find(base_tex.id)->second.first_pass, 0U);
        EXPECT_EQ(dag.texture_lifetimes.find(base_tex.id)->second.last_pass, 1U);
    }

    TEST(dag_test, cycle_detection)
    {
        auto compiler = dag_compiler{};

        const auto t1 = compiler.register_texture(rg_texture_desc{.name = "T1"});
        const auto t2 = compiler.register_texture(rg_texture_desc{.name = "T2"});

        // Circular: Pass 0 reads T2 and writes T1; Pass 1 reads T1 and writes T2
        compiler.add_pass(pass_node{
            .name = "PassA",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{.texture = t2, .type = access_type::read},
                texture_access{.texture = t1, .type = access_type::write},
            },
            .texture_outputs = vector<rg_texture_id>{init_list, t1},
            .is_sink = true,
        });

        compiler.add_pass(pass_node{
            .name = "PassB",
            .texture_accesses = vector<texture_access>{
                init_list,
                texture_access{.texture = t1, .type = access_type::read},
                texture_access{.texture = t2, .type = access_type::write},
            },
            .texture_outputs = vector<rg_texture_id>{init_list, t2},
            .is_sink = true,
        });

        const auto result = compiler.compile();
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), dag_compile_error::cycle_detected);
    }

    TEST(dag_test, access_descriptors_and_lifetime_validity)
    {
        const auto default_lifetime = resource_lifetime{};
        EXPECT_FALSE(default_lifetime.is_valid());
        EXPECT_FALSE(static_cast<bool>(default_lifetime));
        EXPECT_EQ(default_lifetime.first_pass, resource_lifetime::invalid_pass);
        EXPECT_EQ(default_lifetime.last_pass, resource_lifetime::invalid_pass);

        const auto valid_lifetime = resource_lifetime{.first_pass = 0, .last_pass = 2};
        EXPECT_TRUE(valid_lifetime.is_valid());
        EXPECT_TRUE(static_cast<bool>(valid_lifetime));

        const auto default_buf_access = buffer_access{};
        EXPECT_EQ(default_buf_access.size, buffer_access::whole_size);

        const auto default_tex_access = texture_access{};
        EXPECT_EQ(default_tex_access.attachment, attachment_type::none);

        const auto color_tex_access = texture_access{.attachment = attachment_type::color};
        EXPECT_EQ(color_tex_access.attachment, attachment_type::color);
    }
} // namespace tempest::render_graph

