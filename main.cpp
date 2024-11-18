#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "misc/cpp/imgui_stdlib.h"
#include <stdio.h>
#include <ctype.h>
#include <SDL.h>
#include <iostream>
#include "utilities.hpp"
#include <vector>

#if !SDL_VERSION_ATLEAST(2,0,17)
#error This backend requires SDL 2.0.17+ because of SDL_RenderGeometry() function
#endif

#define CWINDOW_WIDTH 800
#define CWINDOW_HEIGHT 800
#define RWINDOW_WIDTH 600
#define RWINDOW_HEIGHT 600
#define NAME_LEN 32

typedef struct triangle {
  u32 p0, p1, p2;
  f32 red, green, blue;
} triangle;

typedef struct bsp_node {
  std::vector<triangle> t;
  vec4 plane;
} bsp_node;

typedef struct bsp_tree {
  bsp_node node;
  bsp_tree *front;
  bsp_tree *back;
} bsp_tree;

typedef struct model {
  char name[NAME_LEN];
  std::vector<vec3> points;
  std::vector<triangle> tris;
  vec3 pos;
  vec3 rot;
  vec3 scale;
  mat4 matrix;
  bool edit_vert;
  bool edit_face;
} model;

typedef struct camera {
  vec3 pos, rot;
  vec3 bg_col;
  mat4 view;
} camera;

vec4 tri_to_plane(vec3 a, vec3 b, vec3 c) {
  vec3 C = sub3(a, b);
  vec3 B = sub3(a, c);
  vec3 n = cross3(C, B);
  f32 d = -dot3(a, n);
  return cons4(n.x, n.y, n.z, d);
}

u8 behind_plane(vec3 point, vec4 plane) {
  f32 result = dot4(plane, to_3_4h(point));
  if (near_0(result)) {
    return 2;
  } else {
    return signbit(result);
  }
}

const u8 TP_FF = 0;  // 00 00 00
const u8 TP_AK = 1;  // 00 00 01
const u8 TP_FA = 2;  // 00 00 10
const u8 TP_BK = 4;  // 00 01 00
const u8 TP_CF = 5;  // 00 01 01
const u8 TP_CB = 6;  // 00 01 10
const u8 TP_FB = 8;  // 00 10 00
const u8 TP_CA = 9;  // 00 10 01
const u8 TP_FZ = 10; // 00 10 10
const u8 TP_CK = 16; // 01 00 00
const u8 TP_BF = 17; // 01 00 01
const u8 TP_BC = 18; // 01 00 10
const u8 TP_AF = 20; // 01 01 00
const u8 TP_KK = 21; // 01 01 01
const u8 TP_KA = 22; // 01 01 10
const u8 TP_AC = 24; // 01 10 00
const u8 TP_KB = 25; // 01 10 01
const u8 TP_KZ = 26; // 01 10 10
const u8 TP_FC = 32; // 10 00 00
const u8 TP_BA = 33; // 10 00 01
const u8 TP_FY = 34; // 10 00 10
const u8 TP_AB = 36; // 10 01 00
const u8 TP_KC = 37; // 10 01 01
const u8 TP_KY = 38; // 10 01 10
const u8 TP_FX = 40; // 10 10 00
const u8 TP_KX = 41; // 10 10 01
const u8 TP_FK = 42; // 10 10 10

u8 compare_tri_plane(vec4 plane, vec3 a, vec3 b, vec3 c) {
  u8 ab = behind_plane(a, plane);
  u8 bb = behind_plane(b, plane);
  u8 cb = behind_plane(c, plane);

  return ab | (bb << 2) | (cb << 4);
}

vec3 line_x_plane(vec3 start, vec3 end, vec4 plane) {
  // start = i, end = j
  vec3 n = to_4_3(plane);
  f32 d = plane.w;
  f32 p = (dot3(end, n) + d) / dot3(sub3(start, end), n);
  return sub3(mul3(end, 1+p), mul3(start, p));
}

