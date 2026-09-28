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

        // Queries & Stepping
        /// \brief Casts a ray into the physics world and retrieves the closest intersection.
        /// \param query Ray definition including origin, direction, and maximum distance.
        /// \return Hit result containing hit status, position, normal, distance, and body ID.
        [[nodiscard]] virtual auto cast_ray(const raycast_query& query) -> raycast_hit = 0;

        /// \brief Advances the physics simulation by the specified delta time.
        /// \param delta_time Simulation time step in seconds.
        virtual void step(float delta_time) = 0;
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
