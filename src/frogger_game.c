#include "frogger_game.h"
#include "transform.h"
#include "render.h"
#include "gpu.h"
#include "fs.h"
#include "timer.h"
#include "timer_object.h"
#include "input.h"
#include "ecs.h"
#include "heap.h"
#include "wm.h"
#include "collide.h"
#include "string.h"

typedef struct transform_component_t
{
	transform_t transform;
} transform_component_t;

typedef struct camera_component_t
{
	mat4f_t projection;
	mat4f_t view;
} camera_component_t;

typedef struct player_component_t 
{
	float player_speed;
} player_component_t;

typedef struct model_component_t
{
	gpu_mesh_info_t* mesh_info;
	gpu_shader_info_t* shader_info;
} model_component_t;

typedef struct enemy_component_t
{
	int index;
	int row;
	float speed;
	bool respawning;
} enemy_component_t;

typedef struct name_component_t
{
	char name[32];
} name_component_t;

typedef struct collider_component_t {
	collide_t collider;
} collider_component_t;

typedef struct frogger_t
{
	heap_t* heap;
	fs_t* fs;
	wm_window_t* window;
	render_t* render;
	input_t* input;
	timer_object_t* timer;

	ecs_t* ecs;
	int transform_type;
	int camera_type;
	int model_type;
	int player_type;
	int name_type;
	int collider_type;
	int enemy_type;
	bool playerRespawning;
	ecs_entity_ref_t player_ent;
	ecs_entity_ref_t camera_ent;
	ecs_entity_ref_t enemy_ent[3][5];

	gpu_mesh_info_t cube_mesh;
	gpu_mesh_info_t rect_mesh;
	gpu_shader_info_t cube_shader;
	fs_work_t* vertex_shader_work;
	fs_work_t* fragment_shader_work;
} frogger_t;

static void load_resources(frogger_t* game);
static void unload_resources(frogger_t* game);
static void spawn_player(frogger_t* game);
static void spawn_enemy(frogger_t* game, int index, int row, bool respawn);
static void spawn_camera(frogger_t* game);
static void update_players(frogger_t* game);
static void update_enemies(frogger_t* game);
static void draw_models(frogger_t* game);

frogger_t* frogger_create(heap_t* heap, fs_t* fs, wm_window_t* window, render_t* render, input_t* input)
{
	frogger_t* game = heap_alloc(heap, sizeof(frogger_t), 8);
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

	game->playerRespawning = false;
	load_resources(game);
	spawn_player(game);
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 4; j++) {
			spawn_enemy(game, j, i, false);
		}
	}
	spawn_camera(game);

	game->input = input;
	return game;
}

void frogger_destroy(frogger_t* game)
{
	ecs_destroy(game->ecs);
	timer_object_destroy(game->timer);
	unload_resources(game);
	heap_free(game->heap, game);
}

void frogger_update(frogger_t* game)
{
	if (game->playerRespawning) {
		spawn_player(game);
		game->playerRespawning = false;
	}
	timer_object_update(game->timer);
	ecs_update(game->ecs);
	update_players(game);
	update_enemies(game);
	draw_models(game);
	render_push_done(game->render);
}

static void load_resources(frogger_t* game)
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
		{  1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
		{ -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f,  0.0f },
	};
	
	static vec3f_t rect_verts[] =
	{
		{ -1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{ -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f,  0.0f },
		{ -1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f,  0.0f },
		{  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f,  0.0f },
		{  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f,  0.0f },
		{ -1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f,  0.0f },
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

	game->rect_mesh = (gpu_mesh_info_t)
	{
		.layout = k_gpu_mesh_layout_tri_p444_c444_i2,
		.vertex_data = rect_verts,
		.vertex_data_size = sizeof(rect_verts),
		.index_data = cube_indices,
		.index_data_size = sizeof(cube_indices),
	};
}

static void unload_resources(frogger_t* game)
{
	fs_work_destroy(game->fragment_shader_work);
	fs_work_destroy(game->vertex_shader_work);
}

static void set_enemy(int row, int index, float* zpos, float* ypos, float* scale, float* speed) {
	switch (row) {
	case 2:
		*zpos = -6.0f;
		*scale = 2.0f;
		*speed = 3.0f;
		break;
	case 1:
		*zpos = 0.0f;
		*scale = 2.4f;
		*speed = 1.0f;
		break;
	default:
		*zpos = 5.0f;
		*scale = 1.5f;
		*speed = 1.5f;
	}

	switch (index)
	{
	case 4:
		*ypos = 0.0f;
		break;
	case 3:
		*ypos = -14.0f;
		break;
	case 2:
		*ypos = -7.0f;
		break;
	case 1:
		*ypos = 7.0f;
		break;
	default:
		*ypos = 14.0f;
	}
}

static void spawn_player(frogger_t* game)
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
	transform_comp->transform.translation.z = 7.5f;


	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "player");

	player_component_t* player_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->player_type, true);
	player_comp->player_speed = 5.0f;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->model_type, true);
	model_comp->mesh_info = &game->cube_mesh;
	model_comp->shader_info = &game->cube_shader;

	collider_component_t* collide_comp = ecs_entity_get_component(game->ecs, game->player_ent, game->collider_type, true);
	set_collider(&collide_comp->collider, &transform_comp->transform);
}