// A will be assumed to be the outlier in front.
void subdiv4(triangle t, vec3 a, vec3 b, vec3 c, vec4 plane, std::vector<vec3>& ps, std::vector<triangle>& front, std::vector<triangle>& back) {
  vec3 mA = lerp3(b, c, 0.5);
  vec3 mB = line_x_plane(a, c, plane);
  vec3 mC = line_x_plane(a, b, plane);
  u32 start = (u32) ps.size();
  ps.push_back(mA);
  ps.push_back(mB);
  ps.push_back(mC);

  front.push_back((triangle) { start+1, t.p0,    start+2, t.red, t.green, t.blue });
  back.push_back((triangle)  { t.p1,    start+0, start+2, t.red, t.green, t.blue });
  back.push_back((triangle)  { start,   start+1, t.p2,    t.red, t.green, t.blue }); 
  back.push_back((triangle)  { start,   start+1, start+2, t.red, t.green, t.blue });
}

// A is on the plane, B is in front, C is behind.
void subdiv2(triangle t, vec3 b, vec3 c, vec4 plane, std::vector<vec3>& ps, std::vector<triangle>& front, std::vector<triangle>& back) {
  vec3 mA = line_x_plane(b, c, plane);
  
  u32 start = (u32) ps.size();
  ps.push_back(mA);

  front.push_back((triangle) { t.p0, t.p1, start, t.red, t.green, t.blue });
  back.push_back((triangle)  { t.p1, t.p2, start, t.red, t.green, t.blue });
}

u8 rate_comp(u8 comp) {
  if (comp == TP_FF || comp == TP_FA || comp == TP_FB || comp == TP_FC || comp == TP_FX || comp == TP_FY || comp == TP_FZ || comp == TP_KK || comp == TP_KA || comp == TP_KB || comp == TP_KC || comp == TP_KX || comp == TP_KY || comp == TP_KZ || comp == TP_FK) {
    return 1;
  } else if (comp == TP_AF || comp == TP_BF || comp == TP_CF || comp == TP_AK || comp == TP_BK || comp == TP_CK) {
    return 4;
  } else {
    return 2;
  }
}

void test_tri(vec4 plane, triangle t, std::vector<vec3>& ps, std::vector<triangle>& front, std::vector<triangle>& back, std::vector<triangle>& at) {
  vec3 a = ps[t.p0];
  vec3 b = ps[t.p1];
  vec3 c = ps[t.p2];
  u8 result = compare_tri_plane(plane, a, b, c);
  
  switch (result) {
  case TP_FF:
    front.push_back(t);
    break;
  case TP_KK:
    back.push_back(t);
    break;
  case TP_FK:
    at.push_back(t);
    break;
  case TP_AF:
    subdiv4(t, a, b, c, plane, ps, front, back);
    break;
  case TP_BF:
    subdiv4(t, b, a, c, plane, ps, front, back);
    break;
  case TP_CF:
    subdiv4(t, c, a, b, plane, ps, front, back);
    break;
  case TP_AK:
    subdiv4(t, a, b, c, plane, ps, back, front);
    break;
  case TP_BK:
    subdiv4(t, b, a, c, plane, ps, back, front);
    break;
  case TP_CK:
    subdiv4(t, c, a, b, plane, ps, back, front);
    break;
  case TP_AB:
    subdiv2(t, a, b, plane, ps, front, back);
    break;
  case TP_AC:
    subdiv2(t, a, c, plane, ps, front, back);
    break;
  case TP_BA:
    subdiv2(t, b, a, plane, ps, front, back);
    break;
  case TP_BC:
    subdiv2(t, b, c, plane, ps, front, back);
    break;
  case TP_CA:
    subdiv2(t, c, a, plane, ps, front, back);
    break;
  case TP_CB:
    subdiv2(t, c, b, plane, ps, front, back);
    break;
  case TP_FA:;
  case TP_FB:;
  case TP_FC:;
  case TP_FX:;
  case TP_FY:;
  case TP_FZ:
    front.push_back(t);
    break;
  case TP_KA:;
  case TP_KB:;
  case TP_KC:;
  case TP_KX:;
  case TP_KY:;   
  case TP_KZ:
    back.push_back(t);
    break;
  }
}

typedef struct bsp_queue {
  bsp_tree *branch;
  std::vector<triangle> queue;
} bsp_queue;

