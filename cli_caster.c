#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Math */
typedef struct {
  float x, y, z;
} Vec3;

typedef struct {
  uint32_t a, b, c;
} Face;

typedef struct {
  Vec3 a, b, c, norm;
} Tri;

Vec3 vec3_add(Vec3 a, Vec3 b) {
  return (Vec3){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z};
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
  return (Vec3){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z};
}

Vec3 vec3_scale(Vec3 a, float t) {
  return (Vec3){.x = a.x * t, .y = a.y * t, .z = a.z * t};
}

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 cross(Vec3 a, Vec3 b) {
  return (Vec3){
      .x = a.y * b.z - a.z * b.y,
      .y = a.z * b.x - a.x * b.z,
      .z = a.x * b.y - a.y * b.x,
  };
}

Vec3 calc_norm(Vec3 a, Vec3 b, Vec3 c) {
  Vec3 u = vec3_sub(b, a), v = vec3_sub(c, a), norm = cross(u, v);
  float mag = sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
  return vec3_scale(norm, 1.0 / mag);
}

Vec3 rot_x(Vec3 vec, float angle) {
  return (Vec3){
      .x = vec.x,
      .y = vec.y * cosf(angle) - vec.z * sinf(angle),
      .z = vec.y * sinf(angle) + vec.z * cosf(angle),
  };
}

Vec3 rot_y(Vec3 vec, float angle) {
  return (Vec3){
      .x = vec.x * cosf(angle) + vec.z * sinf(angle),
      .y = vec.y,
      .z = -vec.x * sinf(angle) + vec.z * cosf(angle),
  };
}

Vec3 rot_z(Vec3 vec, float angle) {
  return (Vec3){
      .x = vec.x * cosf(angle) - vec.y * sinf(angle),
      .y = vec.x * sinf(angle) + vec.y * cosf(angle),
      .z = vec.z,
  };
}

Vec3 transform(Vec3 vec, Vec3 offset, Vec3 rot) {
  return vec3_add(rot_z(rot_y(rot_x(vec, rot.x), rot.y), rot.z), offset);
}

/* Models */
typedef struct {
  Tri *tris;
  size_t num_tris;
} Model;

Model model_load(FILE *fp) {
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  Vec3 *points = malloc(sizeof(Vec3));
  size_t num_points = 0;
  size_t point_capacity = 1;

  Tri *tris = malloc(sizeof(Tri));
  size_t num_tris = 0;
  size_t tri_capacity = 1;

  while ((read = getline(&line, &len, fp)) != -1) {
    Vec3 point;
    Face face;
    if (sscanf(line, "v %f %f %f\n", &point.x, &point.y, &point.z) == 3) {
      points[num_points] = point;
      num_points++;
      if (num_points == point_capacity) {
        point_capacity *= 2;
        points = realloc(points, sizeof(Vec3) * point_capacity);
      }
    } else if (sscanf(line, "f %u %u %u\n", &face.a, &face.b, &face.c) == 3) {
      Tri tri;
      tri.a = points[face.a - 1];
      tri.b = points[face.b - 1];
      tri.c = points[face.c - 1];
      tri.norm = calc_norm(tri.a, tri.b, tri.c);
      /*
      printf("Tri: (%f %f %f) (%f %f %f) (%f %f %f) -> (%f %f %f)\n", tri.a.x,
                   tri.a.y, tri.a.z, tri.b.x, tri.b.y, tri.b.z, tri.c.x,
                   tri.c.y, tri.c.z, tri.norm.x, tri.norm.y, tri.norm.z);
      */
      tris[num_tris] = tri;
      num_tris++;
      if (num_tris == tri_capacity) {
        tri_capacity *= 2;
        tris = realloc(tris, sizeof(Tri) * tri_capacity);
      }
    } else {
      printf("Unparseable line: %s\n", line);
      exit(EXIT_FAILURE);
    }
  }

  if (line) {
    free(line);
  }
  free(points);

  return (Model){.tris = tris, .num_tris = num_tris};
}

void model_free(Model model) { free(model.tris); }

/* Graphics */
typedef struct {
  uint8_t r, g, b;
} Color;

typedef struct {
  Color *pixels;
  size_t width, height;
  float aspect_ratio;
} Screen;

Screen screen_new(size_t width, size_t height, float aspect_ratio) {
  return (Screen){
      .pixels = malloc(sizeof(Color) * width * height),
      .width = width,
      .height = height,
      .aspect_ratio = aspect_ratio,
  };
}

void screen_free(Screen screen) { free(screen.pixels); }

void set_pixel(Screen *screen, size_t x, size_t y, Color color) {
  screen->pixels[screen->width * y + x] = color;
}

