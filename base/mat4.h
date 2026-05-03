#pragma once

#include <base/base_types.h>

// 4x4 matrix in column-major order (for compatibility with Metal/OpenGL)
typedef struct {
    float m[16];
} mat4;

// Create identity matrix
mat4 mat4_identity(void);

// Matrix multiplication: result = a * b
mat4 mat4_multiply(mat4 a, mat4 b);

// Create perspective projection matrix
// fov_y: vertical field of view in radians
// aspect: aspect ratio (width/height)
// near: near clipping plane
// far: far clipping plane
mat4 mat4_perspective(float fov_y, float aspect, float near, float far);

// Create translation matrix
mat4 mat4_translate(float x, float y, float z);

// Create rotation matrix around X axis (angle in radians)
mat4 mat4_rotate_x(float angle);

// Create rotation matrix around Y axis (angle in radians)
mat4 mat4_rotate_y(float angle);

// Create rotation matrix around Z axis (angle in radians)
mat4 mat4_rotate_z(float angle);

// Create scale matrix
mat4 mat4_scale(float x, float y, float z);

// Create view matrix from camera position and orientation (yaw, pitch in radians)
mat4 mat4_look_at_fps(float cam_x, float cam_y, float cam_z, float yaw, float pitch);
