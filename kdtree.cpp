#include <iostream>
#include "kdtree.h"
#include "scene_types.h"


typedef struct s_kdtreeNode KdTreeNode;

struct s_kdtreeNode {
  bool leaf; //! is this node a leaf ?
  int axis;//! axis index of the split, if not leaf
  float split;//!position of the split
  int depth; //!depth in the tree
  std::vector<unsigned> objects;//! index of objects, if leaf
  KdTreeNode* left;//!ptr to left child
  KdTreeNode* right;//! ptr to right child
  vec3 min;//! min pos of node bounding box
  vec3 max;//! max pos of node bounding box
};

KdTreeNode * initNode(bool l, int a, int d) {
    auto *ret = new KdTreeNode();
    ret->leaf = l;
    ret->axis = a;
    ret->depth = d;
    ret->left = nullptr;
    ret->right = nullptr;
    return ret;
}

typedef struct s_stackNode {
    float tmin;
    float tmax;
    KdTreeNode *node;
} StackNode;

struct s_kdtree {
    int depthLimit;
    size_t objLimit;
    KdTreeNode *root;

    std::vector<unsigned> outOfTree;
    std::vector<unsigned> inTree;
};

void subdivide(Scene *scene, KdTree *tree, KdTreeNode *node);

KdTree*  initKdTree(Scene *scene) {
    auto tree = new KdTree;
    tree->root = initNode(false, 0, 0);
    tree->root->objects.clear();
    vec3 minAabb = vec3(0.f), maxAabb = vec3(0.f);
    unsigned long objectSize = scene->objects.size();
    vec3 p1, p2, p3;
    for (unsigned i = 0; i < objectSize; ++i){
        Geometry geom = scene->objects.at(i)->geom;
        switch (geom.type){
            case SPHERE:
                p1 = vec3(geom.sphere.center.x - geom.sphere.radius,
                            geom.sphere.center.y - geom.sphere.radius,
                            geom.sphere.center.z - geom.sphere.radius);
                p2 = vec3(geom.sphere.center.x + geom.sphere.radius,
                            geom.sphere.center.y + geom.sphere.radius,
                            geom.sphere.center.z + geom.sphere.radius);
                minAabb = glm::min(minAabb, p1);
                maxAabb = glm::max(maxAabb, p2);
				tree->root->objects.push_back(i);
				tree->inTree.push_back(i);
                break;
        	case TRIANGLE:
        		p1 = geom.triangle.a;
        		p2 = geom.triangle.b;
        		p3 = geom.triangle.c;
        		minAabb = glm::min(p1, glm::min(p2, glm::min(p3, minAabb)));
        		maxAabb = glm::max(p1, glm::max(p2, glm::max(p3, maxAabb)));
				tree->root->objects.push_back(i);
				tree->inTree.push_back(i);
        		break;
			case PLANE:
				tree->outOfTree.push_back(i);
				break;
			case KDFREE_SPHERE:
				tree->outOfTree.push_back(i);
				break;
            default:
                break;
        }
    }
    std::cout << "all object added";
    tree->root->min = minAabb;
    tree->root->max = maxAabb;
    tree->depthLimit = (int) ceil(log(objectSize));
    subdivide(scene, tree, tree->root);
    //!\todo compute scene bbox, store object in outOfTree or inTree depending on type
    return tree;

}

bool intersectPointAabb(vec3 point, vec3 aabbMin, vec3 aabbMax){
	return point.x >= aabbMin.x && point.y >= aabbMin.y &&
			point.z >= aabbMin.z && point.x <= aabbMax.x &&
			point.y <= aabbMax.y && point.z <= aabbMax.z;
}

//from http://blog.nuclex-games.com/tutorials/collision-detection/static-sphere-vs-aabb/
bool intersectSphereAabb(vec3 sphereCenter, float sphereRadius, vec3 aabbMin, vec3 aabbMax) {
    vec3 closestPointInAabb = min(max(sphereCenter, aabbMin), aabbMax);
    vec3 seg = closestPointInAabb -  sphereCenter;
    float distanceSquared = dot(seg, seg);
    // The AABB and the sphere overlap if the closest point within the rectangle is
    // within the sphere's radius
    return distanceSquared < (sphereRadius * sphereRadius);
}

