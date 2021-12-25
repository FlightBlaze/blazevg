//
//  blazevgc.c
//  MyProject
//
//  Created by Dmitry on 21.12.2021.
//

#include "blazevgc.h"
#include <stdlib.h>
#include <memory.h>
#include <assert.h>
#include <math.h>

void vg_lerp(float* dest, float a, float b, float t) {
    *dest = a + t * (b - a);
}

void vg_zero_v2(struct vg_v2* v) {
    v->x = 0.0f;
    v->y = 0.0f;
}

void vg_copy_v2(struct vg_v2* dest, struct vg_v2* src) {
    dest->x = src->x;
    dest->y = src->y;
}

void vg_add_v2(struct vg_v2* dest, struct vg_v2* src) {
    dest->x += src->x;
    dest->y += src->y;
}

void vg_sub_v2(struct vg_v2* dest, struct vg_v2* src) {
    dest->x -= src->x;
    dest->y -= src->y;
}

void vg_mul_v2(struct vg_v2* v, float scalar) {
    v->x *= scalar;
    v->y *= scalar;
}

void vg_div_v2(struct vg_v2* v, float scalar) {
    v->x /= scalar;
    v->y /= scalar;
}

void vg_len2_v2(float* dest, struct vg_v2* v) {
    *dest = v->x * v->x + v->y * v->y;
}

void vg_len_v2(float* dest, struct vg_v2* v) {
    vg_len2_v2(dest, v);
    *dest = sqrtf(*dest);
}

void vg_dot_v2(float* dest, struct vg_v2* a, struct vg_v2* b) {
    *dest = a->x * b->x + a->y * b->y;
}

void vg_lerp_v2(struct vg_v2* dest, struct vg_v2* a, struct vg_v2* b, float t) {
    vg_lerp(&dest->x, a->x, b->x, t);
    vg_lerp(&dest->y, a->y, b->y, t);
}

void vg_perpendicular_v2(struct vg_v2* dest, struct vg_v2* src) {
    struct vg_v2 v;
    v.x = src->y;
    v.y = -src->x;
    *dest = v;
}

void vg_normalize_v2(struct vg_v2* dest, struct vg_v2* src) {
    float len;
    vg_len_v2(&len, src);
    dest->x /= len;
    dest->y /= len;
}

void vg_free_v2_list(struct vg_v2_list*** head) {
    struct vg_v2_list *current, *next;
    current = **head;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    free(*head);
    *head = NULL;
}

void vg_create_polyline_shape(struct vg_v2_array* dest, struct vg_v2_list** head,
                              float thickness) {
    struct vg_v2_list *current_item, *previous_item, *next_item;
    struct vg_v2 *curr_point, *prev_point, *next_point;
    struct vg_v2 forward_dir, backward_dir, mean_dir;
    struct vg_v2 left_point, right_point;
    struct vg_v2 *points;
    size_t path_points_num, shape_points_num, i, back_i;
    vg_bool_t is_start, is_end;
    float radius;
    
    radius = thickness / 2.0f;
    
    path_points_num = 0;
    current_item = *head;
    
    while (current_item != NULL) {
        path_points_num++;
        current_item = current_item->next;
    }
    
    if(path_points_num < 2)
        return;
    
    shape_points_num = path_points_num * 2;
    dest->size = shape_points_num;
    dest->data = realloc(dest->data, sizeof(*dest->data) * dest->size);
    
    current_item = *head;
    previous_item = NULL;
    next_item = NULL;
    
    i = 0;
    back_i = shape_points_num - 1;
    is_start = VG_TRUE;
    is_end = VG_FALSE;
    points = dest->data;
    while (current_item != NULL) {
        next_item = current_item->next;
        if(next_item == NULL)
            is_end = VG_TRUE;
        
        curr_point = &current_item->vector;
        
        if (is_start) {
            next_point = &next_item->vector;
            vg_copy_v2(&mean_dir, next_point);
            vg_sub_v2(&mean_dir, curr_point);
        }
        else if (is_end) {
            prev_point = &previous_item->vector;
            vg_copy_v2(&mean_dir, curr_point);
            vg_sub_v2(&mean_dir, prev_point);
        }
        else {
            prev_point = &previous_item->vector;
            next_point = &next_item->vector;
            vg_copy_v2(&forward_dir, next_point);
            vg_copy_v2(&backward_dir, prev_point);
            vg_sub_v2(&forward_dir, curr_point);
            vg_sub_v2(&backward_dir, curr_point);
            
            vg_copy_v2(&mean_dir, &forward_dir);
            vg_add_v2(&mean_dir, &backward_dir);
            vg_div_v2(&mean_dir, 2.0f);
        }
        vg_normalize_v2(&mean_dir, &mean_dir);
        vg_perpendicular_v2(&left_point, &mean_dir);
        vg_mul_v2(&left_point, radius);
        
        vg_copy_v2(&right_point, &left_point);
        vg_mul_v2(&right_point, -1.0f);
        
        vg_add_v2(&right_point, curr_point);
        vg_add_v2(&left_point, curr_point);
        
        assert(back_i < dest->size);
        assert(i < dest->size);
        
        points[i] = left_point;
        points[back_i] = right_point;
        
        previous_item = current_item;
        current_item = next_item;
        is_start = VG_FALSE;
        back_i--;
        i++;
    }
}