usize choose_test(std::vector<vec3>& ps, std::vector<triangle>& tris) {
  usize best_index = 0;
  u32 best_score = 1 << 31;
  for (usize i = 0; i < tris.size(); i++) {
    triangle tri1 = tris[i];
    vec4 plane = tri_to_plane(ps[tri1.p0], ps[tri1.p1], ps[tri1.p2]);
    u32 score = 0;
    for (usize j = 0; j < tris.size(); j++) {
      if (i != j) {
	triangle tri2 = tris[j];
	u8 result = compare_tri_plane(plane, ps[tri2.p0], ps[tri2.p1], ps[tri2.p2]);
	score += rate_comp(result);
      }
    }
    if (score < best_score) {
      best_index = i;
      best_score = score;
    }
  }
  return best_index;
}

bsp_tree *generate_bsp(std::vector<vec3>& ps, std::vector<triangle> tris) {
  if (tris.size()) {
    usize test1i = choose_test(ps, tris);
    std::swap(tris.at(test1i), tris.at(tris.size() - 1));
    triangle test1 = tris.back();
    tris.pop_back();
    std::vector<triangle> tlist1 = {};
    tlist1.push_back(test1);
    bsp_tree *tree = new bsp_tree((bsp_tree) { (bsp_node) { tlist1, tri_to_plane(ps[test1.p0], ps[test1.p1], ps[test1.p2]) }, NULL, NULL });
    std::vector<bsp_queue> queue = {};
    queue.push_back((bsp_queue) { tree, tris });
    
    bsp_queue current;
    while (queue.size()) {
      current = queue.back();
      queue.pop_back();
      std::vector<triangle> front = {};
      std::vector<triangle> back = {};
      vec4 plane = current.branch->node.plane;
      for (usize i = 0; i < current.queue.size(); i++) {
	test_tri(plane, current.queue[i], ps, front, back, current.branch->node.t);
      }
      
      if (front.size()) {
	usize test2i = choose_test(ps, front);
	std::swap(front.at(test2i), front.at(front.size() - 1));
	triangle test2 = front.back();
	front.pop_back();
	std::vector<triangle> tlist2 = {};
	tlist2.push_back(test2);
	bsp_tree *front_tree = new bsp_tree((bsp_tree) { (bsp_node) { tlist2, tri_to_plane(ps[test2.p0], ps[test2.p1], ps[test2.p2]) }, NULL, NULL });
	
	queue.push_back((bsp_queue) { front_tree, front });
	current.branch->front = front_tree;
      }
      
      if (back.size()) {
	usize test2i = choose_test(ps, back);
	std::swap(back.at(test2i), back.at(back.size() - 1));
	triangle test2 = back.back();
	back.pop_back();
	std::vector<triangle> tlist2 = {};
	tlist2.push_back(test2);
	bsp_tree *back_tree = new bsp_tree((bsp_tree) { (bsp_node) { tlist2, tri_to_plane(ps[test2.p0], ps[test2.p1], ps[test2.p2]) }, NULL, NULL });
	
	queue.push_back((bsp_queue) { back_tree, back });
	current.branch->back = back_tree;
      }
    }
    return tree;
  } else {
    return NULL;
  }
}

int triangle_order(const void *a, const void *b) {
  return (((vec2 *)a)->y < ((vec2 *)b)->y) ? -1 : 1;
}