// --------------------- Intersect Triangle Aabb Start ----------------------

//implementation of http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/tribox_tam.pdf

inline bool axisTestX01(float a, float b, float fa, float fb, const glm::vec3 &v0,
						const glm::vec3 &v2, const glm::vec3 &boxhalfsize, float &rad, float &min,
						float &max, float &p0, float &p2) {
	p0 = a * v0.y - b * v0.z;
	p2 = a * v2.y - b * v2.z;
	if (p0 < p2) {
		min = p0;
		max = p2;
	} else {
		min = p2;
		max = p0;
	}
	rad = fa * boxhalfsize.y + fb * boxhalfsize.z;
	if (min >= rad || max <= -rad)
		return false;
	return true;
}
inline bool axisTestX2(float a, float b, float fa, float fb, const glm::vec3 &v0,
					   const glm::vec3 &v1, const glm::vec3 &boxhalfsize, float &rad, float &min,
					   float &max, float &p0, float &p1) {
	p0 = a * v0.y - b * v0.z;
	p1 = a * v1.y - b * v1.z;
	if (p0 < p1) {
		min = p0;
		max = p1;
	} else {
		min = p1;
		max = p0;
	}
	rad = fa * boxhalfsize.y + fb * boxhalfsize.z;
	if (min >= rad || max <= -rad)
		return false;
	return true;
}

/*======================== Y-tests ========================*/

inline bool axisTestY02(float a, float b, float fa, float fb, const glm::vec3 &v0,
						const glm::vec3 &v2, const glm::vec3 &boxhalfsize, float &rad, float &min,
						float &max, float &p0, float &p2) {
	p0 = -a * v0.x + b * v0.z;
	p2 = -a * v2.x + b * v2.z;
	if (p0 < p2) {
		min = p0;
		max = p2;
	} else {
		min = p2;
		max = p0;
	}
	rad = fa * boxhalfsize.x + fb * boxhalfsize.z;
	if (min >= rad || max <= -rad)
		return false;
	return true;
}
/*======================== Z-tests ========================*/
inline bool axisTestZ12(float a, float b, float fa, float fb, const glm::vec3 &v2,
						const glm::vec3 &boxhalfsize, float &rad, float min, float max, float &p2) {
	p2 = a * v2.x - b * v2.y;
	rad = fa * boxhalfsize.x + fb * boxhalfsize.y;
	if (min >= rad || max <= -rad)
		return false;
	return true;
}

inline bool axisTestZ0(float a, float b, float fa, float fb, const glm::vec3 &v0,
					   const glm::vec3 &v1, const glm::vec3 &boxhalfsize, float &rad, float &min,
					   float &max, float &p0, float &p1) {
	p0 = a * v0.x - b * v0.y;
	p1 = a * v1.x - b * v1.y;
	if (p0 < p1) {
		min = p0;
		max = p1;
	} else {
		min = p1;
		max = p0;
	}
	rad = fa * boxhalfsize.x + fb * boxhalfsize.y;
	if (min >= rad || max <= -rad)
		return false;
	return true;
}

inline void findMinMax(float x0, float x1, float x2, float &min, float &max) {
	min = max = x0;
	if (x1 < min)
		min = x1;
	if (x1 > max)
		max = x1;
	if (x2 < min)
		min = x2;
	if (x2 > max)
		max = x2;
}

inline bool planeBoxOverlap(glm::vec3 normal, glm::vec3 vert, glm::vec3 maxbox) {
	glm::vec3 vmin, vmax;
	float v;
	for (size_t q = 0; q < 3; q++) {
		v = vert[q];
		if (normal[q] > 0.0f) {
			vmin[q] = -maxbox[q] - v;
			vmax[q] = maxbox[q] - v;
		} else {
			vmin[q] = maxbox[q] - v;
			vmax[q] = -maxbox[q] - v;
		}
	}
	if (glm::dot(normal, vmin) > 0.0f)
		return false;
	return glm::dot(normal, vmax) >= 0.0f;

}


