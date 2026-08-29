#ifndef tempest_math_transformations_hpp__
#define tempest_math_transformations_hpp__

#include <tempest/mat3.hpp>
#include <tempest/mat4.hpp>
#include <tempest/quat.hpp>
#include <tempest/vec2.hpp>
#include <tempest/vec3.hpp>
#include <tempest/vec4.hpp>

#include <tempest/int.hpp>
#include <tempest/type_traits.hpp>

#undef near
#undef far

namespace tempest::math
{
    template <typename T>
    constexpr vec3<T> front = vec3<T>(static_cast<T>(0), static_cast<T>(0), static_cast<T>(1));

    template <typename T>
    constexpr vec3<T> up = vec3<T>(static_cast<T>(0), static_cast<T>(1), static_cast<T>(0));

    template <typename T>
    constexpr vec3<T> right = vec3<T>(static_cast<T>(1), static_cast<T>(0), static_cast<T>(0));

    template <typename T>
    inline constexpr mat3<T> as_mat3(const quat<T>& q)
    {
        const T n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        const T s = n > 0 ? 2 / n : 0;
        const T x = s * q.x;
        const T y = s * q.y;
        const T z = s * q.z;
        const T xx = x * q.x;
        const T xy = x * q.y;
        const T xz = x * q.z;
        const T xw = x * q.w;
        const T yy = y * q.y;
        const T yz = y * q.z;
        const T yw = y * q.w;
        const T zz = z * q.z;
        const T zw = z * q.w;

        mat3<T> res;

        res[0] = vec3<T>{static_cast<T>(1) - yy - zz, xy + zw, xz - yw};
        res[1] = vec3<T>{xy - zw, static_cast<T>(1) - xx - zz, yz + xw};
        res[2] = vec3<T>{xz + yw, yz - xw, static_cast<T>(1) - xx - yy};

        return res;
    }

    template <typename T>
    inline constexpr quat<T> as_quat(const mat3<T>& m)
    {
        quat<T> quat(static_cast<T>(0));

        // Compute the trace to see if it is positive or not.
        const T trace = m[0][0] + m[1][1] + m[2][2];

        // check the sign of the trace
        if (trace > 0)
        {
            // trace is positive
            T s = math::sqrt(trace + 1);
            quat.w = T(0.5) * s;
            s = T(0.5) / s;
            quat.x = (m[1][2] - m[2][1]) * s;
            quat.y = (m[2][0] - m[0][2]) * s;
            quat.z = (m[0][1] - m[1][0]) * s;
        }
        else
        {
            // trace is negative

            // Find the index of the greatest diagonal
            size_t i = 0;
            if (m[1][1] > m[0][0])
            {
                i = 1;
            }
            if (m[2][2] > m[i][i])
            {
                i = 2;
            }

            // Get the next indices: (n+1)%3
            constexpr size_t next_ijk[3] = {1, 2, 0};
            size_t j = next_ijk[i];
            size_t k = next_ijk[j];
            T s = math::sqrt((m[i][i] - (m[j][j] + m[k][k])) + 1);
            quat[i] = T(0.5) * s;
            if (s != 0)
            {
                s = T(0.5) / s;
            }
            quat.w = (m[j][k] - m[k][j]) * s;
            quat[j] = (m[i][j] + m[j][i]) * s;
            quat[k] = (m[i][k] + m[k][i]) * s;
        }
        return quat;
    }

    template <typename T>
    inline constexpr mat4<T> as_mat4(const quat<T>& q)
    {
        mat4 res(static_cast<T>(1));

        const T x = q.x, y = q.y, z = q.z, w = q.w;
        const T x2 = x + x, y2 = y + y, z2 = z + z;
        const T xx = x * x2, xy = x * y2, xz = x * z2;
        const T yy = y * y2, yz = y * z2, zz = z * z2;
        const T wx = w * x2, wy = w * y2, wz = w * z2;

        auto data = res.data;

        data[0] = (1 - (yy + zz));
        data[1] = (xy + wz);
        data[2] = (xz - wy);
        data[3] = 0;
        data[4] = (xy - wz);
        data[5] = (1 - (xx + zz));
        data[6] = (yz + wx);
        data[7] = 0;
        data[8] = (xz + wy);
        data[9] = (yz - wx);
        data[10] = (1 - (xx + yy));
        data[11] = 0;
        data[15] = 1;

        return res;
    }

    template <typename T>
    inline constexpr mat4<T> translate(const mat4<T>& m, const vec3<T>& v)
    {
        mat4 res(m);
        res[3] = v[0] * m[0] + v[1] * m[1] + v[2] * m[2] + static_cast<T>(1) * m[3];
        return res;
    }

