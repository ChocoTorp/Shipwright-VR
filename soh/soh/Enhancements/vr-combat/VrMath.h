#pragma once
// QuestShip: small shared math for the VR hand code. Quaternions are (x, y, z, w), unit length,
// the layout VR_GetHandPose hands out.

namespace VrCombat {

// Rotate v by q (conj = true rotates by its inverse). out may not alias v.
inline void QuatRotate(const float q[4], const float v[3], float out[3], bool conj = false) {
    const float x = conj ? -q[0] : q[0], y = conj ? -q[1] : q[1], z = conj ? -q[2] : q[2], w = q[3];
    const float tx = 2.0f * (y * v[2] - z * v[1]);
    const float ty = 2.0f * (z * v[0] - x * v[2]);
    const float tz = 2.0f * (x * v[1] - y * v[0]);
    out[0] = v[0] + w * tx + (y * tz - z * ty);
    out[1] = v[1] + w * ty + (z * tx - x * tz);
    out[2] = v[2] + w * tz + (x * ty - y * tx);
}

// 3x3 rotation matrix of q, r[row][col], for column vectors.
inline void QuatToRot(const float q[4], float r[3][3]) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    r[0][0] = 1 - 2 * (y * y + z * z);
    r[0][1] = 2 * (x * y - z * w);
    r[0][2] = 2 * (x * z + y * w);
    r[1][0] = 2 * (x * y + z * w);
    r[1][1] = 1 - 2 * (x * x + z * z);
    r[1][2] = 2 * (y * z - x * w);
    r[2][0] = 2 * (x * z - y * w);
    r[2][1] = 2 * (y * z + x * w);
    r[2][2] = 1 - 2 * (x * x + y * y);
}

} // namespace VrCombat
