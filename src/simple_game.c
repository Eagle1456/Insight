#include "simple_game.h"

#include "debug.h"
#include "ecs.h"
#include "fs.h"
#include "gpu.h"
#include "heap.h"
#include "net.h"
#include "render.h"
#include "timer_object.h"
#include "transform.h"
#include "wm.h"
#include "debug.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct player_component_t
{
	int index;
} player_component_t;

typedef struct enemy_component_t
{
	int index;
} enemy_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct collider_component_t {
	float width;
	float height;
	float depth;
	float minY;
	float minZ;
	float maxY;
	float maxZ;
	float minX;
	float maxX;
} collider_component_t;

typedef struct simple_game_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;
	net_t* net;

	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int player_type;
	int name_type;
	int collider_type;
	int enemy_type;
	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t camera_ent;
	ecs_entity_ref_t enemy_ent;

	gpu_mesh_info_t cube_mesh;
	gpu_shader_info_t cube_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
} simple_game_t;

static void load_resources(simple_game_t* game);
static void unload_resources(simple_game_t* game);
static void spawn_player(simple_game_t* game, int index);
static void spawn_enemy(simple_game_t* game, int index);
static void spawn_camera(simple_game_t* game);
static void spawn_camera_ortho(simple_game_t* game);
static void update_players(simple_game_t* game);
static void draw_models(simple_game_t* game);

simple_game_t* simple_game_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, int argc, const char** argv)
{
	simple_game_t* game = heap_alloc(heap, sizeof(simple_game_t), 8);
	game->heap = heap;
	game->fs = fs;
	game->window = window;
	game->render = render;

	game->timer = timer_object_create(heap, NULL);
	
	game->ecs = ecs_create(heap);
	game->transform_type = ecs_register_component_type(game->ecs, "transform", sizeof(transform_component_t), _Alignof(transform_component_t));
	game->camera_type = ecs_register_component_type(game->ecs, "camera", sizeof(camera_component_t), _Alignof(camera_component_t));
	game->model_type = ecs_register_component_type(game->ecs, "model", sizeof(model_component_t), _Alignof(model_component_t));
	game->player_type = ecs_register_component_type(game->ecs, "player", sizeof(player_component_t), _Alignof(player_component_t));
	game->name_type = ecs_register_component_type(game->ecs, "name", sizeof(name_component_t), _Alignof(name_component_t));
	game->collider_type = ecs_register_component_type(game->ecs, "collider", sizeof(collider_component_t), _Alignof(collider_component_t));
	game->enemy_type = ecs_register_component_type(game->ecs, "enemy", sizeof(enemy_component_t), _Alignof(enemy_component_t));

	game->net = net_create(heap, game->ecs);
	if (argc >= 2)
	{
		net_address_t server;
		if (net_string_to_address(argv[1], &server))
		{
			net_connect(game->net, &server);
		}
		else
		{
			debug_print(k_print_error, "Unable to resolve server address: %s\n", argv[1]);
		}
	}

	load_resources(game);
	spawn_player(game, 0);
	spawn_camera(game);

	return game;
}

void simple_game_destroy(simple_game_t* game)
{
	net_destroy(game->net);
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

void simple_game_update(simple_game_t* game)
{
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	net_update(game->net);
	update_players(game);
	draw_models(game);
	render_push_done(game->render);
}

static void load_resources(simple_game_t* game)
{
	game->vertex_shader_work = fs_read(game->fs, "shaders/triangle.vert.spv", game->heap, false, false);
	game->fragment_shader_work = fs_read(game->fs, "shaders/triangle.frag.spv", game->heap, false, false);
	game->cube_shader = (gpu_shader_info_t)
	{
		.vertex_shader_data = fs_work_get_buffer(game->vertex_shader_work),
		.vertex_shader_size = fs_work_get_size(game->vertex_shader_work),
		.fragment_shader_data = fs_work_get_buffer(game->fragment_shader_work),
		.fragment_shader_size = fs_work_get_size(game->fragment_shader_work),
		.uniform_buffer_count = 1,
	};

	static vec3f_t cube_verts[] =
	{
		{ -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  1.0f },
		{  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f,  1.0f }, { 1.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{ -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f,  1.0f },
		{  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f,  1.0f },
		{ -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f,  0.0f },
	};
	static uint16_t cube_indices[] =
	{
		0, 1, 2,
		2, 3, 0,
		1, 5, 6,
		6, 2, 1,
		7, 6, 5,
		5, 4, 7,
		4, 0, 3,
		3, 7, 4,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};
	game->cube_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = cube_verts,
		.vertex_data_size = sizeof(cube_verts),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};
}

static void unload_resources(simple_game_t* game)
{
	heap_free(game->heap, fs_work_get_buffer(game->vertex_shader_work));
	heap_free(game->heap, fs_work_get_buffer(game->fragment_shader_work));
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

static void player_net_configure(ecs_t* ecs, ecs_entity_ref_t entity, int type, void* user)
{
	simple_game_t* game = user;

	model_component_t* model_comp = ecs_entity_get_component(ecs, entity, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;
}

static void spawn_player(simple_game_t* game, int index)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->player_type) |
		(1ULL << game->name_type) |
		(1ULL << game->collider_type);
	game->player_ent = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->transform_type, true);
	transform_identity(&transform_comp->transform);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "player");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->index = index;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;

	uint64_t k_player_ent_net_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->name_type);
	uint64_t k_player_ent_rep_mask =
		(1ULL << game->transform_type);
	net_state_register_entity_type(game->net, 0, k_player_ent_net_mask, k_player_ent_rep_mask, player_net_configure, game);

	net_state_register_entity_instance(game->net, 0, game->player_ent);
}

