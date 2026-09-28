#include <array>
#include <cassert>
#include <memory>
#include <thread>

#include <tempest/physics/shim/jolt_shim.hpp>

// Jolt.h MUST be included before any other Jolt headers as it defines core macros and types
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

namespace jolt::shim
{
    namespace
    {
        constexpr auto broad_phase_layer_non_moving = JPH::BroadPhaseLayer{0};
        constexpr auto broad_phase_layer_moving = JPH::BroadPhaseLayer{1};
        constexpr auto broad_phase_layer_count = JPH::uint{2};
        constexpr auto object_layer_count = size_t{2};
        constexpr auto default_body_mutex_count = uint32_t{0};
        constexpr auto min_heightfield_sample_count = uint32_t{4};
    } // namespace

    class bp_layer_interface_impl final : public JPH::BroadPhaseLayerInterface
    {
      public:
        bp_layer_interface_impl()
        {
            mObjectToBroadPhase[static_cast<uint16_t>(object_layer::non_moving)] = broad_phase_layer_non_moving;
            mObjectToBroadPhase[static_cast<uint16_t>(object_layer::moving)] = broad_phase_layer_moving;
        }

        bp_layer_interface_impl(const bp_layer_interface_impl&) = delete;
        bp_layer_interface_impl(bp_layer_interface_impl&&) noexcept = delete;
        ~bp_layer_interface_impl() override = default;

        auto operator=(const bp_layer_interface_impl&) -> bp_layer_interface_impl& = delete;
        auto operator=(bp_layer_interface_impl&&) noexcept -> bp_layer_interface_impl& = delete;

        [[nodiscard]] auto GetNumBroadPhaseLayers() const -> JPH::uint override
        {
            return broad_phase_layer_count;
        }

        [[nodiscard]] auto GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const -> JPH::BroadPhaseLayer override
        {
            JPH_ASSERT(inLayer < object_layer_count);
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        [[nodiscard]] auto GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const -> const char* override
        {
            switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
            {
            case static_cast<JPH::BroadPhaseLayer::Type>(broad_phase_layer_non_moving):
                return "NON_MOVING";
            case static_cast<JPH::BroadPhaseLayer::Type>(broad_phase_layer_moving):
                return "MOVING";
            default:
                JPH_ASSERT(false);
                return "INVALID";
            }
        }
#endif

      private:
        std::array<JPH::BroadPhaseLayer, object_layer_count> mObjectToBroadPhase{};
    };

    class object_vs_broad_phase_layer_filter_impl final : public JPH::ObjectVsBroadPhaseLayerFilter
    {
      public:
        object_vs_broad_phase_layer_filter_impl() = default;
        object_vs_broad_phase_layer_filter_impl(const object_vs_broad_phase_layer_filter_impl&) = delete;
        object_vs_broad_phase_layer_filter_impl(object_vs_broad_phase_layer_filter_impl&&) noexcept = delete;
        ~object_vs_broad_phase_layer_filter_impl() override = default;

        auto operator=(const object_vs_broad_phase_layer_filter_impl&)
            -> object_vs_broad_phase_layer_filter_impl& = delete;
        auto operator=(object_vs_broad_phase_layer_filter_impl&&) noexcept
            -> object_vs_broad_phase_layer_filter_impl& = delete;

        [[nodiscard]] auto ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const
            -> bool override
        {
            switch (static_cast<object_layer>(inLayer1))
            {
            case object_layer::non_moving:
                return inLayer2 == broad_phase_layer_moving;
            case object_layer::moving:
                return true;
            default:
                return false;
            }
        }
    };

    class object_layer_pair_filter_impl final : public JPH::ObjectLayerPairFilter
    {
      public:
        object_layer_pair_filter_impl() = default;
        object_layer_pair_filter_impl(const object_layer_pair_filter_impl&) = delete;
        object_layer_pair_filter_impl(object_layer_pair_filter_impl&&) noexcept = delete;
        ~object_layer_pair_filter_impl() override = default;