void draw_triangle(SDL_Surface *surface, vec2 p0, vec2 p1, vec2 p2, f32 red, f32 green, f32 blue) {
  vec2 points[3] = { p0, p1, p2 };
  qsort(points, 3, sizeof(vec2), triangle_order);
  vec2 a = points[0];
  vec2 b = points[1];
  vec2 c = points[2];

  f32 A = (b.x - c.x) / (b.y - c.y);
  f32 B = (a.x - c.x) / (a.y - c.y);
  f32 C = (a.x - b.x) / (a.y - b.y);

  if (dist(a.y, b.y) <= 1.0) {
    f32 start = MIN(a.x, b.x);
    f32 end = MAX(a.x, b.x);
    f32 si = MAX(A, B);
    f32 ei = MIN(A, B);

    if (a.y < 0) {
      start += si * -a.y;
      end += ei * -a.y;
      a.y = 0;
      b.y = 0;
    }

    for (f32 y = a.y; y < MIN(c.y, RWINDOW_HEIGHT); y++) {
      for (f32 x = MAX(start, 0); x < MIN(end, RWINDOW_WIDTH); x++) {
	// std::cout << "Writing to pixel ";
	// debug2(cons2(x, y));
	// std::cout << "\n" << std::flush;
	((u32 *)surface->pixels)[(int)y * surface->w + (int)x] = SDL_MapRGB(surface->format, (u8) (red * 255), (u8) (green * 255), blue);
      }
      start += si;
      end += ei;
    }
  } else if (dist(b.y, c.y) < 1.0) {
    f32 start = a.x;
    f32 end = a.x;
    f32 si = MIN(B, C);
    f32 ei = MAX(B, C);

    if (a.y < 0) {
      start += si * -a.y;
      end += ei * -a.y;
      a.y = 0;
    }

    for (u32 y = a.y; y < MIN((u32)c.y, RWINDOW_HEIGHT); y++) {
      for (f32 x = MAX(start, 0); x < MIN(end, RWINDOW_WIDTH); x++) {
	((u32 *)surface->pixels)[(int)y * surface->w + (int)x] = SDL_MapRGB(surface->format, (u8) (red * 255), (u8) (green * 255), blue);
      }
      start += si;
      end += ei;
    }
  } else {
    f32 pas, pbs, pae, pbe;
    bool over = C < B;
    if (over) {
      pas = C;
      pae = B;
      pbs = A;
      pbe = B;
    } else {
      pas = B;
      pae = C;
      pbs = B;
      pbe = A;
    }
    
    f32 start = a.x;
    f32 end = a.x;

    if (a.y < 0) {
      if (b.y < 0) {
	start += (pas * -a.y) + (pbs * (b.y - a.y));
	end += (pae * -a.y) + (pbe * (b.y - a.y));
	a.y = 0;
	b.y = 0;
      } else {
	start += (pas * -a.y);
	end += (pae * -a.y);
	a.y = 0;
      }
    }
    
    for (f32 y = a.y; y < MIN(b.y, RWINDOW_HEIGHT); y++) {
      for (f32 x = MAX(start, 0); x <= MIN(end, RWINDOW_WIDTH); x++) {
	((u32 *)surface->pixels)[(int)y * surface->w + (int)x] = SDL_MapRGB(surface->format, (u8) (red * 255), (u8) (green * 255), blue);
      }
      start += pas;
      end += pae;
    }

    if (over) {
      if (start < b.x) {
	start = b.x;
      }
    } else {
      if (end > b.x) {
	end = b.x;
      }
    }
    
    for (f32 y = b.y; y <= MIN(c.y, RWINDOW_HEIGHT); y++) {
      for (f32 x = MAX(start, 0); x <= MIN(end, RWINDOW_WIDTH); x++) {
	((u32 *)surface->pixels)[(int)y * surface->w + (int)x] = SDL_MapRGB(surface->format, (u8) (red * 255), (u8) (green * 255), blue);
      }
      start += pbs;
      end += pbe;
    }
  }
}

void render_bsp(SDL_Surface *surface, std::vector<vec3>& points, bsp_tree *bsp, vec3 cpos, mat4 view) {
  if (bsp) {
    bsp_node node = bsp->node;
    u8 result = behind_plane(cpos, node.plane);
    switch (result) {
    case 0:
      render_bsp(surface, points, bsp->front, cpos, view);
      if (dot4(to_3_4h(mul3(cpos, -1)), node.plane) > 0) {
	for (usize i = 0; i < node.t.size(); i++) {
	  vec3 p0 = points[node.t[i].p0];
	  vec3 p1 = points[node.t[i].p1];
	  vec3 p2 = points[node.t[i].p2];
	  vec2 a = to_4h_2(mul4(to_3_4h(p0), view));
	  vec2 b = to_4h_2(mul4(to_3_4h(p1), view));
	  vec2 c = to_4h_2(mul4(to_3_4h(p2), view));
	
	  draw_triangle(surface, a, b, c, node.t[i].red, node.t[i].green, node.t[i].blue);
	}	
      }
      render_bsp(surface, points, bsp->back, cpos, view);
      break;
    case 1:
      render_bsp(surface, points, bsp->back, cpos, view);
      if (dot4(to_3_4h(mul3(cpos, -1)), node.plane) > 0) {
	for (usize i = 0; i < node.t.size(); i++) {
	  vec3 p0 = points[node.t[i].p0];
	  vec3 p1 = points[node.t[i].p1];
	  vec3 p2 = points[node.t[i].p2];
	  vec2 a = to_4h_2(mul4(to_3_4h(points[node.t[i].p0]), view));
	  vec2 b = to_4h_2(mul4(to_3_4h(points[node.t[i].p1]), view));
	  vec2 c = to_4h_2(mul4(to_3_4h(points[node.t[i].p2]), view));

	  draw_triangle(surface, a, b, c, node.t[i].red, node.t[i].green, node.t[i].blue);
	}
      }
      render_bsp(surface, points, bsp->front, cpos, view);
      break;
    case 2:
      render_bsp(surface, points, bsp->back, cpos, view);
      render_bsp(surface, points, bsp->front, cpos, view);
      break;
    }
  }
}