bool intersectTriangleAabb(vec3 aabbMin, vec3 aabbMax, vec3 a, vec3 b, vec3 c){
	if (intersectPointAabb(a, aabbMin, aabbMax)) return true;
	if (intersectPointAabb(b, aabbMin, aabbMax)) return true;
	if (intersectPointAabb(c, aabbMin, aabbMax)) return true;
	vec3 halfSize = (aabbMax - aabbMin) * 0.5f;
	vec3 boxCenter = (aabbMax + aabbMin) * 0.5f;

	float min, max, p0, p1, p2, rad, fex, fey, fez;

	vec3 v0 = a - boxCenter;
	vec3 v1 = b - boxCenter;
	vec3 v2 = c - boxCenter;

	vec3 edge0 = v1 - v0;
	vec3 edge1 = v2 - v1;
	vec3 edge2 = v0 - v2;

	fex = fabsf(edge0.x);
	fey = fabsf(edge0.y);
	fez = fabsf(edge0.z);

	if (!axisTestX01(edge0.z, edge0.y, fez, fey, v0, v2, halfSize, rad, min, max, p0, p2)) {
		return false;
	}
	if (!axisTestY02(edge0.z, edge0.x, fez, fex, v0, v2, halfSize, rad, min, max, p0, p2)) {
		return false;
	}
	if (!axisTestZ0(edge0.y, edge0.x, fez, fex, v0, v1, halfSize, rad, min, max, p0, p1)) {
		return false;
	}

	fex = fabsf(edge1.x);
	fey = fabsf(edge1.y);
	fez = fabsf(edge1.z);

	if (!axisTestX01(edge1.z, edge1.y, fez, fey, v0, v2, halfSize, rad, min, max, p0, p2)) {
		return false;
	}
	if (!axisTestY02(edge1.z, edge1.x, fez, fex, v0, v2, halfSize, rad, min, max, p0, p2)) {
		return false;
	}
	if (!axisTestZ0(edge1.y, edge1.x, fez, fex, v0, v1, halfSize, rad, min, max, p0, p1)) {
		return false;
	}

	fex = fabsf(edge2.x);
	fey = fabsf(edge2.y);
	fez = fabsf(edge2.z);

	if (!axisTestX2(edge2.z, edge2.y, fez, fey, v0, v1, halfSize, rad, min, max, p0, p1)) {
		return false;
	}
	if (!axisTestY02(edge2.z, edge2.x, fez, fex, v0, v2, halfSize, rad, min, max, p0, p2)) {
		return false;
	}
	if (!axisTestZ12(edge2.y, edge2.x, fey, fex, v2, halfSize, rad, min, max, p2)) {
		return false;
	}

	findMinMax(v0.x, v1.x, v2.x, min, max);
	if (min >= halfSize.x || max <= -halfSize.x)
		return false;

	/* test in Y-direction */
	findMinMax(v0.y, v1.y, v2.y, min, max);
	if (min >= halfSize.y || max <= -halfSize.y)
		return false;

	/* test in Z-direction */
	findMinMax(v0.z, v1.z, v2.z, min, max);
	if (min >= halfSize.z || max <= -halfSize.z)
		return false;

	vec3 normal = cross(edge0, edge1);
	return planeBoxOverlap(normal, v0, halfSize);
}
// --------------------- Intersect Triangle Aabb end   ----------------------