    template <typename T>
    inline constexpr mat4<T> translate(const vec3<T>& v)
    {
        return translate(mat4(static_cast<T>(1)), v);
    }

    template <typename T>
    inline constexpr mat4<T> rotate(const mat4<T>& m, const T& angle, const vec3<T>& v)
    {
        const T a = angle;
        const T c = math::cos(a);
        const T s = math::sin(a);

        vec3<T> axis(normalize(v));
        vec3<T> temp((static_cast<T>(1) - c) * axis);

        mat4<T> rot(static_cast<T>(1));
        rot[0][0] = c + temp[0] * axis[0];
        rot[0][1] = temp[0] * axis[1] + s * axis[2];
        rot[0][2] = temp[0] * axis[2] - s * axis[1];

        rot[1][0] = temp[1] * axis[0] - s * axis[2];
        rot[1][1] = c + temp[1] * axis[1];
        rot[1][2] = temp[1] * axis[2] + s * axis[0];

        rot[2][0] = temp[2] * axis[0] + s * axis[1];
        rot[2][1] = temp[2] * axis[1] - s * axis[0];
        rot[2][2] = c + temp[2] * axis[2];

        mat4<T> res;
        res[0] = m[0] * rot[0][0] + m[1] * rot[0][1] + m[2] * rot[0][2];
        res[1] = m[0] * rot[1][0] + m[1] * rot[1][1] + m[2] * rot[1][2];
        res[2] = m[0] * rot[2][0] + m[1] * rot[2][1] + m[2] * rot[2][2];
        res[3] = m[3];

        return res;
    }

    template <typename T>
    inline constexpr mat4<T> rotate(const T& angle, const vec3<T>& v)
    {
        return rotate(mat4(static_cast<T>(1)), angle, v);
    }

    template <typename T>
    inline constexpr quat<T> rotate(const quat<T>& q, const T& angle, const vec3<T>& axis)
    {
        const vec3 normalizedAxis = normalize(axis);
        const T sine = math::sin(angle * static_cast<T>(0.5));

        return q * quat<T>(math::cos(angle * static_cast<T>(0.5)), normalizedAxis.x * sine, normalizedAxis.y * sine,
                           normalizedAxis.z * sine);
    }

    template <typename T>
    inline constexpr mat4<T> rotate(const vec3<T>& euler)
    {
        return as_mat4(quat(euler));
    }

    template <typename T>
    inline constexpr mat4<T> scale(const mat4<T>& m, const vec3<T>& v)
    {
        mat4<T> res;
        res[0] = v[0] * m[0];
        res[1] = v[1] * m[1];
        res[2] = v[2] * m[2];
        res[3] = m[3];
        return res;
    }

    template <typename T>
    inline constexpr mat4<T> scale(const vec3<T>& v)
    {
        return scale(mat4(static_cast<T>(1)), v);
    }

    template <typename T>
    inline constexpr mat4<T> transform(const vec3<T>& translation, const quat<T>& rotation, const vec3<T>& scale)
    {
        // transformation = translation * rotation * scale
        const auto translating = translate(translation);
        const auto rotating = as_mat4(rotation);
        const auto tr = translating * rotating;
        const auto scaling = tempest::math::scale(tr, scale);

        return scaling;
    }

    template <typename T>
    inline constexpr mat4<T> transform(const vec3<T>& translation, const vec3<T>& rotation, const vec3<T>& scale)
    {
        // transformation = translation * rotation * scale
        const auto translating = translate(translation);
        const auto scaling = tempest::math::scale(scale);
        const auto rotating = as_mat4(quat(rotation));
        return translating * rotating * scaling;
    }

    template <typename T>
    inline constexpr bool decompose(const mat4<T>& transformationMatrix, vec3<T>& translate, quat<T>& rotation,
                                    vec3<T>& scale)
    {
        auto local = transformationMatrix;

        // Matrix normalization
        if (local[3][3] == static_cast<T>(0))
        {
            return false;
        }

        for (size_t i = 0; i < 4; ++i)
        {
            for (size_t j = 0; j < 4; ++j)
            {
                local[i][j] /= local[3][3];
            }
        }

        // solve for translation and remove
        const vec4<T> translation = local[3];
        translate.x = translation.x;
        translate.y = translation.y;
        translate.z = translation.z;
        local[3] = vec4<T>(static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), translation.w);

        // solve for scale
        vec3<T> col[3];
        for (size_t i = 0; i < 3; ++i)
        {
            col[i] = vec3<T>{local[i][0], local[i][1], local[i][2]};
        }

        scale.x = norm(col[0]);
        scale.y = norm(col[1]);
        scale.z = norm(col[2]);