void render_model(SDL_Surface *surface, std::vector<vec3>& points, bsp_tree *bsp, camera c) {
  render_bsp(surface, points, bsp, c.pos, mul4x4(mul4x4(c.view, perspective), mul4x4(mul4x4(rotate(cons3(0, 0, c.rot.z)), rotate(cons3(0, c.rot.y, 0))), translate(c.pos))));
}

void clear(SDL_Surface *surface, u32 color) {
  for (int x = 0; x < surface->h; x++) {
    for (int y = 0; y < surface->w; y++) {
      int position = x * surface->w + y;
      ((u32 *)surface->pixels)[position] = color;
    }
  }  
}

void debug_bsp(bsp_tree *bsp) {
  if (!bsp) return;
  ImGui::PushID((usize) bsp->node.t.data());
  std::vector<triangle> t = bsp->node.t;
  for (usize i = 0; i < t.size(); i++) {
    ImGui::Text("%d-%d-%d", t[i].p0, t[i].p1, t[i].p2);
  }
  ImGui::Text("%f, %f, %f, %f", bsp->node.plane.x, bsp->node.plane.y, bsp->node.plane.z, bsp->node.plane.w);

  if (bsp->front) {
    if (ImGui::TreeNode("Front")) {
      debug_bsp(bsp->front);
      ImGui::TreePop();
    }
  }

  if (bsp->back) {
    if (ImGui::TreeNode("Back")) {
      debug_bsp(bsp->back);
      ImGui::TreePop();      
    }
  }

  ImGui::PopID();
}