        auto operator=(const object_layer_pair_filter_impl&) -> object_layer_pair_filter_impl& = delete;
        auto operator=(object_layer_pair_filter_impl&&) noexcept -> object_layer_pair_filter_impl& = delete;

        [[nodiscard]] auto ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const -> bool override
        {
            switch (static_cast<object_layer>(inLayer1))
            {
            case object_layer::non_moving:
                return static_cast<object_layer>(inLayer2) == object_layer::moving;
            case object_layer::moving:
                return true;
            default:
                return false;
            }
        }
    };

    class tempest_jolt_job_system final : public JPH::JobSystemWithBarrier
    {
      public:
        struct create_desc
        {
            uint32_t max_jobs = 0;
            uint32_t max_barriers = 0;
            job_dispatch_fn dispatch_callback = nullptr;
            void* dispatcher_user_data = nullptr;
            uint32_t max_concurrency = 0;
        };

        explicit tempest_jolt_job_system(const create_desc& desc)
            : mDispatch(desc.dispatch_callback), mDispatcherUserData(desc.dispatcher_user_data),
              mMaxConcurrency(static_cast<int>(desc.max_concurrency))
        {
            JobSystemWithBarrier::Init(desc.max_barriers);
            mJobs.Init(desc.max_jobs, desc.max_jobs);
        }

        tempest_jolt_job_system(const tempest_jolt_job_system&) = delete;
        tempest_jolt_job_system(tempest_jolt_job_system&&) noexcept = delete;
        ~tempest_jolt_job_system() override = default;

        auto operator=(const tempest_jolt_job_system&) -> tempest_jolt_job_system& = delete;
        auto operator=(tempest_jolt_job_system&&) noexcept -> tempest_jolt_job_system& = delete;

        [[nodiscard]] auto GetMaxConcurrency() const -> int override
        {
            return mMaxConcurrency;
        }

        [[nodiscard]] auto CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction,
                                     uint32_t inNumDependencies = 0) -> JobHandle override
        {
            auto index = JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex;
            for (;;)
            {
                index = mJobs.ConstructObject(inName, inColor, this, inJobFunction, inNumDependencies);
                if (index != JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex)
                {
                    break;
                }
                std::this_thread::yield();
            }

            auto* const job = &mJobs.Get(index);
            auto handle = JobHandle(job);
            if (inNumDependencies == 0)
            {
                QueueJob(job);
            }
            return handle;
        }

        auto FreeJob(Job* inJob) -> void override
        {
            mJobs.DestructObject(inJob);
        }

      protected:
        auto QueueJob(Job* inJob) -> void override
        {
            inJob->AddRef();
            mDispatch(
                [](void* job_context) -> void {
                    auto* const job = static_cast<Job*>(job_context);
                    job->Execute();
                    job->Release();
                },
                inJob, mDispatcherUserData);
        }

        auto QueueJobs(Job** inJobs, uint32_t inNumJobs) -> void override
        {
            for (auto job_index = 0U; job_index < inNumJobs; ++job_index)
            {
                QueueJob(inJobs[job_index]);
            }
        }

      private:
        job_dispatch_fn mDispatch{nullptr};
        void* mDispatcherUserData{nullptr};
        int mMaxConcurrency{static_cast<int>(default_max_concurrency)};
        JPH::FixedSizeFreeList<Job> mJobs;
    };

    class physics_system_impl final : public physics_system
    {
      public:
        explicit physics_system_impl(const init_desc& description)
            : warn(description.warn), error(description.error), log_user_data(description.log_user_data),
              collision_steps(description.collision_steps)
        {
            temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(description.temp_allocator_size_bytes);
            job_system = std::make_unique<tempest_jolt_job_system>(tempest_jolt_job_system::create_desc{
                .max_jobs = JPH::cMaxPhysicsJobs,
                .max_barriers = JPH::cMaxPhysicsBarriers,
                .dispatch_callback = description.job_dispatch,
                .dispatcher_user_data = description.job_dispatcher_user_data,
                .max_concurrency = description.max_concurrency,
            });

            physics_system.Init(description.max_bodies, default_body_mutex_count, description.max_body_pairs,
                                description.max_contact_constraints, bp_layer_interface, obj_vs_bp_filter,
                                obj_pair_filter);
        }