        if (scale.x == static_cast<T>(0) || scale.y == static_cast<T>(0) || scale.z == static_cast<T>(0))
        {
            rotation = quat<T>(static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1));
            return true;
        }

        // Check for negative determinant (reflection)
        if (dot(col[0], cross(col[1], col[2])) < static_cast<T>(0))
        {
            scale.x = -scale.x;
            col[0] = -col[0];
        }

        col[0] = col[0] / scale.x;
        col[1] = col[1] / scale.y;
        col[2] = col[2] / scale.z;

        const mat3<T> rot_mat(col[0], col[1], col[2]);
        rotation = as_quat(rot_mat);

        return true;
    }

    template <typename T>
    inline constexpr mat4<T> perspective(const T aspect, const T fov, T near)
    {
        const T fovy = fov;
        const T f = static_cast<T>(1) / math::tan(fovy / 2);
        return mat4<T>{f / aspect, 0, 0, 0, 0, -f, 0, 0, 0, 0, 0, -1, 0, 0, near, 0};
    }

    template <typename T>
    inline constexpr mat4<T> perspective(const T aspect, const T fov, T near, const T far)
    {
        T fov_rad = fov;
        T focal_length = static_cast<T>(1) / math::tan(fov_rad / 2);

        T x = focal_length / aspect;
        T y = -focal_length;
        T A = near / (far - near);
        T B = far * A;

        return transpose(mat4<T>{x, 0, 0, 0, 0, y, 0, 0, 0, 0, A, B, 0, 0, -1, 0});
    }

    template <typename T>
    inline constexpr mat4<T> ortho(const T left, const T right, const T bottom, const T top, const T near, const T far)
    {
        const auto sx = T(2) / (right - left);
        const auto sy = T(2) / (top - bottom);
        const auto sz = T(1) / (far - near);

        const auto tx = -(right + left) / (right - left);
        const auto ty = -(top + bottom) / (top - bottom);
        const auto tz = -near / (far - near);

        return mat4<T>{sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, tx, ty, tz, 1};
    }

    template <typename T>
    mat4<T> look_at(const vec3<T>& eye, const vec3<T>& target, const vec3<T>& up)
    {
        const auto dir = target - eye;
        const auto f = normalize(dir);
        const auto s = normalize(cross(up, f));
        const auto u = cross(f, s);

        mat4<T> result{
            vec4<T>{s.x, u.x, -f.x, 0},
            vec4<T>{s.y, u.y, -f.y, 0},
            vec4<T>{s.z, u.z, -f.z, 0},
            vec4<T>{-dot(eye, s), -dot(eye, u), dot(eye, f), 1},
        };

        return result;
    }

    template <typename T>
    inline constexpr mat3<T> tbn(const vec3<T>& tangent, const vec3<T>& bitangent, const vec3<T>& normal)
    {
        return mat3(tangent, bitangent, normal);
    }

    template <typename T>
    inline constexpr quat<T> encode_tbn(const mat3<T> tbn)
    {
        const mat3 tmp = {tbn[0], cross(tbn[2], tbn[0]), tbn[2]};
        quat q = normalize(as_quat(tmp));
        q = q.w < 0 ? -q : q;

        constexpr T bias = static_cast<T>(1) / static_cast<T>((1 << (sizeof(uint16_t) * 8 - 1)) - 1);
        if (q.w < bias)
        {
            q.w = bias;
            const T factor = static_cast<T>(math::sqrt(static_cast<T>(1) - bias * bias));
            q.x *= factor;
            q.y *= factor;
            q.z *= factor;
        }

        const vec3<T> binorm = cross(tbn[0], tbn[2]);
        const T direction = dot(binorm, tbn[1]);
        if (direction < 0)
        {
            q = -q;
        }

        return q;
    }

    template <typename T>
    inline constexpr vec3<T> extract_forward(const quat<T>& rotation)
    {
        return rotation * vec3<T>(0, 0, 1);
    }

    template <typename T>
    inline constexpr vec3<T> extract_up(const quat<T>& rotation)
    {
        return rotation * vec3<T>(0, 1, 0);
    }

    template <typename T>
    inline constexpr vec3<T> extract_right(const quat<T>& rotation)
    {
        return rotation * vec3<T>(1, 0, 0);
    }

    template <typename T>
    inline constexpr vec2<T> encode_direction_to_euler_angles(const vec3<T>& dir)
    {
        const auto d = normalize(dir);
        const T theta = math::atan(static_cast<T>(1) / d.z);
        const T phi = math::atan(d.y / d.x);
        return {as_degrees(theta), as_degrees(phi)};
    }
} // namespace tempest::math

#endif // tempest_math_transformations_hpp__
