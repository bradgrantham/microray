#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#ifdef PROFILE
#include <time.h>
#endif
#include "bvh.h"
#include "branch.h"
#include "leaf.h"
#include "util.h"

#define MAX_MAX_BVH_DEPTH 16
#define MAX_LEAF_SIZE_COUNT 16

static int bvh_max_depth = 8;
static unsigned int bvh_leaf_max = 4;

static void initialize_BVH_parameters()
{
    if(getenv("BVH_MAX_DEPTH") != 0) {
        bvh_max_depth = atoi(getenv("BVH_MAX_DEPTH"));
        if(bvh_max_depth > MAX_MAX_BVH_DEPTH)
            bvh_max_depth = MAX_MAX_BVH_DEPTH;
        fprintf(stderr, "BVH max depth set to %d\n", bvh_max_depth);
    }
    if(getenv("BVH_LEAF_MAX") != 0) {
        bvh_leaf_max = atoi(getenv("BVH_LEAF_MAX"));
        fprintf(stderr, "BVH max shapes per leaf set to %u\n", bvh_leaf_max);
    }
}

#ifdef PROFILE
static int total_treed = 0;
static time_t previous;
static int bvh_level_counts[MAX_MAX_BVH_DEPTH];
/* Each element N of this array is the number of leafs with N children */
static int bvh_leaf_size_counts[MAX_LEAF_SIZE_COUNT + 1];
static int bvh_node_count = 0;
static int bvh_leaf_count = 0;

void print_tree_stats()
{
    fprintf(stderr, "%d bvh nodes\n", bvh_node_count);
    fprintf(stderr, "%d of those are leaves\n", bvh_leaf_count);
    for(int i = 0; i < bvh_max_depth + 1; i++) {
        fprintf(stderr, "bvh level %2d: %6d nodes\n", i, bvh_level_counts[i]);
    }
    int largest_leaf_count = MAX_LEAF_SIZE_COUNT;
    while((largest_leaf_count > 0) && (bvh_leaf_size_counts[largest_leaf_count]) == 0)
        largest_leaf_count--;

    for(int i = 0; i <= largest_leaf_count; i++) {
        fprintf(stderr, "%2d shapes in %6d leaves\n", i, bvh_leaf_size_counts[i]);
    }
    if(bvh_leaf_size_counts[MAX_LEAF_SIZE_COUNT] > 0)
        fprintf(stderr, "%d or more shapes in %6d leaves\n", MAX_LEAF_SIZE_COUNT, bvh_leaf_size_counts[MAX_LEAF_SIZE_COUNT]);
}
#endif /* PROFILE */

Vec3 sort_dir;
int shape_sort(const void *p1, const void *p2)
{
    int result;

    Shape* s1 = *(Shape**)p1;
    Shape* s2 = *(Shape**)p2;

    Vec3 s1boxmin;
    Vec3 s1boxmax;
    Vec3 s1center;
    Vec3 s2boxmin;
    Vec3 s2boxmax;
    Vec3 s2center;
    Vec3 diff;
	float dot;
    
    s1->op->bounds(s1, &s1boxmin, &s1boxmax);
    add3(&s1boxmin, &s1boxmax, &s1center);
    mult3(&s1center, .5f, &s1center);

    s2->op->bounds(s2, &s2boxmin, &s2boxmax);
    add3(&s2boxmin, &s2boxmax, &s2center);
    mult3(&s2center, .5f, &s2center);

    sub3(&s2center, &s1center, &diff);

    dot = dot3(&diff, &sort_dir);

    if(dot < 0)
        result = 1; /* s1 should be after s2 */
    else if(dot > 0)
        result = -1; /* s1 should be before s2 */
    else
        result = 0; /* s1 is at s2 */

    return result;
}

Shape* make_tree(Shape** shapes, int nShapes, int level)
{
    Vec3 boxmin, boxmax, boxdim;
    Vec3 split_plane_normal;
	int i, startA, countA, startB, countB;
    Shape *g;

#ifdef PROFILE
    if(level == 0) {
        previous = time(NULL);
    }
#endif /* PROFILE */

#ifdef PROFILE
    if(time(NULL) > previous) {
        fprintf(stderr, "total treed = %d\n", total_treed);
        previous = time(NULL);
    }
#endif /* PROFILE */

    if((level >= bvh_max_depth) || nShapes <= bvh_leaf_max) {
        Shape* s = createLeaf(shapes, nShapes);
#ifdef PROFILE
        total_treed += nShapes;
        bvh_leaf_size_counts[(nShapes > MAX_LEAF_SIZE_COUNT) ? MAX_LEAF_SIZE_COUNT : nShapes]++;
        bvh_leaf_count++;
        bvh_level_counts[level]++;
        bvh_node_count++;
#endif /* PROFILE */
        return s;
    }

    /* find bounding box of all children */
    vec3(FLT_MAX, FLT_MAX, FLT_MAX, &boxmin);
    vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX, &boxmax);

    for(i = 0; i < nShapes; i++) {
        /* extend box by child box */
        Vec3 childmax;
        Vec3 childmin;

        shapes[i]->op->bounds(shapes[i], &childmin, &childmax);

        boxmin.x = fminf(boxmin.x, childmin.x);
        boxmin.y = fminf(boxmin.y, childmin.y);
        boxmin.z = fminf(boxmin.z, childmin.z);
        boxmax.x = fmaxf(boxmax.x, childmax.x);
        boxmax.y = fmaxf(boxmax.y, childmax.y);
        boxmax.z = fmaxf(boxmax.z, childmax.z);
    }

    sub3(&boxmax, &boxmin, &boxdim);

    if(boxdim.x > boxdim.y && boxdim.x > boxdim.z) {
        vec3(1, 0, 0, &split_plane_normal);
    } else if(boxdim.y > boxdim.z) {
        vec3(0, 1, 0, &split_plane_normal);
    } else {
        vec3(0, 0, 1, &split_plane_normal);
    }

    sort_dir = split_plane_normal;
    qsort(shapes, nShapes, sizeof(Shape*), shape_sort);

    startA = 0;
    countA = nShapes / 2;
    startB = startA + countA;
    countB = nShapes - countA;

    if(countA > 0 && countB > 0) {

        /* construct children */
        Shape *g1 = make_tree(shapes + startA, countA, level + 1);
        Shape *g2 = make_tree(shapes + startB, countB, level + 1);
        g = createBranch(g1, g2, &split_plane_normal);

    } else {

        g = createLeaf(shapes, nShapes);

#ifdef PROFILE
        total_treed += nShapes;
        bvh_leaf_size_counts[(nShapes > MAX_LEAF_SIZE_COUNT) ? MAX_LEAF_SIZE_COUNT : nShapes]++;
        bvh_leaf_count++;
#endif /* PROFILE */
    }

#ifdef PROFILE
        bvh_level_counts[level]++;
        bvh_node_count++;
#endif /* PROFILE */

    return g;
}

Shape *createBVH(Shape** shapes, int nShapes)
{
    Shape* s; 

	initialize_BVH_parameters();

    s = make_tree(shapes, nShapes, 0);

#ifdef PROFILE
    print_tree_stats();
#endif /* PROFILE */

    return s;
}

