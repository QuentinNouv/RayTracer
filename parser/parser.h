//
// Created by Quentin Nouvel on 2019-03-01.
//

#ifndef RAYTRACER_PARSER_H
#define RAYTRACER_PARSER_H

#include "../defines.h"
#include <vector>

typedef struct {vec3 coord; vec2 texture;} Sommet;
typedef struct {Sommet a; Sommet b; Sommet c;} Face;

typedef struct{
	std::vector<Face> *faces;
	std::vector<std::vector<vec3>*> *textures;
	unsigned int width;
	unsigned int height;
} Model;


Model *objParse(std::string filename, std::string texturename);


#endif //RAYTRACER_PARSER_H