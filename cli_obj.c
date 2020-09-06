#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// customization variables
#define WIDTH 100
#define HEIGHT 50
#define CHAR_HW_RATIO 2
#define MODEL_SCALE 0.9
#define YAW_RATE 0.0006
#define PITCH_RATE 0.0003

// global buffers and structs
char SHADE_BUF[HEIGHT][WIDTH + 1];
float DEPTH_BUF[HEIGHT][WIDTH];

typedef struct {
  float x, y, z;
} Vector;

typedef struct {
  int a, b, c;
  Vector norm;
} Face;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

/*
 * Given three points, calculate the normal vector of the resulting triangle
 */
inline Vector calc_norm(Vector a, Vector b, Vector c) {
  Vector u = {b.x - a.x, b.y - a.y, b.z - a.z},
         v = {c.x - a.x, c.y - a.y, c.z - a.z},
         norm = {
             u.y * v.z - u.z * v.y,
             u.z * v.x - u.x * v.z,
             u.x * v.y - u.y * v.x,
         };
  float mag = sqrtf(norm.x * norm.x + norm.y * norm.y + norm.z * norm.z);
  norm.x /= mag, norm.y /= mag, norm.z /= mag;
  return norm;
}

/*
 * Transform a vector given its trig ids
 */
inline void transform(Vector *v, float ys, float yc, float ps, float pc) {
  *v = (Vector){v->x * yc - v->z * ys, -v->y, v->z * yc + v->x * ys};
  *v = (Vector){v->x, v->y * pc - v->z * ps, v->z * pc + v->y * ps};
}

/*
 * Calculate the shade character of a given the transformed normal
 */
inline char calc_shade(Vector norm) {
  return ".,:~=+*#%@"[(
      int)((norm.x * 0.57 - norm.y * 0.57 + norm.z * 0.57 + 0.95) * 5)];
}

/*
 * Map vector from [-1, 1] to [0, SIZE] coordinate systems
 */
inline void map(Vector *v) {
  const int x_shift = HEIGHT * CHAR_HW_RATIO / 2;
  const int y_shift = HEIGHT / 2;
  *v = (Vector){v->x * x_shift * MODEL_SCALE + x_shift,
                v->y * y_shift * MODEL_SCALE + y_shift, v->z};
}

/*
 * Swap the values of two vectors
 */
inline void swap_vec(Vector *a, Vector *b) {
  Vector tmp = *a;
  *a = *b;
  *b = tmp;
}

/*
 * Draw a given triangle
 */
inline void draw_tri(Vector a, Vector b, Vector c, char value) {
  // map vectors to screen coordinate system
  map(&a);
  map(&b);
  map(&c);

  /*
   * sort vectors so that triangle is in form:
   *   a          a     *----> x+
   *  / \        / \    |
   * b_  \  or  /  _b   |
   *   ^^-c    c-^^     v y+
   */
  if (a.y > c.y)
    swap_vec(&a, &c);
  if (b.y > c.y)
    swap_vec(&b, &c);
  if (a.y > b.y)
    swap_vec(&a, &b);

  // calculate divergent angles
  int beg_y = a.y, mid_y = b.y, end_y = c.y;
  float a_full = (beg_y == end_y) ? 0 : (a.x - c.x) / (beg_y - end_y),
        a_half = (beg_y == mid_y) ? 0 : (a.x - b.x) / (beg_y - mid_y),
        x_full = a.x, x_half = a.x;

  // rasterize
  for (int y = beg_y; y <= MIN(end_y, HEIGHT - 1); y++) {
    // draw line if on screen
    if (y >= 0) {
      int left_x = MAX(MIN(x_full, x_half), 0),
          right_x = MIN(MAX(x_full, x_half), WIDTH - 1);
      for (int x = left_x; x <= right_x; x++)
        if (a.z > DEPTH_BUF[y][x]) {
          SHADE_BUF[y][x] = value;
          DEPTH_BUF[y][x] = a.z;
        }
    }

    // if reached midpoint, switch directions
    if (y == mid_y)
      a_half = (mid_y == end_y) ? 0 : (b.x - c.x) / (mid_y - end_y);

    // diverge points
    x_full += a_full;
    x_half += a_half;
  }
}

/*
 * Retrieve information about a given object file
 */
static inline void read_obj(FILE *fp, int *num_verts, int *num_faces) {
  // count vertices in file (starting at 1 to follow .obj standard)
  float _f[1];
  for (*num_verts = 1; fscanf(fp, "v %f %f %f\n", _f, _f, _f) == 3;
       (*num_verts)++)
    ;

  // count faces in file
  int _i[1];
  for (*num_faces = 0; fscanf(fp, "f %d %d %d\n", _i, _i, _i) == 3;
       (*num_faces)++)
    ;

  // ensure full file was parsed
  if (!feof(fp)) {
    char line[1024];
    printf("Unparsable line encountered: `\n%s`\n", fgets(line, 1024, fp));
    exit(EXIT_FAILURE);
  }

  // reset file location
  rewind(fp);
}

/*
 * Save contents of object file
 */
static inline void save_obj(FILE *fp, Vector *verts, Face *faces) {
  // load vertices (starting at 1 to follow .obj standard)
  for (int i = 1;
       fscanf(fp, "v %f %f %f\n", &verts[i].x, &verts[i].y, &verts[i].z) == 3;
       i++)
    ;

  // load faces
  for (int i = 0;
       fscanf(fp, "f %d %d %d\n", &faces[i].a, &faces[i].b, &faces[i].c) == 3;
       i++)
    faces[i].norm =
        calc_norm(verts[faces[i].a], verts[faces[i].b], verts[faces[i].c]);
}

int main(int argc, char **argv) {
  // check args
  if (argc < 2) {
    printf("Usage: `cli_obj <filename>`\n");
    exit(EXIT_FAILURE);
  }

  // open object file
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    printf("Unable to open file: `%s`\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  // read information from object file and allocate space
  int num_verts, num_faces;
  read_obj(fp, &num_verts, &num_faces);
  Vector verts[num_verts];
  Face faces[num_faces];

  // save object file
  save_obj(fp, verts, faces);
  fclose(fp);

  // prepare shade buffer
  for (int y = 0; y < HEIGHT; y++)
    SHADE_BUF[y][WIDTH] = '\n';

  // render loop
  for (float yaw = 0, pitch = 0;; yaw += YAW_RATE, pitch += PITCH_RATE) {
    // clear shade and depth buffer
    for (int y = 0; y < HEIGHT; y++)
      for (int x = 0; x < WIDTH; x++) {
        SHADE_BUF[y][x] = ' ';
        DEPTH_BUF[y][x] = -1. / 0.;
      }

    // calculate trig ids
    float yaw_sin = sinf(yaw), yaw_cos = cosf(yaw), pitch_sin = sinf(pitch),
          pitch_cos = cosf(pitch);

    // loop through faces
    for (Face *face = faces; face < faces + num_faces; face++) {
      Vector a = verts[face->a], b = verts[face->b], c = verts[face->c],
             norm = face->norm;

      // transform vectors
      transform(&a, yaw_sin, yaw_cos, pitch_sin, pitch_cos);
      transform(&b, yaw_sin, yaw_cos, pitch_sin, pitch_cos);
      transform(&c, yaw_sin, yaw_cos, pitch_sin, pitch_cos);
      transform(&norm, yaw_sin, yaw_cos, pitch_sin, pitch_cos);

      // calculate shade
      char shade = calc_shade(norm);

      // draw triangle
      draw_tri(a, b, c, shade);
    }

    // print buffer
    printf("\x1b[H%s", *SHADE_BUF);
  }
}