static void spawn_enemy(frogger_t* game, int index, int row, bool respawn)
{
	uint64_t k_player_ent_mask =
		(1ULL << game->transform_type) |
		(1ULL << game->model_type) |
		(1ULL << game->enemy_type) |
		(1ULL << game->name_type) |
		(1ULL << game->collider_type);
	game->enemy_ent[row][index] = ecs_entity_add(game->ecs, k_player_ent_mask);

	transform_component_t* transform_comp = ecs_entity_get_component(game->ecs, game->enemy_ent[row][index], game->transform_type, true);
	transform_identity(&transform_comp->transform);
	float zposition;
	float yposition;
	float scale;
	float speed;
	set_enemy(row, index, &zposition, &yposition, &scale, &speed);
	
	if (respawn) {
		yposition = 16.8f;
	}

	transform_comp->transform.translation = (vec3f_t){.x = 0, .y = yposition, .z = zposition};
	transform_comp->transform.scale.y = scale;

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->enemy_ent[row][index], game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "enemy");

	enemy_component_t* enemy_comp = ecs_entity_get_component(game->ecs, game->enemy_ent[row][index], game->enemy_type, true);
	enemy_comp->index = index;
	enemy_comp->row = row;
	enemy_comp->speed = speed;

	model_component_t* model_comp = ecs_entity_get_component(game->ecs, game->enemy_ent[row][index], game->model_type, true);
	model_comp->mesh_info = &game->rect_mesh;
	model_comp->shader_info = &game->cube_shader;

	collider_component_t* collide_comp = ecs_entity_get_component(game->ecs, game->enemy_ent[row][index], game->collider_type, true);
	set_collider(&collide_comp->collider, &transform_comp->transform);
}


static void spawn_camera(frogger_t* game)
{
	uint64_t k_camera_ent_mask =
		(1ULL << game->camera_type) |
		(1ULL << game->name_type);
	game->camera_ent = ecs_entity_add(game->ecs, k_camera_ent_mask);

	name_component_t* name_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->name_type, true);
	strcpy_s(name_comp->name, sizeof(name_comp->name), "camera");

	camera_component_t* camera_comp = ecs_entity_get_component(game->ecs, game->camera_ent, game->camera_type, true);
	mat4f_make_orthographic(&camera_comp->projection, -16.0f, 16.0f, -9.0f, 9.0f, 0.1f, 100.0f);

	vec3f_t eye_pos = (vec3f_t){ .x = 5.0f, .y = 0.0f, .z = 0.0f };
	vec3f_t forward = vec3f_scale(vec3f_forward(), -1.0f);
	vec3f_t up = vec3f_scale(vec3f_up(), -1.0f);
	mat4f_make_lookat(&camera_comp->view, &eye_pos, &forward, &up);
}

static bool collide_check(frogger_t* game, collider_component_t* player_col) {
	uint64_t k_query_mask = (1ULL << game->enemy_type) | (1ULL << game->collider_type);
	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query)) {
		collider_component_t* enemy_col = ecs_query_get_component(game->ecs, &query, game->collider_type);

		if (intersecting(&player_col->collider, &enemy_col->collider)) {
			return true;
		}
	}
	return false;
}

static void update_enemies(frogger_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;

	uint32_t key_mask = wm_get_key_mask(game->window);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->enemy_type) | (1ULL << game->collider_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		enemy_component_t* enemy_comp = ecs_query_get_component(game->ecs, &query, game->enemy_type);
		collider_component_t* collide_comp = ecs_query_get_component(game->ecs, &query, game->collider_type);

		float enemy_speed = enemy_comp->speed;
		float dist = dt * -enemy_speed;
		if (transform_comp->transform.translation.y < -16.8f)
		{
			int row = enemy_comp->row;
			int index = enemy_comp->index;
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
			spawn_enemy(game, index, row, true);
		}

		transform_t move;
		transform_identity(&move);
		move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dist));
		transform_multiply(&transform_comp->transform, &move);
		set_collider(&collide_comp->collider, &transform_comp->transform);
	}
}

static void update_players(frogger_t* game)
{
	float dt = (float)timer_object_get_delta_ms(game->timer) * 0.001f;
	float end_dist = -8.0f;

	uint32_t key_mask = input_get_key_mask(game->input);

	uint64_t k_query_mask = (1ULL << game->transform_type) | (1ULL << game->player_type) | (1ULL << game->collider_type);

	for (ecs_query_t query = ecs_query_create(game->ecs, k_query_mask);
		ecs_query_is_valid(game->ecs, &query);
		ecs_query_next(game->ecs, &query))
	{
		
		transform_component_t* transform_comp = ecs_query_get_component(game->ecs, &query, game->transform_type);
		player_component_t* player_comp = ecs_query_get_component(game->ecs, &query, game->player_type);
		collider_component_t* collide_comp = ecs_query_get_component(game->ecs, &query, game->collider_type);

		float player_speed = player_comp->player_speed;
		float dist = dt * player_speed;
		transform_t move;
		transform_identity(&move);
		if (key_mask & k_button_up)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), -dist));
		}
		if (key_mask & k_button_down)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_up(), dist));
		}
		if (key_mask & k_button_left)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), -dist));
		}
		if (key_mask & k_button_right)
		{
			move.translation = vec3f_add(move.translation, vec3f_scale(vec3f_right(), dist));
		}
		transform_multiply(&transform_comp->transform, &move);
		set_collider(&collide_comp->collider, &transform_comp->transform);
		if (transform_comp->transform.translation.z < end_dist || collide_check(game, collide_comp)) {
			ecs_entity_remove(game->ecs, ecs_query_get_entity(game->ecs, &query), false);
			game->playerRespawning = true;
		}
	}
}

static void draw_models(frogger_t* game)
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
