#include "collide.h"



void set_collider(collide_t* collider, transform_t* transform) {
	float height = transform->scale.z;
	float width = transform->scale.y;
	float depth = transform->scale.x;

	collider->height = height;
	collider->width = width;
	collider->depth = depth;
	collider->minY = transform->translation.y - width;
	collider->minZ = transform->translation.z - height;
	collider->minX = transform->translation.x - depth;
	collider->maxY = transform->translation.y + width;
	collider->maxZ = transform->translation.z + height;
	collider->maxX = transform->translation.x + depth;
}

int intersecting(collide_t* comp1, collide_t* comp2) {
	if (comp1->minY <= comp2->maxY &&
		comp1->maxY >= comp2->minY &&
		comp1->minZ <= comp2->maxZ &&
		comp1->maxZ >= comp2->minZ
		) {
		return 1;
	}
	return 0;
}