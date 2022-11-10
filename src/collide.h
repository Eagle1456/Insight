#include "transform.h"

// Collider object
typedef struct collide_t {
	float width;
	float height;
	float depth;
	float minY;
	float minZ;
	float maxY;
	float maxZ;
	float minX;
	float maxX;
} collide_t;

// Set the collider object using a transform
void set_collider(collide_t* collider, transform_t* transform);

// Check if two colliders are intersecting
int intersecting(collide_t* comp1, collide_t* comp2);