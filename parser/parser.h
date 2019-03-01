//
// Created by Quentin Nouvel on 2019-03-01.
//

#ifndef RAYTRACER_PARSER_H
#define RAYTRACER_PARSER_H

#include "../scene_types.h"

typedef struct {vec3 a; vec3 b; vec3 c;} Face;

std::vector<Face> *facesParse(char* filename);


#endif //RAYTRACER_PARSER_H