void vg_create_polyline_indices(struct vg_tri_array* dest, size_t num_vertices) {
    size_t triangles_num, half_vertices_num, i, j, back_i, end;
    struct vg_tri* data;
    
    half_vertices_num = num_vertices / 2;
    triangles_num = num_vertices - 2;
    dest->size = triangles_num;
    dest->data = realloc(dest->data, sizeof(*dest->data) * dest->size);
    
    i = 0;
    j = 0;
    end = half_vertices_num - 1;
    back_i = num_vertices - 1;
    /*
       i       end
       |        |
      \|/      \|/
       0--1--2--3-\
       7--6--5--4-/
      /|\
       |
     back_i
     */
    data = dest->data;
    while(i < end) {
        /*
        i+1 |\   | back_i - 1
            |-\  |
            |--\ |
          i |---\| back_i
         */
        data[j].a = i;
        data[j].b = back_i;
        data[j].c = i + 1;
        
        j++;
        assert(j < dest->size);
        
        /*
        i+1 |\---| back_i - 1
            | \--|
            |  \-|
          i |   \| back_i
         */
        data[j].a = i + 1;
        data[j].b = back_i - 1;
        data[j].c = back_i;
        
        back_i--;
        j++;
        i++;
    }
}

void vg_create_convex_indices(struct vg_tri_array* dest, size_t num_vertices) {
    size_t triangles_num;
    struct vg_tri* data;
    
    triangles_num = num_vertices - 2;
    dest->size = triangles_num;
    dest->data = realloc(dest->data, sizeof(*dest->data) * dest->size);
    
    data = dest->data;
    for (size_t i = 0; i < triangles_num; i++)
    {
        data[i].a = 0;
        data[i].b = i + 1;
        data[i].c = i + 2;
    }
}

void vg_append_to_polyline_array(struct vg_polyline_array* array, struct vg_v2_list** head) {
    size_t last;
    last = array->size;
    array->size++;
    array->data = realloc(array->data, sizeof(struct vg_v2_list**) * array->size);
    array->data[last] = head;
}

void vg_clear_polyline_array(struct vg_polyline_array* array) {
    struct vg_v2_list ***data;
    data = array->data;
    for(int i = 0; i < array->size; i++) {
        vg_free_v2_list(&data[i]);
    }
    free(array->data);
    array->data = NULL;
}

void vg_free_submesh_list(struct vg_submesh_list*** head) {
    struct vg_submesh_list *current, *next;
    current = **head;
    while (current != NULL) {
        next = current->next;
        current->shape.size = 0;
        free(current->shape.data);
        current->tris.size = 0;
        free(current->tris.data);
        free(current);
        current = next;
    }
    free(*head);
    *head = NULL;
}

void vg_move_to(struct vg_context* ctx, float x, float y) {
    ctx->cursor.x = x;
    ctx->cursor.y = y;
}

void vg_line_to(struct vg_context* ctx, float x, float y) {
    struct vg_v2_list **list_head, *current;
    list_head = malloc(sizeof(void*));
    *list_head = malloc(sizeof(struct vg_v2_list));
    current = *list_head;
    vg_copy_v2(&current->vector, &ctx->cursor);
    current->next = malloc(sizeof(struct vg_v2_list));
    current = current->next;
    current->vector.x = x;
    current->vector.y = y;
    current->next = NULL;
    vg_append_to_polyline_array(&ctx->path, list_head);
    vg_move_to(ctx, x, y);
}

struct vg_submesh_list** vg_create_stroke(struct vg_polyline_array* path) {
    struct vg_submesh_list **list_head, **current_list_head, *current_node;
    struct vg_v2_list ***data, **current_polyline;
    list_head = malloc(sizeof(void*));
    current_list_head = list_head;
    data = path->data;
    for(int i = 0; i < path->size; i++) {
        *current_list_head = (struct vg_submesh_list*)malloc(sizeof(struct vg_submesh_list));
        current_polyline = data[i];
        current_node = *current_list_head;
        memset(current_node, 0, sizeof(struct vg_submesh_list));
        vg_create_polyline_shape(&current_node->shape, current_polyline, 2.0f);
        vg_create_polyline_indices(&current_node->tris, current_node->shape.size);
        current_list_head = &current_node->next;
    }
    return list_head;
}

void vg_create_mesh(struct vg_mesh* dest, struct vg_submesh_list** submeshes) {
    struct vg_submesh_list *current;
    struct vg_tri mesh_tri, *submesh_tri;
    size_t num_verts, num_tris;
    
    num_verts = 0;
    num_tris = 0;
    current = *submeshes;
    while(current != NULL) {
        num_verts += current->shape.size;
        num_tris += current->tris.size;
        current = current->next;
    }
    
    dest->verts.size = num_verts;
    dest->tris.size = num_tris;
    dest->verts.data = calloc(dest->verts.size, sizeof(*dest->verts.data));
    dest->tris.data = calloc(dest->tris.size, sizeof(*dest->tris.data));
    
    num_verts = 0;
    num_tris = 0;
    current = *submeshes;
    while(current != NULL) {
        /* memcpy(&dest->verts.data[num_verts], current->shape.data, current->shape.size); */
        for(int i = 0; i < current->shape.size; i++) {
            assert(num_verts + i < dest->verts.size);
            vg_copy_v2(&dest->verts.data[num_verts + i], &current->shape.data[i]);
        }
        for(int i = 0; i < current->tris.size; i++) {
            assert(num_tris + i < dest->tris.size);
            submesh_tri = &current->tris.data[i];
            mesh_tri.a = submesh_tri->a + num_verts;
            mesh_tri.b = submesh_tri->b + num_verts;
            mesh_tri.c = submesh_tri->c + num_verts;
            dest->tris.data[num_tris + i] = mesh_tri;
        }
        num_verts += current->shape.size;
        num_tris += current->tris.size;
        current = current->next;
    }
}

void vg_clear_mesh(struct vg_mesh* mesh) {
    free(mesh->verts.data);
    free(mesh->tris.data);
    mesh->verts.data = NULL;
    mesh->tris.data = NULL;
    mesh->verts.size = 0;
    mesh->tris.size = 0;
}
