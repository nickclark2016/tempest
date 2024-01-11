#ifndef tempest_math_transformations_hpp__
#define tempest_math_transformations_hpp__

#include "mat3.hpp"
#include "mat4.hpp"
#include "quat.hpp"
#include "vec2.hpp"
#include "vec3.hpp"
#include "vec4.hpp"

#include <cassert>

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
            T s = std::sqrt(trace + 1);
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
            T s = std::sqrt((m[i][i] - (m[j][j] + m[k][k])) + 1);
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
        const T c = cos(a);
        const T s = sin(a);

        vec3<T> axis(normalize(v));
        vec3<T> temp((static_cast<T>(1) - c) * axis);

        mat4<T> rot;
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
        const T sine = std::sin(angle * static_cast<T>(0.5));

        return q * quat<T>(cos(angle * static_cast<T>(0.5)), normalizedAxis.x * sine, normalizedAxis.y * sine,
                           normalizedAxis.z * sine);
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
        return scaling * rotating * translating;
    }

    template <typename T>
    inline constexpr bool decompose(const mat4<T>& transformationMatrix, vec3<T>& translate, quat<T> rotation,
                                    vec3<T>& scale)
    {
        auto local = transformationMatrix;

        // Matrix normalization
        if (local[3][3] == 0)
        {
            return false;
        }

        for (std::size_t i = 0; i < 4; ++i)
        {
            for (std::size_t j = 0; j < 4; ++j)
            {
                local[i][j] /= local[3][3];
            }
        }

        // solve for translation and remove
        vec4 translation = local[3];
        translate.x = translation.x;
        translate.y = translation.y;
        translate.z = translation.z;
        local[3] = vec4(static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), translation.w);

        // solve for scale
        vec3<float> row[3];
        for (std::size_t i = 0; i < 3; ++i)
        {
            for (std::size_t j = 0; j < 3; ++j)
            {
                row[i][j] = local[i][j];
            }
        }

        scale.x = norm(row[0]); // x scale is the length of the first row
        scale.y = norm(row[1]);
        scale.z = norm(row[2]);

        T root, trace = row[0].x + row[1].y + row[2].z;
        if (trace > static_cast<T>(0))
        {
            root = std::sqrt(trace + static_cast<T>(1));
            rotation.w = static_cast<T>(0.5) * root;
            root = static_cast<T>(0.5) / root;
            rotation.x = root * (row[1].z - row[2].y);
            rotation.y = root * (row[2].x - row[0].z);
            rotation.z = root * (row[0].y - row[1].x);
        }
        else
        {
            constexpr std::size_t next[3] = {1, 2, 0};
            std::size_t i = 0;
            if (row[1].y > row[0].x)
                i = 1;
            if (row[2].z > row[i][i])
                i = 2;
            std::size_t j = next[i];
            std::size_t k = next[j];

            root = std::sqrt(row[i][i] - row[j][j] - row[k][k] + static_cast<T>(1));
            rotation[i + 1] = static_cast<T>(0.5) * root;
            root = static_cast<T>(0.5) / root;
            rotation[j + 1] = root * (row[i][j] + row[j][i]);
            rotation[k + 1] = root * (row[i][k] + row[k][i]);
            rotation.w = root * (row[j][k] - row[k][j]);
        }

        return true;
    }

    template <typename T>
    inline constexpr mat4<T> perspective(const T aspect, const T fov, T near)
    {
        const T f = static_cast<T>(1) / std::tan(as_radians(fov / 2));
        return mat4<T>{f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, 0, -1, 0, 0, near, 0};
    }

    template <typename T>
    inline constexpr mat4<T> perspective(const T aspect, const T fov, T near, const T far)
    {
        T fov_rad = as_radians(fov);
        T focal_length = static_cast<T>(1) / std::tan(fov_rad / 2);

        T x = focal_length / aspect;
        T y = focal_length;
        T A = near / (far - near);
        T B = far * A;

        return transpose(mat4<T>{
            x, 0, 0, 0,
            0, y, 0, 0,
            0, 0, A, B,
            0, 0, -1, 0
        });
    }

    template <typename T>
    mat4<T> look_at(const vec3<T>& eye, const vec3<T>& target, const vec3<T>& up)
    {
        const auto dir = target - eye;
        const auto f = normalize(dir);
        const auto s = normalize(cross(f, up));
        const auto u = cross(s, f);

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

        constexpr T bias = static_cast<T>(1) / static_cast<T>((1 << (sizeof(std::uint16_t) * 8 - 1)) - 1);
        if (q.w < bias)
        {
            q.w = bias;
            const T factor = static_cast<T>(std::sqrt(static_cast<T>(1) - bias * bias));
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
        const T x = static_cast<T>(2) * (rotation.x * rotation.z + rotation.w * rotation.y);
        const T y = static_cast<T>(2) * (rotation.y * rotation.z - rotation.w * rotation.x);
        const T z = static_cast<T>(1) - static_cast<T>(2) * (rotation.x * rotation.x + rotation.y * rotation.y);
        return vec3(x, y, z);
    }

    template <typename T>
    inline constexpr vec3<T> extract_up(const quat<T>& rotation)
    {
        const T x = static_cast<T>(2) * (rotation.x * rotation.y - rotation.w * rotation.z);
        const T y = static_cast<T>(1) - static_cast<T>(2) * (rotation.x * rotation.x + rotation.z * rotation.z);
        const T z = static_cast<T>(2) * (rotation.y * rotation.z + rotation.w * rotation.x);
        return vec3(x, y, z);
    }

    template <typename T>
    inline constexpr vec2<T> encode_direction_to_euler_angles(const vec3<T>& dir)
    {
        const auto d = normalize(dir);
        const T theta = std::atan(static_cast<T>(1) / d.z);
        const T phi = std::atan(d.y / d.x);
        return {as_degrees(theta), as_degrees(phi)};
    }
} // namespace tempest::math

#endif // tempest_math_transformations_hpp__
