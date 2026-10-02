#ifndef TEMPEST_PHYSICS_SHIM_JOLT_SHIM_HPP
#define TEMPEST_PHYSICS_SHIM_JOLT_SHIM_HPP

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
    #ifdef JOLT_SHIM_EXPORTS
        #define JOLT_SHIM_API __declspec(dllexport)
    #else
        #define JOLT_SHIM_API __declspec(dllimport)
    #endif
#else
    #define JOLT_SHIM_API __attribute__((visibility("default")))
#endif

namespace jolt::shim
{
    struct shape_t;
    using shape_handle = shape_t*;

    using body_id = uint32_t;
    inline constexpr body_id invalid_body_id = 0xFFFFFFFF;

    inline constexpr uint32_t default_max_bodies = 1024;
    inline constexpr uint32_t default_max_body_pairs = 1024;
    inline constexpr uint32_t default_max_contact_constraints = 1024;
    inline constexpr size_t default_temp_allocator_size = static_cast<size_t>(10) * 1024 * 1024;
    inline constexpr uint32_t default_collision_steps = 1;
    inline constexpr uint32_t default_max_concurrency = 16;
    inline constexpr float default_raycast_distance = 1000.0F;

    struct vec3
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
    };

    struct quat
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;
    };

    enum class motion_type : uint8_t
    {
        static_motion = 0,
        kinematic = 1,
        dynamic = 2,
    };

    enum class object_layer : uint8_t
    {
        non_moving = 0,
        moving = 1,
    };

    enum class ground_state : uint8_t
    {
        on_ground = 0,
        on_steep_ground = 1,
        not_supported = 2,
        in_air = 3,
    };

    struct raycast_query
    {
        vec3 origin;
        vec3 direction;
        float max_distance = default_raycast_distance;
    };

    struct raycast_hit
    {
        vec3 position{};
        vec3 normal{};
        float distance = 0.0F;
        body_id hit_body_id = invalid_body_id;
        bool has_hit = false;
    };

    struct body_desc
    {
        shape_handle shape = nullptr;
        vec3 position{};
        quat rotation{};
        motion_type motion = motion_type::static_motion;
        object_layer layer = object_layer::non_moving;
        float mass = 0.0F;
    };

    using job_execute_fn = void (*)(void* job_context);
    using job_dispatch_fn = void (*)(job_execute_fn func, void* job_context, void* dispatcher_user_data);

    using log_fn = void (*)(const char* message, void* user_data);

    struct init_desc
    {
        uint32_t max_bodies = default_max_bodies;
        uint32_t max_body_pairs = default_max_body_pairs;
        uint32_t max_contact_constraints = default_max_contact_constraints;
        size_t temp_allocator_size_bytes = default_temp_allocator_size;
        uint32_t collision_steps = default_collision_steps;
        uint32_t max_concurrency = default_max_concurrency;

        job_dispatch_fn job_dispatch = nullptr;
        void* job_dispatcher_user_data = nullptr;

        log_fn warn = nullptr;
        log_fn error = nullptr;
        void* log_user_data = nullptr;
    };

    // Character Controller Defaults (matching JPH::CharacterVirtualSettings & JPH::CharacterBaseSettings)

    /// \brief Default world-space up direction vector (0, 1, 0). Matches JPH::CharacterBaseSettings::mUp.
    inline constexpr vec3 default_character_up{.x=0.0F, .y=1.0F, .z=0.0F};

    /// \brief Default character mass in kilograms (80 kg). Kinematic characters use this mass to exert
    /// downward force on dynamic bodies they stand on or push against. Sourced from Tempest character specification.
    inline constexpr float default_character_mass = 80.0F;

    /// \brief Default maximum force in Newtons (100 N) that a character can exert when pushing dynamic rigid bodies.
    /// Matches JPH::CharacterVirtualSettings::mMaxStrength.
    inline constexpr float default_character_max_strength = 100.0F;

    /// \brief Default maximum slope angle in radians (~0.872665 rad = 50 degrees) that the character can climb.
    /// Normal angles exceeding this threshold transition ground state to on_steep_ground. Matches JPH::CharacterBaseSettings::mMaxSlopeAngle.
    inline constexpr float default_character_max_slope_angle = 0.872665F;

    /// \brief Default local-space shape offset vector (0, 0, 0). Matches JPH::CharacterVirtualSettings::mShapeOffset.
    inline constexpr vec3 default_character_shape_offset{.x=0.0F, .y=0.0F, .z=0.0F};

    /// \brief Artificial padding margin in meters (0.02 m = 2 cm) around character shape to avoid snagging on vertices/edges during sweeps.
    /// Matches JPH::CharacterVirtualSettings::mCharacterPadding.
    inline constexpr float default_character_padding = 0.02F;

    /// \brief Fraction of penetration resolved per simulation tick (1.0 = 100% instant resolution).
    /// Matches JPH::CharacterVirtualSettings::mPenetrationRecoverySpeed.
    inline constexpr float default_character_penetration_recovery_speed = 1.0F;

    /// \brief Lookahead distance in meters (0.1 m = 10 cm) outside hull to detect predictive contacts for sliding plane calculation.
    /// Matches JPH::CharacterVirtualSettings::mPredictiveContactDistance.
    inline constexpr float default_character_predictive_contact_distance = 0.1F;

    /// \brief Flag indicating whether extra edge removal filtering is applied to prevent catching on coplanar triangle mesh seams (false).
    /// Matches JPH::CharacterBaseSettings::mEnhancedInternalEdgeRemoval.
    inline constexpr bool default_character_enhanced_internal_edge_removal = false;

    /// \brief Maximum collision iterations per update step to resolve corner wedging and multi-plane deflections (5).
    /// Matches JPH::CharacterVirtualSettings::mMaxCollisionIterations.
    inline constexpr uint32_t default_character_max_collision_iterations = 5;

    /// \brief Maximum constraint solver iterations when sliding against multiple contacting surfaces (15).
    /// Matches JPH::CharacterVirtualSettings::mMaxConstraintIterations.
    inline constexpr uint32_t default_character_max_constraint_iterations = 15;

    /// \brief Minimum distance penetration threshold in meters (0.001 m = 1 mm) before solver applies position correction.
    /// Matches JPH::CharacterVirtualSettings::mCollisionTolerance.
    inline constexpr float default_character_collision_tolerance = 1.0e-3F;

    /// \brief Object layer assigned to the character's optional inner rigid body (moving).
    inline constexpr object_layer default_character_inner_body_layer = object_layer::moving;

    // Extended Update & Stair Stepping Defaults (matching JPH::CharacterVirtual::ExtendedUpdateSettings)

    /// \brief Maximum downward probe displacement in meters (0.5 m down) to snap the character to the floor when walking down slopes or ledges.
    /// Matches JPH::CharacterVirtual::ExtendedUpdateSettings::mStickToFloorStepDown.
    inline constexpr vec3 default_stick_to_floor_step_down{0.0F, -0.5F, 0.0F};

    /// \brief Maximum vertical step height displacement in meters (0.4 m up) for automatic stair navigation.
    /// Matches JPH::CharacterVirtual::ExtendedUpdateSettings::mWalkStairsStepUp.
    inline constexpr vec3 default_walk_stairs_step_up{0.0F, 0.4F, 0.0F};

    /// \brief Minimum horizontal forward distance in meters (0.02 m) required after stepping up to validate stair landing.
    /// Matches JPH::CharacterVirtual::ExtendedUpdateSettings::mWalkStairsMinStepForward.
    inline constexpr float default_walk_stairs_min_step_forward = 0.02F;

    /// \brief Lookahead test distance in meters (0.15 m) to probe stair landing and avoid catching on riser edges at small delta times.
    /// Matches JPH::CharacterVirtual::ExtendedUpdateSettings::mWalkStairsStepForwardTest.
    inline constexpr float default_walk_stairs_step_forward_test = 0.15F;

    /// \brief Cosine of the maximum contact normal angle in horizontal plane (cos(75 deg) ~= 0.258819) where forward contact adjustment applies.
    /// Matches JPH::CharacterVirtual::ExtendedUpdateSettings::mWalkStairsCosAngleForwardContact.
    inline constexpr float default_walk_stairs_cos_angle_forward_contact = 0.258819F;

    /// \brief Extra downward translation in meters added when stepping down at the end of a stair step. Defaults to zero.
    /// Matches JPH::CharacterVirtual::ExtendedUpdateSettings::mWalkStairsStepDownExtra.
    inline constexpr vec3 default_walk_stairs_step_down_extra{0.0F, 0.0F, 0.0F};

    struct extended_update_settings
    {
        vec3 stick_to_floor_step_down = default_stick_to_floor_step_down;
        vec3 walk_stairs_step_up = default_walk_stairs_step_up;
        float walk_stairs_min_step_forward = default_walk_stairs_min_step_forward;
        float walk_stairs_step_forward_test = default_walk_stairs_step_forward_test;
        float walk_stairs_cos_angle_forward_contact = default_walk_stairs_cos_angle_forward_contact;
        vec3 walk_stairs_step_down_extra = default_walk_stairs_step_down_extra;
    };

    class character_virtual;

    struct character_contact_callbacks
    {
        void (*on_contact_added)(character_virtual* character, body_id body, vec3 position, vec3 normal, void* user_data) = nullptr;
        void (*on_contact_solve)(character_virtual* character, body_id body, vec3 position, vec3 normal, vec3 velocity, vec3* io_new_velocity, void* user_data) = nullptr;
        void* user_data = nullptr;
    };

    struct character_virtual_desc
    {
        shape_handle shape = nullptr;
        vec3 position{0.0F, 0.0F, 0.0F};
        quat rotation{0.0F, 0.0F, 0.0F, 1.0F};
        vec3 up = default_character_up;
        float mass = default_character_mass;
        float max_strength = default_character_max_strength;
        float max_slope_angle = default_character_max_slope_angle;
        vec3 shape_offset = default_character_shape_offset;
        float character_padding = default_character_padding;
        float penetration_recovery_speed = default_character_penetration_recovery_speed;
        float predictive_contact_distance = default_character_predictive_contact_distance;
        bool enhanced_internal_edge_removal = default_character_enhanced_internal_edge_removal;
        uint32_t max_collision_iterations = default_character_max_collision_iterations;
        uint32_t max_constraint_iterations = default_character_max_constraint_iterations;
        float collision_tolerance = default_character_collision_tolerance;
        shape_handle inner_body_shape = nullptr;
        object_layer inner_body_layer = default_character_inner_body_layer;
        character_contact_callbacks callbacks{};
    };

    class JOLT_SHIM_API character_virtual
    {
    public:
        character_virtual() = default;
        character_virtual(const character_virtual&) = delete;
        character_virtual(character_virtual&&) noexcept = delete;
        virtual ~character_virtual() = default;

        auto operator=(const character_virtual&) -> character_virtual& = delete;
        auto operator=(character_virtual&&) noexcept -> character_virtual& = delete;

        virtual void update(float delta_time, vec3 gravity) = 0;
        virtual void extended_update(float delta_time, vec3 gravity, const extended_update_settings& settings) = 0;
        [[nodiscard]] virtual auto walk_stairs(float delta_time, vec3 step_up, vec3 step_forward, vec3 step_forward_test, vec3 step_down_extra) -> bool = 0;
        [[nodiscard]] virtual auto stick_to_floor(vec3 step_down) -> bool = 0;
        [[nodiscard]] virtual auto can_walk_stairs(vec3 linear_velocity) const -> bool = 0;
        [[nodiscard]] virtual auto cancel_velocity_towards_steep_slopes(vec3 desired_velocity) const -> vec3 = 0;

        [[nodiscard]] virtual auto get_position() const -> vec3 = 0;
        virtual void set_position(vec3 position) = 0;
        [[nodiscard]] virtual auto get_rotation() const -> quat = 0;
        virtual void set_rotation(quat rotation) = 0;
        [[nodiscard]] virtual auto get_linear_velocity() const -> vec3 = 0;
        virtual void set_linear_velocity(vec3 velocity) = 0;

        [[nodiscard]] virtual auto get_ground_state() const -> ground_state = 0;
        [[nodiscard]] virtual auto is_supported() const -> bool = 0;
        [[nodiscard]] virtual auto get_ground_position() const -> vec3 = 0;
        [[nodiscard]] virtual auto get_ground_normal() const -> vec3 = 0;
        [[nodiscard]] virtual auto get_ground_velocity() const -> vec3 = 0;
        [[nodiscard]] virtual auto get_ground_body_id() const -> body_id = 0;

        [[nodiscard]] virtual auto get_mass() const -> float = 0;
        virtual void set_mass(float mass) = 0;
        [[nodiscard]] virtual auto get_max_slope_angle() const -> float = 0;
        virtual void set_max_slope_angle(float max_slope_angle) = 0;
    };

    class JOLT_SHIM_API physics_system
    {
    public:
        physics_system() = default;
        physics_system(const physics_system&) = delete;
        physics_system(physics_system&&) noexcept = delete;
        virtual ~physics_system() = default;

        auto operator=(const physics_system&) -> physics_system& = delete;
        auto operator=(physics_system&&) noexcept -> physics_system& = delete;

        // Shapes
        /// \brief Creates a box collision shape.
        /// \param half_extents Half dimensions of the box along each axis.
        /// \return Handle to the created shape.
        /// \note The returned shape handle MUST be manually managed and destroyed via destroy_shape().
        [[nodiscard]] virtual auto create_box_shape(vec3 half_extents) -> shape_handle = 0;

        /// \brief Creates a sphere collision shape.
        /// \param radius Radius of the sphere.
        /// \return Handle to the created shape.
        /// \note The returned shape handle MUST be manually managed and destroyed via destroy_shape().
        [[nodiscard]] virtual auto create_sphere_shape(float radius) -> shape_handle = 0;

        /// \brief Creates a capsule collision shape.
        /// \param half_height Half height of the cylindrical section.
        /// \param radius Radius of the hemispherical caps.
        /// \return Handle to the created shape.
        /// \note The returned shape handle MUST be manually managed and destroyed via destroy_shape().
        [[nodiscard]] virtual auto create_capsule_shape(float half_height, float radius) -> shape_handle = 0;

        /// \brief Creates a heightfield collision shape from an array of height values.
        /// \param heights Pointer to the contiguous array of height samples.
        /// \param sample_count Dimension of the square sample grid (sample_count x sample_count).
        /// \param offset World space position offset.
        /// \param scale Spatial scaling vector applied to the samples.
        /// \return Handle to the created shape.
        /// \note The returned shape handle MUST be manually managed and destroyed via destroy_shape().
        [[nodiscard]] virtual auto create_heightfield_shape(const float* heights, uint32_t sample_count, vec3 offset, vec3 scale) -> shape_handle = 0;

        /// \brief Destroys a previously created shape handle.
        /// \param shape The shape handle to release.
        virtual void destroy_shape(shape_handle shape) = 0;

        // Bodies
        /// \brief Creates a rigid body within the physics system.
        /// \param description Construction properties and initial state of the body.
        /// \return Unique 32-bit identifier of the created body.
        /// \note The returned body ID MUST be manually managed and destroyed via destroy_body().
        [[nodiscard]] virtual auto create_body(const body_desc& description) -> body_id = 0;

        /// \brief Destroys a rigid body and releases its allocated slot.
        /// \param target_body_id Identifier of the body to destroy.
        virtual void destroy_body(body_id target_body_id) = 0;

        /// \brief Adds a body to the active physics simulation world.
        /// \param target_body_id Identifier of the body to add.
        /// \param activate Whether to activate the body immediately upon insertion.
        virtual void add_body(body_id target_body_id, bool activate = true) = 0;

        /// \brief Removes a body from the active physics simulation world without destroying it.
        /// \param target_body_id Identifier of the body to remove.
        virtual void remove_body(body_id target_body_id) = 0;

        /// \brief Queries the current world position of a body.
        /// \param target_body_id Identifier of the body.
        /// \return Current world position.
        [[nodiscard]] virtual auto get_body_position(body_id target_body_id) const -> vec3 = 0;

        /// \brief Sets the world position of a body.
        /// \param target_body_id Identifier of the body.
        /// \param position New world position.
        virtual void set_body_position(body_id target_body_id, vec3 position) = 0;

        /// \brief Queries the current world orientation quaternion of a body.
        /// \param target_body_id Identifier of the body.
        /// \return Current world rotation quaternion.
        [[nodiscard]] virtual auto get_body_rotation(body_id target_body_id) const -> quat = 0;

        /// \brief Sets the world orientation quaternion of a body.
        /// \param target_body_id Identifier of the body.
        /// \param rotation New world rotation quaternion.
        virtual void set_body_rotation(body_id target_body_id, quat rotation) = 0;

        /// \brief Queries the linear velocity vector of a body.
        /// \param target_body_id Identifier of the body.
        /// \return Current linear velocity in meters per second.
        [[nodiscard]] virtual auto get_body_linear_velocity(body_id target_body_id) const -> vec3 = 0;

        /// \brief Sets the linear velocity vector of a body.
        /// \param target_body_id Identifier of the body.
        /// \param velocity New linear velocity in meters per second.
        virtual void set_body_linear_velocity(body_id target_body_id, vec3 velocity) = 0;

        /// \brief Queries the angular velocity vector of a body.
        /// \param target_body_id Identifier of the body.
        /// \return Current angular velocity in radians per second.
        [[nodiscard]] virtual auto get_body_angular_velocity(body_id target_body_id) const -> vec3 = 0;

        /// \brief Sets the angular velocity vector of a body.
        /// \param target_body_id Identifier of the body.
        /// \param velocity New angular velocity in radians per second.
        virtual void set_body_angular_velocity(body_id target_body_id, vec3 velocity) = 0;

        // Queries & Stepping
        /// \brief Casts a ray into the physics world and retrieves the closest intersection.
        /// \param query Ray definition including origin, direction, and maximum distance.
        /// \return Hit result containing hit status, position, normal, distance, and body ID.
        [[nodiscard]] virtual auto cast_ray(const raycast_query& query) -> raycast_hit = 0;

        /// \brief Advances the physics simulation by the specified delta time.
        /// \param delta_time Simulation time step in seconds.
        virtual void step(float delta_time) = 0;

        // Characters
        /// \brief Creates a virtual character for kinematic character movement.
        /// \param description Construction settings for the character.
        /// \return Pointer to the created character virtual interface, or nullptr on failure.
        /// \note The returned character MUST be destroyed via destroy_character_virtual().
        [[nodiscard]] virtual auto create_character_virtual(const character_virtual_desc& description) -> character_virtual* = 0;

        /// \brief Destroys a virtual character previously created by create_character_virtual().
        /// \param character Pointer to the character virtual instance to destroy.
        virtual void destroy_character_virtual(character_virtual* character) = 0;
    };

    /// \brief Creates an instance of the Jolt physics system.
    /// \param description Initialization parameters including limits and job dispatch callbacks.
    /// \return Raw pointer to the allocated physics system instance.
    /// \note The returned pointer MUST be manually managed by the caller, either by calling
    ///       destroy_physics_system() or by immediately depositing it into a unique_ptr.
    [[nodiscard]] JOLT_SHIM_API auto create_physics_system(const init_desc& description) -> physics_system*;

    /// \brief Destroys an instance of the Jolt physics system previously created by create_physics_system().
    /// \param system The physics system instance to destroy and deallocate.
    JOLT_SHIM_API void destroy_physics_system(physics_system* system);
} // namespace jolt::shim

#endif // TEMPEST_PHYSICS_SHIM_JOLT_SHIM_HPP