void draw_color(Screen screen) {
  printf("\x1b[H");
  for (int y = 0; y < screen.height; y++) {
    for (int x = 0; x < screen.width; x++) {
      Color pixel = screen.pixels[screen.width * y + x];
      printf("\x1b[48;2;%d;%d;%dm ", pixel.r, pixel.g, pixel.b);
    }
    printf("\x1b[0m\n");
  }
}

void draw_ascii(Screen screen) {
  printf("\x1b[H");
  for (int y = 0; y < screen.height; y++) {
    for (int x = 0; x < screen.width; x++) {
      Color pixel = screen.pixels[screen.width * y + x];
      uint8_t shade = (pixel.r + pixel.g + pixel.b) / 3;
      printf("%c", ".,:~=+*#%@"[shade * 10 / 256]);
    }
    printf("\n");
  }
}

void trace_model(Screen *screen, Model model, Vec3 offset, Vec3 rot) {
  // first, create a transformed list of the model's tris
  Tri *tris = malloc(sizeof(Tri) * model.num_tris);
  for (size_t i = 0; i < model.num_tris; i++) {
    Tri tri = model.tris[i];
    tris[i] = (Tri){
        .a = transform(tri.a, offset, rot),
        .b = transform(tri.b, offset, rot),
        .c = transform(tri.c, offset, rot),
        .norm = transform(tri.norm, (Vec3){.x = 0, .y = 0, .z = 0}, rot),
    };
    tri = tris[i];
    /*
    printf("Tri: (%f %f %f) (%f %f %f) (%f %f %f) -> (%f %f %f)\n", tri.a.x,
           tri.a.y, tri.a.z, tri.b.x, tri.b.y, tri.b.z, tri.c.x, tri.c.y,
           tri.c.z, tri.norm.x, tri.norm.y, tri.norm.z);
    */
  }

  // Draw
  Vec3 orig = (Vec3){.x = 0, .y = 0, .z = 0};
  Vec3 light = (Vec3){.x = 0, .y = 1, .z = 0};
  for (int y = 0; y < screen->height; y++) {
    float ya = ((float)y / screen->height - 0.5) * M_PI_2;
    for (int x = 0; x < screen->width; x++) {
      float xa = ((float)x / screen->width - 0.5) * M_PI_2;
      // calculate ray's normal
      Vec3 dir = rot_y(rot_x((Vec3){.x = 0, .y = 0, .z = -1}, xa), ya);
      // see if there are any intersections
      uint8_t shade = 0;
      float min_z = INFINITY;
      for (Tri *tri = tris; tri < tris + model.num_tris; tri++) {
        // make sure direction and normal are not parallel
        if (dot(tri->norm, dir) == 0) {
          continue;
        }
        float D = dot(tri->norm, tri->a);
        float t = -D / dot(tri->norm, dir);
        if (t < 0) {
          continue;
        }
        Vec3 Phit = vec3_scale(dir, -t);
        if (Phit.z > min_z) {
          continue;
        }

        // check edges
        Vec3 C;

        Vec3 edge0 = vec3_sub(tri->b, tri->a);
        Vec3 vp0 = vec3_sub(Phit, tri->a);
        C = cross(edge0, vp0);
        if (dot(tri->norm, C) < 0) {
          continue;
        }

        Vec3 edge1 = vec3_sub(tri->c, tri->b);
        Vec3 vp1 = vec3_sub(Phit, tri->b);
        C = cross(edge1, vp1);
        if (dot(tri->norm, C) < 0) {
          continue;
        }

        Vec3 edge2 = vec3_sub(tri->a, tri->c);
        Vec3 vp2 = vec3_sub(Phit, tri->c);
        C = cross(edge2, vp2);
        if (dot(tri->norm, C) < 0) {
          continue;
        }

        shade = (dot(tri->norm, light) + 1) * 255 / 2;
        min_z = Phit.z;
        break;
      }
      set_pixel(screen, x, y, (Color){.r = shade, .g = shade, .b = shade});
    }
  }
}

int main(int argc, char **argv) {
  size_t width = 200, height = 100;
  float aspect_ratio = 1.8;
  Screen screen = screen_new(width, height, aspect_ratio);

  if (argc != 2) {
    fprintf(stderr, "Expected filename\n");
    exit(EXIT_FAILURE);
  }

  FILE *fp = fopen(argv[1], "r");
  Model model = model_load(fp);
  fclose(fp);

  for (float a = 0.1;; a += 0.1) {
    trace_model(&screen, model, (Vec3){0, 0, 2}, (Vec3){a, 0.4 * a, 0.5 * a});
    draw_color(screen);
  }

  screen_free(screen);
  model_free(model);
}
