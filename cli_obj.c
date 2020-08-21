#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// customization variables
#define WIDTH 100
#define HEIGHT 50
#define CHAR_HW_RATIO 2
#define MODEL_SCALE 1
#define YAW_RATE 0.0006

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
inline Vector transform(Vector v, float s, float c) {
  return (Vector){v.x * c - v.z * s, -v.y, v.z * c + v.x * s};
}

/*
 * Calculate the shade character of a given the transformed normal
 */
inline char calc_shade(Vector norm) {
  return ".,:~=+*#%@"[(
      int)((norm.x * 0.57 + norm.y * 0.57 - norm.z * 0.57 + 1) * 5)];
}

/*
 * Draw a given triangle
 */
inline void draw_tri(Vector a, Vector b,
                     Vector c, char value) {
  SHADE_BUF[(int)(a.y * 10 + 20)][(int)(a.x * 10 + 10)] = value;
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
  for (float yaw = 0;; yaw += YAW_RATE) {
    // clear shade and depth buffer
    for (int y = 0; y < HEIGHT; y++)
      for (int x = 0; x < WIDTH; x++) {
        SHADE_BUF[y][x] = ' ';
        DEPTH_BUF[y][x] = ~0;
      }

    // calculate trig ids
    float yaw_sin = sinf(yaw), yaw_cos = cosf(yaw);

    // loop through faces
    for (Face *face = faces; face < faces + num_faces; face++) {
      Vector a = verts[face->a], b = verts[face->b], c = verts[face->c], norm;

      // transform vectors
      a = transform(a, yaw_sin, yaw_cos);
      b = transform(b, yaw_sin, yaw_cos);
      c = transform(c, yaw_sin, yaw_cos);
      norm = transform(norm, yaw_sin, yaw_cos);

      // calculate shade
      char shade = calc_shade(norm);

      // draw triangle
      draw_tri(a, b, c, shade);
    }

    // print buffer
    printf("\x1b[H%s", *SHADE_BUF);
  }
}
