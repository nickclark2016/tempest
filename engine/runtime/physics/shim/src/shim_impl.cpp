#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
#include <utility>

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
#include <Jolt/Physics/Character/CharacterVirtual.h>
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
            : _dispatch(desc.dispatch_callback), _dispatch_user_data(desc.dispatcher_user_data),
              _max_concurrency(static_cast<int>(desc.max_concurrency))
        {
            JobSystemWithBarrier::Init(desc.max_barriers);
            _jobs.Init(desc.max_jobs, desc.max_jobs);
        }

        tempest_jolt_job_system(const tempest_jolt_job_system&) = delete;
        tempest_jolt_job_system(tempest_jolt_job_system&&) noexcept = delete;
        ~tempest_jolt_job_system() override
        {
            while (_active_jobs.load(std::memory_order_acquire) > 0)
            {
                std::this_thread::yield();
            }
        }

        auto operator=(const tempest_jolt_job_system&) -> tempest_jolt_job_system& = delete;
        auto operator=(tempest_jolt_job_system&&) noexcept -> tempest_jolt_job_system& = delete;

        [[nodiscard]] auto GetMaxConcurrency() const -> int override
        {
            return _max_concurrency;
        }

        [[nodiscard]] auto CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction,
                                     uint32_t inNumDependencies = 0) -> JobHandle override
        {
            _active_jobs.fetch_add(1, std::memory_order_relaxed);
            auto index = JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex;
            for (;;)
            {
                index = _jobs.ConstructObject(inName, inColor, this, inJobFunction, inNumDependencies);
                if (index != JPH::FixedSizeFreeList<Job>::cInvalidObjectIndex)
                {
                    break;
                }
                std::this_thread::yield();
            }

            auto* const job = &_jobs.Get(index);
            auto handle = JobHandle(job);
            if (inNumDependencies == 0)
            {
                QueueJob(job);
            }
            return handle;
        }

        auto FreeJob(Job* inJob) -> void override
        {
            _jobs.DestructObject(inJob);
            _active_jobs.fetch_sub(1, std::memory_order_release);
        }

      protected:
        auto QueueJob(Job* inJob) -> void override
        {
            inJob->AddRef();
            _dispatch(
                [](void* job_context) -> void {
                    auto* const job = static_cast<Job*>(job_context);
                    job->Execute();
                    job->Release();
                },
                inJob, _dispatch_user_data);
        }

        auto QueueJobs(Job** inJobs, uint32_t inNumJobs) -> void override
        {
            for (auto job_index = 0U; job_index < inNumJobs; ++job_index)
            {
                QueueJob(inJobs[job_index]);
            }
        }

      private:
        job_dispatch_fn _dispatch = nullptr;
        void* _dispatch_user_data = nullptr;
        int _max_concurrency = static_cast<int>(default_max_concurrency);
        std::atomic<uint32_t> _active_jobs{0};
        JPH::FixedSizeFreeList<Job> _jobs;
    };

    class character_virtual_impl final : public character_virtual, private JPH::CharacterContactListener
    {
      public:
        character_virtual_impl(JPH::Ref<JPH::CharacterVirtual> character, JPH::PhysicsSystem* physics_system,
                               JPH::TempAllocator* temp_allocator, float max_slope_angle,
                               character_contact_callbacks callbacks)
            : _character(std::move(character)), _physics_system(physics_system), _temp_allocator(temp_allocator),
              _max_slope_angle(max_slope_angle), _callbacks(callbacks)
        {
            if (_callbacks.on_contact_added != nullptr || _callbacks.on_contact_solve != nullptr)
            {
                _character->SetListener(this);
            }
        }

        character_virtual_impl(const character_virtual_impl&) = delete;
        character_virtual_impl(character_virtual_impl&&) noexcept = delete;
        ~character_virtual_impl() override
        {
            if (_character != nullptr)
            {
                _character->SetListener(nullptr);
            }
        }

        auto operator=(const character_virtual_impl&) -> character_virtual_impl& = delete;
        auto operator=(character_virtual_impl&&) noexcept -> character_virtual_impl& = delete;

        void update(float delta_time, vec3 gravity) override
        {
            const auto bp_filter =
                _physics_system->GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto obj_filter =
                _physics_system->GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto body_filter = JPH::BodyFilter{};
            const auto shape_filter = JPH::ShapeFilter{};

            _character->Update(delta_time, JPH::Vec3(gravity.x, gravity.y, gravity.z), bp_filter, obj_filter,
                                body_filter, shape_filter, *_temp_allocator);
        }

        void extended_update(float delta_time, vec3 gravity, const extended_update_settings& settings) override
        {
            const auto bp_filter =
                _physics_system->GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto obj_filter =
                _physics_system->GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto body_filter = JPH::BodyFilter{};
            const auto shape_filter = JPH::ShapeFilter{};

            auto jolt_settings = JPH::CharacterVirtual::ExtendedUpdateSettings{};
            jolt_settings.mStickToFloorStepDown =
                JPH::Vec3(settings.stick_to_floor_step_down.x, settings.stick_to_floor_step_down.y,
                          settings.stick_to_floor_step_down.z);
            jolt_settings.mWalkStairsStepUp = JPH::Vec3(settings.walk_stairs_step_up.x, settings.walk_stairs_step_up.y,
                                                        settings.walk_stairs_step_up.z);
            jolt_settings.mWalkStairsMinStepForward = settings.walk_stairs_min_step_forward;
            jolt_settings.mWalkStairsStepForwardTest = settings.walk_stairs_step_forward_test;
            jolt_settings.mWalkStairsCosAngleForwardContact = settings.walk_stairs_cos_angle_forward_contact;
            jolt_settings.mWalkStairsStepDownExtra =
                JPH::Vec3(settings.walk_stairs_step_down_extra.x, settings.walk_stairs_step_down_extra.y,
                          settings.walk_stairs_step_down_extra.z);

            _character->ExtendedUpdate(delta_time, JPH::Vec3(gravity.x, gravity.y, gravity.z), jolt_settings,
                                        bp_filter, obj_filter, body_filter, shape_filter, *_temp_allocator);
        }

        [[nodiscard]] auto walk_stairs(float delta_time, vec3 step_up, vec3 step_forward, vec3 step_forward_test,
                                       vec3 step_down_extra) -> bool override
        {
            const auto bp_filter =
                _physics_system->GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto obj_filter =
                _physics_system->GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto body_filter = JPH::BodyFilter{};
            const auto shape_filter = JPH::ShapeFilter{};

            return _character->WalkStairs(delta_time, JPH::Vec3(step_up.x, step_up.y, step_up.z),
                                           JPH::Vec3(step_forward.x, step_forward.y, step_forward.z),
                                           JPH::Vec3(step_forward_test.x, step_forward_test.y, step_forward_test.z),
                                           JPH::Vec3(step_down_extra.x, step_down_extra.y, step_down_extra.z),
                                           bp_filter, obj_filter, body_filter, shape_filter, *_temp_allocator);
        }

        [[nodiscard]] auto stick_to_floor(vec3 step_down) -> bool override
        {
            const auto bp_filter =
                _physics_system->GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto obj_filter =
                _physics_system->GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(object_layer::moving));
            const auto body_filter = JPH::BodyFilter{};
            const auto shape_filter = JPH::ShapeFilter{};

            return _character->StickToFloor(JPH::Vec3(step_down.x, step_down.y, step_down.z), bp_filter, obj_filter,
                                             body_filter, shape_filter, *_temp_allocator);
        }

        [[nodiscard]] auto can_walk_stairs(vec3 linear_velocity) const -> bool override
        {
            return _character->CanWalkStairs(JPH::Vec3(linear_velocity.x, linear_velocity.y, linear_velocity.z));
        }

        [[nodiscard]] auto cancel_velocity_towards_steep_slopes(vec3 desired_velocity) const -> vec3 override
        {
            const auto adjusted = _character->CancelVelocityTowardsSteepSlopes(
                JPH::Vec3(desired_velocity.x, desired_velocity.y, desired_velocity.z));
            return vec3{
                .x = adjusted.GetX(),
                .y = adjusted.GetY(),
                .z = adjusted.GetZ(),
            };
        }

        [[nodiscard]] auto get_position() const -> vec3 override
        {
            const auto pos = _character->GetPosition();
            return vec3{
                .x = pos.GetX(),
                .y = pos.GetY(),
                .z = pos.GetZ(),
            };
        }

        auto set_position(vec3 position) -> void override
        {
            _character->SetPosition(JPH::RVec3(position.x, position.y, position.z));
        }

        [[nodiscard]] auto get_rotation() const -> quat override
        {
            const auto rot = _character->GetRotation();
            return quat{
                .x = rot.GetX(),
                .y = rot.GetY(),
                .z = rot.GetZ(),
                .w = rot.GetW(),
            };
        }

        auto set_rotation(quat rotation) -> void override
        {
            _character->SetRotation(JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w));
        }

        [[nodiscard]] auto get_linear_velocity() const -> vec3 override
        {
            const auto vel = _character->GetLinearVelocity();
            return vec3{
                .x = vel.GetX(),
                .y = vel.GetY(),
                .z = vel.GetZ(),
            };
        }

        auto set_linear_velocity(vec3 velocity) -> void override
        {
            _character->SetLinearVelocity(JPH::Vec3(velocity.x, velocity.y, velocity.z));
        }

        [[nodiscard]] auto get_ground_state() const -> ground_state override
        {
            switch (_character->GetGroundState())
            {
            case JPH::CharacterBase::EGroundState::OnGround:
                return ground_state::on_ground;
            case JPH::CharacterBase::EGroundState::OnSteepGround:
                return ground_state::on_steep_ground;
            case JPH::CharacterBase::EGroundState::NotSupported:
                return ground_state::not_supported;
            case JPH::CharacterBase::EGroundState::InAir:
            default:
                return ground_state::in_air;
            }
        }

        [[nodiscard]] auto is_supported() const -> bool override
        {
            return _character->IsSupported();
        }

        [[nodiscard]] auto get_ground_position() const -> vec3 override
        {
            const auto pos = _character->GetGroundPosition();
            return vec3{
                .x = pos.GetX(),
                .y = pos.GetY(),
                .z = pos.GetZ(),
            };
        }

        [[nodiscard]] auto get_ground_normal() const -> vec3 override
        {
            const auto norm = _character->GetGroundNormal();
            return vec3{
                .x = norm.GetX(),
                .y = norm.GetY(),
                .z = norm.GetZ(),
            };
        }

        [[nodiscard]] auto get_ground_velocity() const -> vec3 override
        {
            const auto vel = _character->GetGroundVelocity();
            return vec3{
                .x = vel.GetX(),
                .y = vel.GetY(),
                .z = vel.GetZ(),
            };
        }

        [[nodiscard]] auto get_ground_body_id() const -> body_id override
        {
            const auto body = _character->GetGroundBodyID();
            if (body.IsInvalid())
            {
                return invalid_body_id;
            }
            return body.GetIndexAndSequenceNumber();
        }

        [[nodiscard]] auto get_mass() const -> float override
        {
            return _character->GetMass();
        }

        auto set_mass(float mass) -> void override
        {
            _character->SetMass(mass);
        }

        [[nodiscard]] auto get_max_slope_angle() const -> float override
        {
            return _max_slope_angle;
        }

        auto set_max_slope_angle(float max_slope_angle) -> void override
        {
            _max_slope_angle = max_slope_angle;
            _character->SetMaxSlopeAngle(max_slope_angle);
        }

      private:
        auto OnContactAdded([[maybe_unused]] const JPH::CharacterVirtual* inCharacter,
                            const JPH::CharacterContact& inContact,
                            [[maybe_unused]] JPH::CharacterContactSettings& ioSettings) -> void override
        {
            if (_callbacks.on_contact_added != nullptr)
            {
                const auto target_body =
                    inContact.mBodyB.IsInvalid() ? invalid_body_id : inContact.mBodyB.GetIndexAndSequenceNumber();
                const auto pos = vec3{
                    .x = inContact.mPosition.GetX(),
                    .y = inContact.mPosition.GetY(),
                    .z = inContact.mPosition.GetZ(),
                };
                const auto norm = vec3{
                    .x = inContact.mContactNormal.GetX(),
                    .y = inContact.mContactNormal.GetY(),
                    .z = inContact.mContactNormal.GetZ(),
                };
                _callbacks.on_contact_added(this, target_body, pos, norm, _callbacks.user_data);
            }
        }

        auto OnContactSolve([[maybe_unused]] const JPH::CharacterVirtual* inCharacter, const JPH::BodyID& inBodyID2,
                            [[maybe_unused]] const JPH::SubShapeID& inSubShapeID2, JPH::RVec3Arg inContactPosition,
                            JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity,
                            [[maybe_unused]] const JPH::PhysicsMaterial* inContactMaterial,
                            [[maybe_unused]] JPH::Vec3Arg inCharacterVelocity, JPH::Vec3& ioNewCharacterVelocity)
            -> void override
        {
            if (_callbacks.on_contact_solve != nullptr)
            {
                const auto target_body =
                    inBodyID2.IsInvalid() ? invalid_body_id : inBodyID2.GetIndexAndSequenceNumber();
                const auto pos = vec3{
                    .x = inContactPosition.GetX(),
                    .y = inContactPosition.GetY(),
                    .z = inContactPosition.GetZ(),
                };
                const auto norm = vec3{
                    .x = inContactNormal.GetX(),
                    .y = inContactNormal.GetY(),
                    .z = inContactNormal.GetZ(),
                };
                const auto vel = vec3{
                    .x = inContactVelocity.GetX(),
                    .y = inContactVelocity.GetY(),
                    .z = inContactVelocity.GetZ(),
                };
                auto new_velocity = vec3{
                    .x = ioNewCharacterVelocity.GetX(),
                    .y = ioNewCharacterVelocity.GetY(),
                    .z = ioNewCharacterVelocity.GetZ(),
                };
                _callbacks.on_contact_solve(this, target_body, pos, norm, vel, &new_velocity, _callbacks.user_data);
                ioNewCharacterVelocity = JPH::Vec3(new_velocity.x, new_velocity.y, new_velocity.z);
            }
        }

        JPH::Ref<JPH::CharacterVirtual> _character;
        JPH::PhysicsSystem* _physics_system = nullptr;
        JPH::TempAllocator* _temp_allocator = nullptr;
        float _max_slope_angle = default_character_max_slope_angle;
        character_contact_callbacks _callbacks = {};
    };

    class physics_system_impl final : public physics_system
    {
      public:
        explicit physics_system_impl(const init_desc& description)
            : _warn(description.warn), _error(description.error), _log_user_data(description.log_user_data),
              _collision_steps(description.collision_steps)
        {
            _temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(description.temp_allocator_size_bytes);
            _job_system = std::make_unique<tempest_jolt_job_system>(tempest_jolt_job_system::create_desc{
                .max_jobs = JPH::cMaxPhysicsJobs,
                .max_barriers = JPH::cMaxPhysicsBarriers,
                .dispatch_callback = description.job_dispatch,
                .dispatcher_user_data = description.job_dispatcher_user_data,
                .max_concurrency = description.max_concurrency,
            });

            _physics_system.Init(description.max_bodies, default_body_mutex_count, description.max_body_pairs,
                                description.max_contact_constraints, _bp_layer_interface, _obj_vs_bp_filter,
                                _obj_pair_filter);
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
        [[nodiscard]] auto get_body_angular_velocity(body_id target_body_id) const -> vec3 override;
        auto set_body_angular_velocity(body_id target_body_id, vec3 velocity) -> void override;

        // Queries & Stepping
        [[nodiscard]] auto cast_ray(const raycast_query& query) -> raycast_hit override;
        auto step(float delta_time) -> void override;

        // Characters
        [[nodiscard]] auto create_character_virtual(const character_virtual_desc& description)
            -> character_virtual* override;
        auto destroy_character_virtual(character_virtual* character) -> void override;

      private:
        bp_layer_interface_impl _bp_layer_interface;
        object_vs_broad_phase_layer_filter_impl _obj_vs_bp_filter;
        object_layer_pair_filter_impl _obj_pair_filter;

        std::unique_ptr<JPH::TempAllocator> _temp_allocator;
        std::unique_ptr<tempest_jolt_job_system> _job_system;
        JPH::PhysicsSystem _physics_system;

        log_fn _warn = nullptr;
        log_fn _error = nullptr;
        void* _log_user_data = nullptr;
        uint32_t _collision_steps = default_collision_steps;
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
        if (_error != nullptr)
        {
            _error(result.GetError().c_str(), _log_user_data);
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
        if (_error != nullptr)
        {
            _error(result.GetError().c_str(), _log_user_data);
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
        if (_error != nullptr)
        {
            _error(result.GetError().c_str(), _log_user_data);
        }
        return nullptr;
    }

    auto physics_system_impl::create_heightfield_shape(const float* heights, uint32_t sample_count, vec3 offset,
                                                       vec3 scale) -> shape_handle
    {
        if (heights == nullptr || sample_count < min_heightfield_sample_count)
        {
            if (_error != nullptr)
            {
                _error("Invalid heightfield parameters", _log_user_data);
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
        if (_error != nullptr)
        {
            _error(result.GetError().c_str(), _log_user_data);
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

        auto body_settings = JPH::BodyCreationSettings(
            reinterpret_cast<const JPH::Shape*>(description.shape),
            JPH::RVec3(description.position.x, description.position.y, description.position.z),
            JPH::Quat(description.rotation.x, description.rotation.y, description.rotation.z, description.rotation.w),
            motion, static_cast<JPH::ObjectLayer>(description.layer));

        if (description.mass > 0.0F)
        {
            body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            body_settings.mMassPropertiesOverride.mMass = description.mass;
        }

        auto* const created_body = _physics_system.GetBodyInterface().CreateBody(body_settings);
        if (created_body == nullptr)
        {
            if (_error != nullptr)
            {
                _error("Failed to create Jolt body", _log_user_data);
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
        _physics_system.GetBodyInterface().DestroyBody(JPH::BodyID(target_body_id));
    }

    auto physics_system_impl::add_body(body_id target_body_id, bool activate) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        _physics_system.GetBodyInterface().AddBody(
            JPH::BodyID(target_body_id), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    auto physics_system_impl::remove_body(body_id target_body_id) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        _physics_system.GetBodyInterface().RemoveBody(JPH::BodyID(target_body_id));
    }

    auto physics_system_impl::get_body_position(body_id target_body_id) const -> vec3
    {
        if (target_body_id == invalid_body_id)
        {
            return {};
        }
        const auto body_position = _physics_system.GetBodyInterface().GetPosition(JPH::BodyID(target_body_id));
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
        _physics_system.GetBodyInterface().SetPosition(
            JPH::BodyID(target_body_id), JPH::RVec3(position.x, position.y, position.z), JPH::EActivation::Activate);
    }

    auto physics_system_impl::get_body_rotation(body_id target_body_id) const -> quat
    {
        if (target_body_id == invalid_body_id)
        {
            return {};
        }
        const auto body_rotation = _physics_system.GetBodyInterface().GetRotation(JPH::BodyID(target_body_id));
        return quat{body_rotation.GetX(), body_rotation.GetY(), body_rotation.GetZ(), body_rotation.GetW()};
    }

    auto physics_system_impl::set_body_rotation(body_id target_body_id, quat rotation) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        _physics_system.GetBodyInterface().SetRotation(JPH::BodyID(target_body_id),
                                                      JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
                                                      JPH::EActivation::Activate);
    }

    auto physics_system_impl::get_body_linear_velocity(body_id target_body_id) const -> vec3
    {
        if (target_body_id == invalid_body_id)
        {
            return {};
        }
        const auto body_velocity = _physics_system.GetBodyInterface().GetLinearVelocity(JPH::BodyID(target_body_id));
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
        _physics_system.GetBodyInterface().SetLinearVelocity(JPH::BodyID(target_body_id),
                                                            JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    auto physics_system_impl::get_body_angular_velocity(body_id target_body_id) const -> vec3
    {
        if (target_body_id == invalid_body_id)
        {
            return vec3{};
        }
        const auto ang_vel = _physics_system.GetBodyInterface().GetAngularVelocity(JPH::BodyID(target_body_id));
        return vec3{
            .x = ang_vel.GetX(),
            .y = ang_vel.GetY(),
            .z = ang_vel.GetZ(),
        };
    }

    auto physics_system_impl::set_body_angular_velocity(body_id target_body_id, vec3 velocity) -> void
    {
        if (target_body_id == invalid_body_id)
        {
            return;
        }
        _physics_system.GetBodyInterface().SetAngularVelocity(JPH::BodyID(target_body_id),
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
        if (_physics_system.GetNarrowPhaseQuery().CastRay(ray, hit_result))
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

            const auto lock = JPH::BodyLockRead(_physics_system.GetBodyLockInterface(), hit_result.mBodyID);
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
        _physics_system.Update(delta_time, static_cast<int>(_collision_steps), _temp_allocator.get(), _job_system.get());
    }

    auto physics_system_impl::create_character_virtual(const character_virtual_desc& description) -> character_virtual*
    {
        if (description.shape == nullptr)
        {
            if (_error != nullptr)
            {
                _error("Shape handle cannot be null when creating character virtual", _log_user_data);
            }
            return nullptr;
        }

        auto settings = JPH::CharacterVirtualSettings();
        settings.mUp = JPH::Vec3(description.up.x, description.up.y, description.up.z);
        settings.mMaxSlopeAngle = description.max_slope_angle;
        settings.mEnhancedInternalEdgeRemoval = description.enhanced_internal_edge_removal;
        settings.mShape = reinterpret_cast<const JPH::Shape*>(description.shape);
        settings.mMass = description.mass;
        settings.mMaxStrength = description.max_strength;
        settings.mShapeOffset =
            JPH::Vec3(description.shape_offset.x, description.shape_offset.y, description.shape_offset.z);
        settings.mCharacterPadding = description.character_padding;
        settings.mPenetrationRecoverySpeed = description.penetration_recovery_speed;
        settings.mPredictiveContactDistance = description.predictive_contact_distance;
        settings.mMaxCollisionIterations = description.max_collision_iterations;
        settings.mMaxConstraintIterations = description.max_constraint_iterations;
        settings.mCollisionTolerance = description.collision_tolerance;

        if (description.inner_body_shape != nullptr)
        {
            settings.mInnerBodyShape = reinterpret_cast<const JPH::Shape*>(description.inner_body_shape);
            settings.mInnerBodyLayer = static_cast<JPH::ObjectLayer>(description.inner_body_layer);
        }

        auto jolt_character = JPH::Ref<JPH::CharacterVirtual>(new JPH::CharacterVirtual(
            &settings, JPH::RVec3(description.position.x, description.position.y, description.position.z),
            JPH::Quat(description.rotation.x, description.rotation.y, description.rotation.z, description.rotation.w),
            &_physics_system));

        // NOLINTNEXTLINE
        return new character_virtual_impl(std::move(jolt_character), &_physics_system, _temp_allocator.get(),
                                          description.max_slope_angle, description.callbacks);
    }

    auto physics_system_impl::destroy_character_virtual(character_virtual* character) -> void
    {
        delete character; // NOLINT
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