        physics_system_impl(const physics_system_impl&) = delete;
        physics_system_impl(physics_system_impl&&) noexcept = delete;
        ~physics_system_impl() override = default;

        auto operator=(const physics_system_impl&) -> physics_system_impl& = delete;
        auto operator=(physics_system_impl&&) noexcept -> physics_system_impl& = delete;

        // Shapes
        [[nodiscard]] auto create_box_shape(vec3 half_extents) -> shape_handle override;
        [[nodiscard]] auto create_sphere_shape(float radius) -> shape_handle override;
        [[nodiscard]] auto create_capsule_shape(float half_height, float radius) -> shape_handle override;
        [[nodiscard]] auto create_heightfield_shape(const float* heights, uint32_t sample_count, vec3 offset,
                                                    vec3 scale) -> shape_handle override;
        auto destroy_shape(shape_handle shape) -> void override;

        // Bodies
        [[nodiscard]] auto create_body(const body_desc& description) -> body_id override;
        auto destroy_body(body_id target_body_id) -> void override;
        auto add_body(body_id target_body_id, bool activate = true) -> void override;
        auto remove_body(body_id target_body_id) -> void override;

        [[nodiscard]] auto get_body_position(body_id target_body_id) const -> vec3 override;
        auto set_body_position(body_id target_body_id, vec3 position) -> void override;
        [[nodiscard]] auto get_body_rotation(body_id target_body_id) const -> quat override;
        auto set_body_rotation(body_id target_body_id, quat rotation) -> void override;
        [[nodiscard]] auto get_body_linear_velocity(body_id target_body_id) const -> vec3 override;
        auto set_body_linear_velocity(body_id target_body_id, vec3 velocity) -> void override;

        // Queries & Stepping
        [[nodiscard]] auto cast_ray(const raycast_query& query) -> raycast_hit override;
        auto step(float delta_time) -> void override;

      private:
        bp_layer_interface_impl bp_layer_interface;
        object_vs_broad_phase_layer_filter_impl obj_vs_bp_filter;
        object_layer_pair_filter_impl obj_pair_filter;

        std::unique_ptr<JPH::TempAllocator> temp_allocator;
        std::unique_ptr<tempest_jolt_job_system> job_system;
        JPH::PhysicsSystem physics_system;

        log_fn warn{nullptr};
        log_fn error{nullptr};
        void* log_user_data{nullptr};
        uint32_t collision_steps{default_collision_steps};
    };

    auto physics_system_impl::create_box_shape(vec3 half_extents) -> shape_handle
    {
        const auto settings = JPH::BoxShapeSettings(JPH::Vec3(half_extents.x, half_extents.y, half_extents.z));
        auto result = settings.Create();
        if (result.IsValid())
        {
            auto shape = result.Get(); // NOLINT -- Intentional copy of the smart pointer
            shape->AddRef();
            return reinterpret_cast<shape_handle>(shape.GetPtr());
        }
        if (error != nullptr)
        {
            error(result.GetError().c_str(), log_user_data);
        }
        return nullptr;
    }

    auto physics_system_impl::create_sphere_shape(float radius) -> shape_handle
    {
        const auto settings = JPH::SphereShapeSettings(radius);
        auto result = settings.Create();
        if (result.IsValid())
        {
            auto shape = result.Get(); // NOLINT -- Intentional copy of the smart pointer
            shape->AddRef();
            return reinterpret_cast<shape_handle>(shape.GetPtr());
        }
        if (error != nullptr)
        {
            error(result.GetError().c_str(), log_user_data);
        }
        return nullptr;
    }

