#include "mat4.h"
#include <base/base_math.h>
#include <base/mem.h>

mat4 mat4_identity(void) {
    mat4 result;
    base_memset(&result, 0, sizeof(mat4));
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

mat4 mat4_multiply(mat4 a, mat4 b) {
    mat4 result;

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            result.m[col * 4 + row] = sum;
        }
    }

    return result;
}

mat4 mat4_perspective(float fov_y, float aspect, float near, float far) {
    mat4 result;
    base_memset(&result, 0, sizeof(mat4));

    float tan_half_fov = 1.0f / fast_tan(fov_y / 2.0f);

    result.m[0] = tan_half_fov / aspect;
    result.m[5] = tan_half_fov;
    result.m[10] = far / (near - far);
    result.m[11] = -1.0f;
    result.m[14] = -(far * near) / (far - near);

    return result;
}

mat4 mat4_translate(float x, float y, float z) {
    mat4 result = mat4_identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

mat4 mat4_rotate_x(float angle) {
    mat4 result = mat4_identity();
    float c = fast_cos(angle);
    float s = fast_sin(angle);

    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;

    return result;
}

mat4 mat4_rotate_y(float angle) {
    mat4 result = mat4_identity();
    float c = fast_cos(angle);
    float s = fast_sin(angle);

    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;

    return result;
}

mat4 mat4_rotate_z(float angle) {
    mat4 result = mat4_identity();
    float c = fast_cos(angle);
    float s = fast_sin(angle);

    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;

    return result;
}

mat4 mat4_scale(float x, float y, float z) {
    mat4 result = mat4_identity();
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

mat4 mat4_look_at_fps(float cam_x, float cam_y, float cam_z, float yaw, float pitch) {
    // Build view matrix for FPS camera
    // View matrix = inverse of camera transform

    float cos_pitch = fast_cos(pitch);
    float sin_pitch = fast_sin(pitch);
    float cos_yaw = fast_cos(yaw);
    float sin_yaw = fast_sin(yaw);

    // Forward, right, up vectors from yaw and pitch
    float forward_x = cos_pitch * cos_yaw;
    float forward_y = sin_pitch;
    float forward_z = cos_pitch * sin_yaw;

    float right_x = -sin_yaw;
    float right_y = 0.0f;
    float right_z = cos_yaw;

    float up_x = right_y * forward_z - right_z * forward_y;
    float up_y = right_z * forward_x - right_x * forward_z;
    float up_z = right_x * forward_y - right_y * forward_x;

    // Build view matrix (inverse of camera transform)
    mat4 result;

    // Rotation part (transpose of rotation matrix)
    result.m[0] = right_x;
    result.m[1] = up_x;
    result.m[2] = -forward_x;
    result.m[3] = 0.0f;

    result.m[4] = right_y;
    result.m[5] = up_y;
    result.m[6] = -forward_y;
    result.m[7] = 0.0f;

    result.m[8] = right_z;
    result.m[9] = up_z;
    result.m[10] = -forward_z;
    result.m[11] = 0.0f;

    // Translation part
    result.m[12] = -(right_x * cam_x + right_y * cam_y + right_z * cam_z);
    result.m[13] = -(up_x * cam_x + up_y * cam_y + up_z * cam_z);
    result.m[14] = -(-forward_x * cam_x - forward_y * cam_y - forward_z * cam_z);
    result.m[15] = 1.0f;

    return result;
}
