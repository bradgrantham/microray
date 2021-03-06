#ifndef WORLD_H
#define WORLD_H

#include "vec3.h"

#define MAXSHAPES 100
#define MAXSHADERS 20
#define MAXLIGHTS 8

typedef struct World World;
typedef struct Shape Shape;
typedef struct Shader Shader;
typedef struct Light Light;
typedef struct Camera Camera;

struct World {
    Shape* shapes[MAXSHAPES];
    Shader* shaders[MAXSHADERS];
    const char* shaderNames[MAXSHADERS];
    Light* lights[MAXLIGHTS];
    Camera* camera;
    int nShapes;
    int nShaders;
    int nLights;
    Vec3 background; /* background colors */
    float epsilon; /* intersection slop */
    int width; /* image width */
    int height; /* image height */
    int depth; /* image depth */
    int raydepth;
};

World* createWorld();

#endif /* WORLD_H */

