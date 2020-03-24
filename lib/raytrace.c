/*
 * main.c
 *
 *  Created on: Feb 9, 2014
 *      Author: jmiller
 */

#include "vec2.h"
#include "vec3.h"
#include "hit.h"
#include "shader.h"
#include "shape.h"
#include "light.h"
#include "ray.h"
#include "world.h"
#include "raytrace.h"

Vec3* shade(Ray* ray, World* world, Hit* hit, Vec3* color, int maxdepth) {
    if (hit->best) {
        Vec3 I, N;
        int i;
        float kt, kr;
        Shader* shader = hit->best->shader;
        vec3(0,0,0, color);
        addscaled3(&ray->point, hit->t, &ray->dir, &hit->point);
        hit->best->op->normal(hit->best, hit, &hit->normal);
        hit->best->op->uv(hit->best, hit, &hit->uv);
        copy3(&hit->normal, &N);
        copy3(&ray->dir, &I);
        reflectionDirection(&I, &N, &hit->reflect);
        for (i = 0; i < world->nLights; i++) {
            Vec3 tmpColor;
            Light* light = world->lights[i];
            light->op->makeRay(light, &hit->point, &hit->lightRay);
            addscaled3(&hit->lightRay.point, world->epsilon, &hit->lightRay.dir, &hit->lightRay.point);
            hit->inShadow = shadow(&hit->lightRay, hit->best, world);
            shader->op->evaluate(shader, hit, &tmpColor);
            multadd3(&tmpColor, &light->color, color, color); /* TODO: multiply by Light's ambient color */
        }
        kr = shader->op->getReflectionAmount(shader);
        if ((kr > 0.0f) && (maxdepth > 0)) {
            Vec3 reflectColor;
            Ray reflectedRay;
            copy3(&hit->point, &reflectedRay.point);
            copy3(&hit->reflect, &reflectedRay.dir);
            /* nudge the point to avoid self-intersection */
            addscaled3(&reflectedRay.point, world->epsilon, &reflectedRay.dir, &reflectedRay.point);
            trace(&reflectedRay, hit->best, world, &reflectColor, maxdepth - 1);
            addscaled3(color, kr, &reflectColor, color);
        }
        kt = shader->op->getTransmissionAmount(shader);
        if ((kt > 0.0f) && (maxdepth > 0)) {
            Vec3 refractedColor;
            Ray refractedRay;
            float i1, i2;
            copy3(&hit->point, &refractedRay.point);
            i1 = 1.0f;
            i2 = shader->op->getIndexOfRefraction(shader);
            if (dot3(&I, &N) >= 0.0f) { /* entering */
                float tmp = i1; i1 = i2; i2 = tmp;
                negate3(&N);
            }
            (void) transmisionDirection(i1, i2, &ray->dir, &N, &refractedRay.dir);
            /* nudge the point to avoid self-intersection */
            addscaled3(&refractedRay.point, world->epsilon, &refractedRay.dir, &refractedRay.point);
            trace(&refractedRay, 0 /* ignore */, world, &refractedColor, maxdepth - 1);
            addscaled3(color, kt, &refractedColor, color);
        }
    } else {
        copy3(&world->background, color);
    }
    return color;
}

void trace(Ray* ray, const Shape* ignore, World* world, Vec3* color, int maxdepth) {
    int s;
    Hit hit;
    clearHit(&hit);
    hit.ignore = ignore;
    for (s = 0; s < world->nShapes; s++) {
        Shape* shape = world->shapes[s];
        shape->op->intersect(shape, ray, &hit);
    }
    shade(ray, world, &hit, color, maxdepth);
}

int shadow(Ray* ray, const Shape* ignore, World* world) {
    int s;
    Hit hit;
    clearHit(&hit);
    hit.ignore = ignore;
    for (s = 0; s < world->nShapes; s++) {
        Shape* shape = world->shapes[s];
        if(shape->op->intersect(shape, ray, &hit) > 0)
            return 1;
    }
    return 0;
}