int main() {
  srand(time(NULL));
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

#ifdef SDL_HINT_IME_SHOW_UI
  SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
  
  SDL_Window *rwindow = SDL_CreateWindow("Rendered Scene", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, RWINDOW_WIDTH, RWINDOW_HEIGHT, 0);
  SDL_Window *cwindow = SDL_CreateWindow("Configure Scene", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CWINDOW_WIDTH, CWINDOW_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
  SDL_Renderer *renderer = SDL_CreateRenderer(cwindow, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  SDL_Surface *surface = SDL_GetWindowSurface(rwindow);
  SDL_Log("%s", SDL_GetError());

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void) io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForSDLRenderer(cwindow, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  u8 px_width = surface->format->BytesPerPixel;
  if (px_width != 4) {
    std::cerr << "unsupported pixel format";
    return 1;
  }

  camera c = (camera) { cons3(0, 0, -5), cons3(0, 0, 0), cons3(0,0,0), mul4x4(scale(cons3(RWINDOW_WIDTH, RWINDOW_WIDTH, 1)), mul4x4(translate(cons3(0.5, 0.5, 0)), perspective)) };
  std::vector<model> models = {};

  std::vector<vec3> points = {};
  std::vector<triangle> tris = {};
  
  bsp_tree *bsp = NULL;

  f32 move_speed = 0.1;
  f32 rotation_speed = 0.01;

  int last_x, last_y;
  int x, y;
  SDL_GetGlobalMouseState(&x, &y);

  bool show_demo_window = true;
  ImVec4 clear_color = ImVec4(0, 0, 0, 1.0);
  bool done = false;
  bool frozen = true;
  bool last_f = false;
	
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && (event.window.windowID == SDL_GetWindowID(cwindow) || event.window.windowID == SDL_GetWindowID(rwindow)))) {
	done = true;
      }
    }
    
    if (SDL_GetWindowFlags(cwindow) & SDL_WINDOW_MINIMIZED) {
	SDL_Delay(10);
	continue;
    }

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Configure Scene");
    if (ImGui::TreeNode("Configure Models")) {
      for (usize i = 0; i < models.size(); i++) {
	ImGui::PushID(i);
	bool opened = ImGui::TreeNode("");
	model& m = models[i];
	ImGui::SameLine();
	ImGui::InputText("##nameinput", m.name, NAME_LEN);
	ImGui::SameLine();
	bool removed = ImGui::Button("-");
	
	if (opened) {
	  if (ImGui::Button("Edit Vertices")) {
	    m.edit_vert = true;
	  }
	  ImGui::SameLine();
	  if (ImGui::Button("Edit Faces")) {
	    m.edit_face = true;
	  }
	  
	  bool changed = false;
	  ImGui::DragFloat3("Position", (float *)(&m.pos), 0.1);
	  changed |= ImGui::IsItemEdited();
	  ImGui::DragFloat3("Rotation", (float *)(&m.rot), 0.2);
	  changed |= ImGui::IsItemEdited();
	  ImGui::DragFloat3("Scale", (float *)(&m.scale), 0.05);
	  changed |= ImGui::IsItemEdited();
	  
	  ImGui::TreePop();
	}
	if (removed) {	  
	  models.erase(std::next(models.begin(), i));
	  i--;
	}
	ImGui::PopID();
      }
      if (ImGui::Button("New Model")) {
	models.push_back((model) { "New Model", {}, {}, cons3(0,0,0), cons3(0,0,0), cons3(1,1,1), identity, false, false });
      }
      ImGui::SameLine();
      if (ImGui::Button("Generate Scene")) {
	points.clear();
	tris.clear();
	u32 offset = 0;
	for (usize i = 0; i < models.size(); i++) {
	  model m = models[i];
	  for (usize j = 0; j < m.points.size(); j++) {
	    points.push_back(to_4_3(mul4(to_3_4h(m.points[j]), mul4x4(mul4x4(scale(m.scale), rotate(m.rot)), translate(m.pos)))));
	  }
	  for (usize j = 0; j < m.tris.size(); j++) {
	    u32 a = m.tris[j].p0 + offset;
	    u32 b = m.tris[j].p1 + offset;
	    u32 c = m.tris[j].p2 + offset;
	    tris.push_back((triangle) { a, b, c, m.tris[j].red, m.tris[j].green, m.tris[j].blue });
	  }
	  offset += m.points.size();
	}
	bsp = generate_bsp(points, tris);
      }

      static u8 error = 0;
      static std::string file_path = "cube.obj";
      ImGui::InputText("Model File Path", &file_path, 0, NULL, NULL);
      std::vector<vec3> points = {};
      std::vector<triangle> faces = {};
      
      if (ImGui::Button("Load Model")) {
	FILE *f = fopen(file_path.c_str(), "r");

	if (f) {
	  char c;
	  while (true) {
	    do {
	      c = fgetc(f);
	    } while (isspace(c));
	    
	    if (c == EOF) {
	      break;
	    } else if (c == 'v') {
	      f32 x, y, z;
	      if (fscanf(f, " %f %f %f\n", &x, &y, &z) != 3) {
		error = 2;
	      }

	      points.push_back(cons3(x,y,z));
	    } else if (c == 'f') {
	      u32 a, b, c;
	      if (fscanf(f, " %u %u %u\n", &a, &b, &c) != 3) {
		error = 3;
	      }
	    
	      faces.push_back((triangle) { a, b, c, RANDF, RANDF, RANDF });
	    }
	  }
	} else {
	  error = 1;
	}
	fclose(f);
	models.push_back((model) { "New Model", points, faces, cons3(0,0,0), cons3(0,0,0), cons3(1,1,1), identity, false, false });
      }

      switch (error) {
      case 1:
	ImGui::Text("File does not exist.");
	break;
      case 2:
	ImGui::Text("Incorrectly formatted vertex.");
	break;
      case 3:
	ImGui::Text("Incorrectly formatted face.");
	break;
      }
      
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Configure Lighting")) {
      ImGui::ColorEdit3("Background", (float *) &c.bg_col);
      ImGui::TreePop();
    }
    
    ImGui::End();

    for (usize i = 0; i < models.size(); i++) {
      model &m = models[i];
      char window_name[NAME_LEN + 11];
      snprintf(window_name, NAME_LEN+11, "%s (Vertices)", m.name);
      if (ImGui::Begin(window_name, &m.edit_vert)) {
	for (usize j = 0; j < m.points.size(); j++) {
	  vec3 *v = m.points.data() + j;
	  ImGui::PushID(j);
	  std::string label = std::to_string(j);
	  ImGui::Text("%s", label.c_str());
	  ImGui::SameLine();
	  ImGui::DragFloat3("", (float *) v, 0.1);
	  ImGui::SameLine();
	  if (ImGui::Button("-")) {
	    j--;
	    m.points.erase(std::next(m.points.begin(), j));
	    for (usize k = 0; k < m.tris.size(); k++) {
	      triangle &t = m.tris[k];
	      if (t.p0 == j || t.p1 == j || t.p2 == j) {
		m.tris.erase(std::next(m.tris.begin(), k));
		k--;
	      } else {
		if (t.p0 > j) {
		  t.p0--;
		}
		if (t.p1 > j) {
		  t.p1--;
		}
		if (t.p2 > j) {
		  t.p2--;
		}
	      }
	    }
	  }
	
	  ImGui::PopID();
	}
      
	if (ImGui::Button("Add New Vertex")) {
	  m.points.push_back(cons3(0,0,0));
	}
      }
      
      ImGui::End();

      snprintf(window_name, NAME_LEN+11, "%s (Faces)", m.name);
      if (ImGui::Begin(window_name, &m.edit_face)) {
	for (usize i = 0; i < m.tris.size(); i++) {
	  triangle &t = m.tris[i];
	  ImGui::PushID(i);
	  ImGui::InputScalarN("", ImGuiDataType_U32, (u32 *) &t, 3);
	  ImGui::ColorEdit3("", (f32 *) &t.red);
	  ImGui::SameLine();
	  if (ImGui::Button("-")) {
	    m.tris.erase(std::next(m.tris.begin(), i));
	    i--;
	  }
	  ImGui::PopID();
	}

	if (ImGui::Button("Add New Face")) {
	  m.tris.push_back((triangle) { 0,0,0,0,0,0 });
	}
      }

      ImGui::End();
    }
  	
    ImGui::Render();
    SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
    SDL_RenderClear(renderer);    
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    
    const u8 *kb = SDL_GetKeyboardState(NULL);
    
    last_x = x;
    last_y = y;
    SDL_GetGlobalMouseState(&x, &y);

    f32 dx = (f32) (x - last_x);
    f32 dy = (f32) (y - last_y);

    if (kb[SDL_SCANCODE_F]) {
      if (!last_f) {
	frozen = !frozen;
      }
      last_f = true;
    } else {
      last_f = false;
    }
    
    if (!frozen) {
      c.rot.y += dx * rotation_speed;
      c.rot.z -= dy * rotation_speed;

      if (kb[SDL_SCANCODE_W]) {
	c.pos.x += move_speed;
      } else if (kb[SDL_SCANCODE_S]) {
	c.pos.x -= move_speed;
      }
      if (kb[SDL_SCANCODE_A]) {
	c.pos.z += move_speed;
      } else if (kb[SDL_SCANCODE_D]) {
	c.pos.z -= move_speed;
      }
      if (kb[SDL_SCANCODE_SPACE]) {
	c.pos.y += move_speed;
      } else if (kb[SDL_SCANCODE_LSHIFT]) {
	c.pos.y -= move_speed;
      }
    }
    
    SDL_LockSurface(surface);
    clear(surface, SDL_MapRGB(surface->format, (u8) (c.bg_col.x * 255), (u8) (c.bg_col.y * 255), (u8) (c.bg_col.z * 255)));
    render_model(surface, points, bsp, c);
    SDL_UnlockSurface(surface);
    SDL_UpdateWindowSurface(rwindow);
    std::cout << std::flush;
  }
 
  SDL_Quit();
}