static void spawn_camera(simple_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	mat4f_make_perspective(&camera_comp->projection, (float)M_PI / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f);

	vec3f_t eye_pos = (vec3f_t){ .x = 5.0f};
	vec3f_t forward = vec3f_forward();
	vec3f_t up = vec3f_up();
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

static void spawn_camera_ortho(simple_game_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	//mat4f_make_perspective(&camera_comp->projection, (float)M_PI / 2.0f, 16.0f / 9.0f, 0.1f, 100.0f);
	mat4f_make_orthographic(&camera_comp->projection, -16.0f, 16.0f, -9.0f, 9.0f, 0.1f, 100.0f);

	vec3f_t eye_pos = (vec3f_t){ .x = 5.0f, .y = 0.0f, .z = 0.0f};
	//vec3f_t eye_pos = vec3f_zero();
	vec3f_t forward = vec3f_scale(vec3f_forward(), -1.0f);
	vec3f_t up = vec3f_scale(vec3f_up(), -1.0f);
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

static int intersect(collider_component_t* comp1, collider_component_t* comp2) {
	if (comp1->minY <= comp2->maxY &&
		comp1->maxY >= comp2->minY &&
		comp1->minZ <= comp2->maxZ &&
		comp1->maxZ >= comp2->minZ
		//comp1->minX <= comp2->maxX &&
		//comp1->maxX >= comp2->minX
		) {
		return 1;
	}
	return 0;
}

static void collide_check(simple_game_t* game, collider_component_t* player_col) {
	uint64_t k_query_mask = (1ULL << game->enemy_type) | (1ULL << game->collider_type);
	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query)) {
		collider_component_t* enemy_col = ecs_query_get_component(game->ecs, &query, game->collider_type);

		if (intersect(player_col, enemy_col)) {
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
		}
	}
}

static void update_players(simple_game_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type) | (1ULL << game->collider_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);
		collider_component_t* collide_comp = ecs_query_get_component(game->ecs, &query, game->collider_type);

		if (transform_comp->transform.translation.z > 1.0f)
		{
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
		}
		
		transform_t move;
		transform_identity(&move);
		if (key_mask & k_key_up)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -dt));
		}
		if (key_mask & k_key_down)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), dt));
		}
		if (key_mask & k_key_left)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dt));
		}
		if (key_mask & k_key_right)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dt));
		}
		transform_multiply(&transform_comp->transform, &move);
		collide_comp->minY = transform_comp->transform.translation.y - (collide_comp->width);
		collide_comp->minZ = transform_comp->transform.translation.z - (collide_comp->height);
		collide_comp->minX = transform_comp->transform.translation.x - (collide_comp->depth);
		collide_comp->maxY = transform_comp->transform.translation.y + (collide_comp->width);
		collide_comp->maxZ = transform_comp->transform.translation.z + (collide_comp->height);
		collide_comp->maxX = transform_comp->transform.translation.x + (collide_comp->depth);
		collide_check(game, collide_comp);
	}
}

static void draw_models(simple_game_t* game)
{
	uint64_t k_camera_query_mask = (1ULL << game->camera_type);
	for (ecs_query_t camera_query = ecs_query_create(game->ecs, k_camera_query_mask);
		ecs_query_is_valid(game->ecs, &camera_query);
		ecs_query_next(game->ecs, &camera_query))
	{
		camera_component_t* camera_comp = ecs_query_get_component(game->ecs, &camera_query, game->camera_type);

		uint64_t k_model_query_mask = (1ULL << game->transform_type) | (1ULL << game->model_type);
		for (ecs_query_t query = ecs_query_create(game->ecs, k_model_query_mask);
			ecs_query_is_valid(game->ecs, &query);
			ecs_query_next(game->ecs, &query))
		{
			transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
			model_component_t* model_comp = ecs_query_get_component(game->ecs, &query, game->model_type);
			ecs_entity_ref_t entity_ref = ecs_query_get_entity(game->ecs, &query);

			struct
			{
				mat4f_t projection;
				mat4f_t model;
				mat4f_t view;
			} uniform_data;
			uniform_data.projection = camera_comp->projection;
			uniform_data.view = camera_comp->view;
			transform_to_matrix(&transform_comp->transform, &uniform_data.model);
			gpu_uniform_buffer_info_t uniform_info = { .data = &uniform_data, sizeof(uniform_data) };

			render_push_model(game->render, &entity_ref, model_comp->mesh_info, model_comp->shader_info, &uniform_info);
		}
	}
}