void subdivide(Scene *scene, KdTree *tree, KdTreeNode *node) {
    size_t size = node->objects.size();
    if (node->depth >= tree->depthLimit || size == 1){
        node->leaf = true;
        return;
    }

	node->left  = initNode(false, (node->axis + 1) % 3, node->depth + 1);
	node->right = initNode(false, (node->axis + 1) % 3, node->depth + 1);

    node->left->min = node->min;
    node->right->min = node->min;
    node->left->max = node->max;
    node->right->max = node->max;

    switch (node->axis){
        case 0:
            node->split = (node->max.x-node->min.x)/2.f;
            node->left->max.x  -= node->split;
            node->right->min.x 	= node->left->max.x;
            break;
        case 1:
            node->split = (node->max.y-node->min.y)/2.f;
			node->left->max.y  -= node->split;
			node->right->min.y 	= node->left->max.y;
            break;
        case 2:
            node->split = (node->max.z-node->min.z)/2.f;
			node->left->max.z  -= node->split;
			node->right->min.z 	= node->left->max.z;
            break;
        default:
            perror("subdivide : axis error");
    }

    //for (unsigned i = 0; i < size; ++i){
    for (auto const &i : node->objects){
    	Object *object = scene->objects.at(i);
    	switch (object->geom.type){
			case SPHERE:
				if (intersectSphereAabb(object->geom.sphere.center,
										object->geom.sphere.radius, node->left->min,
										node->left->max)) {
					node->left->objects.push_back(i);
				}
				if (intersectSphereAabb(object->geom.sphere.center,
										object->geom.sphere.radius, node->right->min,
										node->right->max)) {
					node->right->objects.push_back(i);
				}
    			break;
    		case TRIANGLE:
				if (intersectTriangleAabb(node->left->min, node->left->max, object->geom.triangle.a,
										  object->geom.triangle.b, object->geom.triangle.c)) {
					node->left->objects.push_back(i);
				}
				if (intersectTriangleAabb(node->right->min, node->right->max, object->geom.triangle.a,
										  object->geom.triangle.b, object->geom.triangle.c)) {
					node->right->objects.push_back(i);
				}
    			//TODO intersect triangle
    			break;
			default:
				perror("Erreur subdivide");
		}
    }
    //node->objects.clear();
    //std::cout << "box " << node->depth << std::endl;
	subdivide(scene, tree, node->left);
	subdivide(scene, tree, node->right);

}

// from http://www.scratchapixel.com/lessons/3d-basic-lessons/lesson-7-intersecting-simple-shapes/ray-box-intersection/
static bool intersectAabb(Ray *theRay,  vec3 min, vec3 max) {
    float tmin, tmax, tymin, tymax, tzmin, tzmax;
    vec3 bounds[2] = {min, max};
    tmin = (bounds[theRay->sign[0]].x - theRay->orig.x) * theRay->invdir.x;
    tmax = (bounds[1-theRay->sign[0]].x - theRay->orig.x) * theRay->invdir.x;
    tymin = (bounds[theRay->sign[1]].y - theRay->orig.y) * theRay->invdir.y;
    tymax = (bounds[1-theRay->sign[1]].y - theRay->orig.y) * theRay->invdir.y;
    if ((tmin > tymax) || (tymin > tmax)) return false;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;
    tzmin = (bounds[theRay->sign[2]].z - theRay->orig.z) * theRay->invdir.z;
    tzmax = (bounds[1-theRay->sign[2]].z - theRay->orig.z) * theRay->invdir.z;
    if ((tmin > tzmax) || (tzmin > tmax)) return false;
    if (tzmin > tmin) tmin = tzmin;
    if (tzmax < tmax) tmax = tzmax;
    if (tmin > theRay->tmin) theRay->tmin = tmin;
    if (tmax < theRay->tmax) theRay->tmax = tmax;
    return true;
}

bool objIntersect(Ray *ray, Intersection *intersection, Object *object){
	switch (object->geom.type){
		case SPHERE:
			return intersectSphere(ray, intersection, object);
		case TRIANGLE:
			return intersectTriangle(ray, intersection, object);
		default:
			perror("objIntersect : unhandled object");
			break;
	}
	//shall never pass through
	return false;
}

inline void swap(float &a, float &b){
	float buffer = a;
	a = b;
	b = buffer;
}

