//
//  blazevgc.h
//  MyProject
//
//  Created by Dmitry on 21.12.2021.
//

#ifndef blazevgc_h
#define blazevgc_h

#include <stdio.h>

#define VG_TRUE 1
#define VG_FALSE 0

typedef char vg_bool_t;

void vg_lerp(float* dest, float a, float b, float t);

struct vg_v2 {
    float x, y;
};

void vg_zero_v2(struct vg_v2* v);
void vg_copy_v2(struct vg_v2* dest, struct vg_v2* src);
void vg_add_v2(struct vg_v2* dest, struct vg_v2* src);
void vg_sub_v2(struct vg_v2* dest, struct vg_v2* src);
void vg_mul_v2(struct vg_v2* v, float scalar);
void vg_div_v2(struct vg_v2* v, float scalar);
void vg_len2_v2(float* dest, struct vg_v2* v);
void vg_len_v2(float* dest, struct vg_v2* v);
void vg_dot_v2(float* dest, struct vg_v2* a, struct vg_v2* b);
void vg_lerp_v2(struct vg_v2* dest, struct vg_v2* a, struct vg_v2* b, float t);
void vg_perpendicular_v2(struct vg_v2* dest, struct vg_v2* src);
void vg_normalize_v2(struct vg_v2* dest, struct vg_v2* src);

struct vg_v2_array {
    struct vg_v2* data;
    size_t size;
};

struct vg_v2_list {
    struct vg_v2_list* next;
    struct vg_v2 vector;
};

void vg_free_v2_list(struct vg_v2_list*** head);

void vg_create_polyline_shape(struct vg_v2_array* dest, struct vg_v2_list** head,
                              float thickness);

struct vg_tri {
    int a, b, c;
};

struct vg_tri_array {
    struct vg_tri* data;
    size_t size;
};

void vg_create_polyline_indices(struct vg_tri_array* dest, size_t num_vertices);
void vg_create_convex_indices(struct vg_tri_array* dest, size_t num_vertices);

struct vg_polyline_array {
    struct vg_v2_list ***data;
    size_t size;
};

void vg_append_to_polyline_array(struct vg_polyline_array* array, struct vg_v2_list** head);
void vg_clear_polyline_array(struct vg_polyline_array* array);

struct vg_submesh_list {
    struct vg_submesh_list* next;
    struct vg_v2_array shape;
    struct vg_tri_array tris;
};

void vg_free_submesh_list(struct vg_submesh_list*** head);

struct vg_context {
    struct vg_polyline_array path;
    struct vg_v2 cursor;
};

void vg_move_to(struct vg_context* ctx, float x, float y);
void vg_line_to(struct vg_context* ctx, float x, float y);

struct vg_submesh_list** vg_create_stroke(struct vg_polyline_array* path);

struct vg_mesh {
    struct vg_v2_array verts;
    struct vg_tri_array tris;
};

void vg_create_mesh(struct vg_mesh* dest, struct vg_submesh_list** submeshes);
void vg_clear_mesh(struct vg_mesh* mesh);

#endif /* blazevgc_h */