    auto physics_system_impl::create_capsule_shape(float half_height, float radius) -> shape_handle
    {
        const auto settings = JPH::CapsuleShapeSettings(half_height, radius);
        auto result = settings.Create();
        if (result.IsValid())
        {
            auto shape = result.Get(); // NOLINT -- Intentional copy of the smart pointer
            shape->AddRef();
            return reinterpret_cast<shape_handle>(shape.GetPtr());
        }
        if (error != nullptr)
        {
            error(result.GetError().c_str(), log_user_data);
        }
        return nullptr;
    }

    auto physics_system_impl::create_heightfield_shape(const float* heights, uint32_t sample_count, vec3 offset,
                                                       vec3 scale) -> shape_handle
    {
        if (heights == nullptr || sample_count < min_heightfield_sample_count)
        {
            if (error != nullptr)
            {
                error("Invalid heightfield parameters", log_user_data);
            }
            return nullptr;
        }

        const auto settings = JPH::HeightFieldShapeSettings(heights, JPH::Vec3(offset.x, offset.y, offset.z),
                                                            JPH::Vec3(scale.x, scale.y, scale.z), sample_count);
        auto result = settings.Create();
        if (result.IsValid())
        {
            auto shape = result.Get(); // NOLINT -- Intentional copy of the smart pointer
            shape->AddRef();
            return reinterpret_cast<shape_handle>(shape.GetPtr());
        }
        if (error != nullptr)
        {
            error(result.GetError().c_str(), log_user_data);
        }
        return nullptr;
    }

    auto physics_system_impl::destroy_shape(shape_handle shape) -> void
    {
        if (shape != nullptr)
        {
            reinterpret_cast<const JPH::Shape*>(shape)->Release();
        }
    }

    auto physics_system_impl::create_body(const body_desc& description) -> body_id
    {
        if (description.shape == nullptr)
        {
            return invalid_body_id;
        }

        auto motion = JPH::EMotionType::Static;
        switch (description.motion)
        {
        case motion_type::static_motion:
            motion = JPH::EMotionType::Static;
            break;
        case motion_type::kinematic:
            motion = JPH::EMotionType::Kinematic;
            break;
        case motion_type::dynamic:
            motion = JPH::EMotionType::Dynamic;
            break;
        }

        const auto body_settings = JPH::BodyCreationSettings(
            reinterpret_cast<const JPH::Shape*>(description.shape),
            JPH::RVec3(description.position.x, description.position.y, description.position.z),
            JPH::Quat(description.rotation.x, description.rotation.y, description.rotation.z, description.rotation.w),
            motion, static_cast<JPH::ObjectLayer>(description.layer));

        auto* const created_body = physics_system.GetBodyInterface().CreateBody(body_settings);
        if (created_body == nullptr)
        {
            if (error != nullptr)
            {
                error("Failed to create Jolt body", log_user_data);
            }
            return invalid_body_id;
        }
        return created_body->GetID().GetIndexAndSequenceNumber();
    }

