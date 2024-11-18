#pragma once

#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <algorithm>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define RANDF ((float) rand() / (float) RAND_MAX)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;
typedef float f32;
typedef double f64;

typedef struct vec2 {
  f32 x, y;
} vec2;
typedef struct vec3 {
  f32 x, y, z;
} vec3;
typedef struct vec4 {
  f32 x, y, z, w;
} vec4;
typedef struct mat4 {
  vec4 x, y, z, w;
} mat4;

bool near_0(f32 n) {
  return n < std::nextafter(0, 1) && n > std::nextafter(0, -1);
}

f32 dist(f32 a, f32 b) {
  return abs(a - b);
}

vec2 cons2(f32 x, f32 y) {
  return (vec2) { x, y };
}

vec3 cons3(f32 x, f32 y, f32 z) {
  return (vec3) { x, y, z };
}

vec4 cons4(f32 x, f32 y, f32 z, f32 w) {
  return (vec4) { x, y, z, w };
}

mat4 cons4x4(vec4 x, vec4 y, vec4 z, vec4 w) {
  mat4 m;
  m.x = x; m.y = y; m.z = z, m.w = w;
  return m;
}

f32 dot2(vec2 a, vec2 b) {
  return a.x * b.x + a.y * b.y;
}

f32 dot3(vec3 a, vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

f32 dot4(vec4 a, vec4 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}


mat4 mul4x4(mat4 a, mat4 b) {
  return cons4x4(
    cons4(
      dot4(a.x, cons4(b.x.x, b.y.x, b.z.x, b.w.x)),
      dot4(a.x, cons4(b.x.y, b.y.y, b.z.y, b.w.y)),
      dot4(a.x, cons4(b.x.z, b.y.z, b.z.z, b.w.z)),
      dot4(a.x, cons4(b.x.w, b.y.w, b.z.w, b.w.w))
    ),
    cons4(
      dot4(a.y, cons4(b.x.x, b.y.x, b.z.x, b.w.x)),
      dot4(a.y, cons4(b.x.y, b.y.y, b.z.y, b.w.y)),
      dot4(a.y, cons4(b.x.z, b.y.z, b.z.z, b.w.z)),
      dot4(a.y, cons4(b.x.w, b.y.w, b.z.w, b.w.w))
    ),
    cons4(
      dot4(a.z, cons4(b.x.x, b.y.x, b.z.x, b.w.x)),
      dot4(a.z, cons4(b.x.y, b.y.y, b.z.y, b.w.y)),
      dot4(a.z, cons4(b.x.z, b.y.z, b.z.z, b.w.z)),
      dot4(a.z, cons4(b.x.w, b.y.w, b.z.w, b.w.w))
    ),
    cons4(
      dot4(a.w, cons4(b.x.x, b.y.x, b.z.x, b.w.x)),
      dot4(a.w, cons4(b.x.y, b.y.y, b.z.y, b.w.y)),
      dot4(a.w, cons4(b.x.z, b.y.z, b.z.z, b.w.z)),
      dot4(a.w, cons4(b.x.w, b.y.w, b.z.w, b.w.w))
    )
  );
}

vec4 mul4(vec4 a, mat4 b) {
  return cons4(dot4(a, b.x), dot4(a, b.y), dot4(a, b.z), dot4(a, b.w));
}

vec3 cross3(vec3 a, vec3 b) {
  return cons3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

vec2 add2(vec2 a, vec2 b) {
  return cons2(a.x + b.x, a.y + b.y);
}

vec3 add3(vec3 a, vec3 b) {
  return cons3(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec4 add4(vec4 a, vec4 b) {
  return cons4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

vec2 sub2(vec2 a, vec2 b) {
  return cons2(a.x - b.x, a.y - b.y);
}

vec3 sub3(vec3 a, vec3 b) {
  return cons3(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec4 sub4(vec4 a, vec4 b) {
  return cons4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

vec2 mul2(vec2 a, f32 b) {
  return cons2(a.x * b, a.y * b);
}

vec3 mul3(vec3 a, f32 b) {
  return cons3(a.x * b, a.y * b, a.z * b);
}

vec4 mul4(vec4 a, f32 b) {
  return cons4(a.x * b, a.y * b, a.z * b, a.w * b);
}

vec2 div2(vec2 a, f32 b) {
  return cons2(a.x / b, a.y / b);
}

vec3 div3(vec3 a, f32 b) {
  return cons3(a.x / b, a.y / b, a.z / b);
}

vec4 div4(vec4 a, f32 b) {
  return cons4(a.x / b, a.y / b, a.z / b, a.w / b);
}

mat4 translate(vec3 v) {
  return cons4x4(
    cons4(1.0, 0.0, 0.0, v.x),
    cons4(0.0, 1.0, 0.0, v.y),
    cons4(0.0, 0.0, 1.0, v.z),
    cons4(0.0, 0.0, 0.0, 1.0)
  );
}

mat4 rotate(vec3 v) {
  f32 sx = sin(v.x);
  f32 cx = cos(v.x);
  f32 sy = sin(v.y);
  f32 cy = cos(v.y);
  f32 sz = sin(v.z);
  f32 cz = cos(v.z);
  return cons4x4(
    cons4(cx*cy, cx*sy*sz - sx*cz, cx*sy*cz + sx*sz, 0.0),
    cons4(sx*cy, sx*sy*sz + cx*cz, sx*sy*cz - cx*sz, 0.0),
    cons4(-sy,   cy*sz,            cy*cz,            0.0),
    cons4(0.0,   0.0,              0.0,              1.0)
  );
}

mat4 scale(vec3 v) {
  return cons4x4(
    cons4(v.x, 0.0, 0.0, 0.0),
    cons4(0.0, v.y, 0.0, 0.0),
    cons4(0.0, 0.0, v.z, 0.0),
    cons4(0.0, 0.0, 0.0, 1.0)
  );
}

vec2 to_3_2(vec3 v) {
  return cons2(v.x, v.y);
}

vec2 to_4_2(vec4 v) {
  return cons2(v.x, v.y);
}

vec2 to_4h_2(vec4 v) {
  return cons2(v.x / v.w, v.y / v.w);
}

vec3 to_2_3(vec2 v) {
  return cons3(v.x, v.y, 0.0);
}

vec3 to_4_3(vec4 v) {
  return cons3(v.x, v.y, v.z);
}

vec3 to_4h_3(vec4 v) {
  return cons3(v.x / v.w, v.y / v.w, v.z / v.w);
}


vec4 to_2_4(vec2 v) {
  return cons4(v.x, v.y, 0.0, 0.0);
}

vec4 to_3_4(vec3 v) {
  return cons4(v.x, v.y, v.z, 0.0);
}

vec4 to_2_4h(vec2 v) {
  return cons4(v.x, v.y, 0.0, 1.0);
}

vec4 to_3_4h(vec3 v) {
  return cons4(v.x, v.y, v.z, 1.0);
}

vec2 lerp2(vec2 p0, vec2 p1, f32 t) {
  return add2(mul2(p0, t), mul2(p1, 1-t));
}

vec3 lerp3(vec3 p0, vec3 p1, f32 t) {
  return add3(mul3(p0, t), mul3(p1, 1-t));
}

vec4 lerp4(vec4 p0, vec4 p1, f32 t) {
  return add4(mul4(p0, t), mul4(p1, 1-t));
}

const mat4 perspective = cons4x4(
  cons4(1.0, 0.0,  0.0, 0.0),
  cons4(0.0, 1.0,  0.0, 0.0),
  cons4(0.0, 0.0,  1.0, 0.0),
  cons4(0.0, 0.0, -1.0, 0.0)
);

const mat4 identity = cons4x4(
  cons4(1.0, 0.0, 0.0, 0.0),
  cons4(0.0, 1.0, 0.0, 0.0),
  cons4(0.0, 0.0, 1.0, 0.0),
  cons4(0.0, 0.0, 0.0, 1.0)	
);

void debug2(vec2 v) {
  std::cout << "[ " << v.x << " " << v.y << " ]";
}

void debug3(vec3 v) {
  std::cout << "[ " << v.x << " " << v.y << " " << v.z << " ]";
}

void debug4(vec4 v) {
  std::cout << "[ " << v.x << " " << v.y << " " << v.z << " " << v.w << " ]";
}

void debug4x4(mat4 m) {
  std::cout << "[ ";
  debug4(m.x);
  std::cout << "\n  ";
  debug4(m.y);
  std::cout << "\n  ";
  debug4(m.z);
  std::cout << "\n  ";
  debug4(m.w);
  std::cout << " ]";
}

f32 hypot2(vec2 v) {
  return sqrt(v.x * v.x + v.y * v.y);
}

f32 hypot3(vec3 v) {
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

f32 hypot4(vec4 v) {
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

vec2 norm2(vec2 v) {
  return div2(v, hypot2(v));
}

vec3 norm3(vec3 v) {
  return div3(v, hypot3(v));
}

vec4 norm4(vec4 v) {
  return div4(v, hypot4(v));
}