bool traverse(Scene * scene, KdTree * tree,StackNode currentNode, Ray * ray, Intersection *intersection) {
	bool intersect = false;
	if (currentNode.node->leaf){
		for (auto const &index : currentNode.node->objects){
			Object *obj = scene->objects.at(index);
			intersect |= objIntersect(ray, intersection, obj);
		}
	} else {
		//Ray t1, t2;
		//bool intersectT1, intersectT2;
		StackNode node;
/*
		rayInit(&t1, ray->orig, ray->dir, currentNode.tmin, currentNode.tmax);
		rayInit(&t2, ray->orig, ray->dir, currentNode.tmin, currentNode.tmax);
		intersectT1 = intersectAabb(&t1, currentNode.node->left->min, currentNode.node->left->max);
		intersectT2 = intersectAabb(&t2, currentNode.node->right->min, currentNode.node->right->max);
		if (t1.tmin > t1.tmax) swap(t1.tmin, t1.tmax);
		if (t2.tmin > t2.tmax) swap(t2.tmin, t2.tmax);
		if (intersectT1 && intersectT2){
			if (t1.tmin < t2.tmin){
				node.node = currentNode.node->left;
				node.tmin = t1.tmin;
				node.tmax = t1.tmax;
				intersect |= traverse(scene, tree, node, ray, intersection);
				if (intersect){
					if (ray->tmax > t2.tmin){
						node.node = currentNode.node->right;
						node.tmin = t2.tmin;
						node.tmax = t2.tmax;
						intersect |= traverse(scene, tree, node, ray, intersection);
					}
				} else {
					node.node = currentNode.node->right;
					node.tmin = t2.tmin;
					node.tmax = t2.tmax;
					intersect |= traverse(scene, tree, node, ray, intersection);
				}
			} else {
				node.node = currentNode.node->right;
				node.tmin = t2.tmin;
				node.tmax = t2.tmax;
				intersect |= traverse(scene, tree, node, ray, intersection);
				if (intersect){
					if (ray->tmax > t1.tmin){
						node.node = currentNode.node->left;
						node.tmin = t1.tmin;
						node.tmax = t1.tmax;
						intersect |= traverse(scene, tree, node, ray, intersection);
					}
				} else {
					node.node = currentNode.node->left;
					node.tmin = t1.tmin;
					node.tmax = t1.tmax;
					intersect |= traverse(scene, tree, node, ray, intersection);
				}
			}
		} else if(intersectT1){
			node.node = currentNode.node->left;
			node.tmin = t1.tmin;
			node.tmax = t1.tmax;
			intersect |= traverse(scene, tree, node, ray, intersection);
		} else if (intersectT2){
			node.node = currentNode.node->right;
			node.tmin = t2.tmin;
			node.tmax = t2.tmax;
			intersect |= traverse(scene, tree, node, ray, intersection);
		}/*Devrais être plus rapide mais ne l'est pas ???????????*/


		Ray travers;
		rayInit(&travers, ray->orig, ray->dir, currentNode.tmin, currentNode.tmax);
		if (intersectAabb(&travers, currentNode.node->left->min, currentNode.node->left->max)){
			node.node = currentNode.node->left;
			node.tmin = travers.tmin;
			node.tmax = travers.tmax;
			intersect |= traverse(scene, tree, node, ray, intersection);
		}
		rayInit(&travers, ray->orig, ray->dir, currentNode.tmin, currentNode.tmax);
		if (intersectAabb(&travers, currentNode.node->right->min, currentNode.node->right->max)){
			node.node = currentNode.node->right;
			node.tmin = travers.tmin;
			node.tmax = travers.tmax;
			intersect |= traverse(scene, tree, node, ray, intersection);
		}/**/
	}
	return intersect;
}

bool intersectKdTree(Scene *scene, KdTree *tree, Ray *ray, Intersection *intersection) {
    bool hasIntersection = false;
    for (auto const &index : tree->outOfTree){
    	Object *obj = scene->objects.at(index);
    	switch (obj->geom.type){
    		case  PLANE:
    			hasIntersection |= intersectPlane(ray, intersection, obj);
    			break;
			case KDFREE_SPHERE:
				hasIntersection |= intersectSphere(ray, intersection, obj);
				break;
    		default:
    			//never used
    			perror("intersectKdTree : unhandled object found");
    	}
    }
    StackNode node;
    node.node = tree->root;

    hasIntersection |= traverse(scene, tree, node, ray, intersection);
    //free(&node);
    return hasIntersection;
}