    auto physics_system_impl::destroy_body(body_id target_body_id) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        physics_system.GetBodyInterface().DestroyBody(JPH::BodyID(target_body_id));
    }

    auto physics_system_impl::add_body(body_id target_body_id, bool activate) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        physics_system.GetBodyInterface().AddBody(
            JPH::BodyID(target_body_id), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    auto physics_system_impl::remove_body(body_id target_body_id) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        physics_system.GetBodyInterface().RemoveBody(JPH::BodyID(target_body_id));
    }

    auto physics_system_impl::get_body_position(body_id target_body_id) const -> vec3
    {
        if (target_body_id == invalid_body_id)
        {
            return {};
        }
        const auto body_position = physics_system.GetBodyInterface().GetPosition(JPH::BodyID(target_body_id));
        return vec3{
            .x = body_position.GetX(),
            .y = body_position.GetY(),
            .z = body_position.GetZ(),
        };
    }

    auto physics_system_impl::set_body_position(body_id target_body_id, vec3 position) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        physics_system.GetBodyInterface().SetPosition(
            JPH::BodyID(target_body_id), JPH::RVec3(position.x, position.y, position.z), JPH::EActivation::Activate);
    }

    auto physics_system_impl::get_body_rotation(body_id target_body_id) const -> quat
    {
        if (target_body_id == invalid_body_id)
        {
            return {};
        }
        const auto body_rotation = physics_system.GetBodyInterface().GetRotation(JPH::BodyID(target_body_id));
        return quat{body_rotation.GetX(), body_rotation.GetY(), body_rotation.GetZ(), body_rotation.GetW()};
    }

    auto physics_system_impl::set_body_rotation(body_id target_body_id, quat rotation) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        physics_system.GetBodyInterface().SetRotation(JPH::BodyID(target_body_id),
                                                      JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
                                                      JPH::EActivation::Activate);
    }

    auto physics_system_impl::get_body_linear_velocity(body_id target_body_id) const -> vec3
    {
        if (target_body_id == invalid_body_id)
        {
            return {};
        }
        const auto body_velocity = physics_system.GetBodyInterface().GetLinearVelocity(JPH::BodyID(target_body_id));
        return vec3{
            .x = body_velocity.GetX(),
            .y = body_velocity.GetY(),
            .z = body_velocity.GetZ(),
        };
    }

    auto physics_system_impl::set_body_linear_velocity(body_id target_body_id, vec3 velocity) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        physics_system.GetBodyInterface().SetLinearVelocity(JPH::BodyID(target_body_id),
                                                            JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    auto physics_system_impl::cast_ray(const raycast_query& query) -> raycast_hit
    {
        auto result = raycast_hit{};
        result.has_hit = false;
        result.hit_body_id = invalid_body_id;

        const auto ray =
            JPH::RRayCast{JPH::RVec3(query.origin.x, query.origin.y, query.origin.z),
                          JPH::Vec3(query.direction.x * query.max_distance, query.direction.y * query.max_distance,
                                    query.direction.z * query.max_distance)};

        auto hit_result = JPH::RayCastResult{};
        if (physics_system.GetNarrowPhaseQuery().CastRay(ray, hit_result))
        {
            result.has_hit = true;
            result.distance = hit_result.mFraction * query.max_distance;
            result.hit_body_id = hit_result.mBodyID.GetIndexAndSequenceNumber();

            const auto hit_position = ray.GetPointOnRay(hit_result.mFraction);
            result.position = vec3{
                .x = hit_position.GetX(),
                .y = hit_position.GetY(),
                .z = hit_position.GetZ(),
            };

            const auto lock = JPH::BodyLockRead(physics_system.GetBodyLockInterface(), hit_result.mBodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                const auto surface_normal = body.GetWorldSpaceSurfaceNormal(hit_result.mSubShapeID2, hit_position);
                result.normal = vec3{
                    .x = surface_normal.GetX(),
                    .y = surface_normal.GetY(),
                    .z = surface_normal.GetZ(),
                };
            }
        }

        return result;
    }

    auto physics_system_impl::step(float delta_time) -> void
    {
        physics_system.Update(delta_time, static_cast<int>(collision_steps), temp_allocator.get(), job_system.get());
    }

    JOLT_SHIM_API auto create_physics_system(const init_desc& description) -> physics_system*
    {
        assert(description.job_dispatch != nullptr);

        if (JPH::Factory::sInstance == nullptr)
        {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory(); // NOLINT
            JPH::RegisterTypes();
        }

        return new physics_system_impl(description); // NOLINT
    }

    JOLT_SHIM_API auto destroy_physics_system(physics_system* system) -> void
    {
        delete system; // NOLINT
    }
} // namespace jolt::shim
