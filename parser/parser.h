//
// Created by Quentin Nouvel on 2019-03-01.
//

#ifndef RAYTRACER_PARSER_H
#define RAYTRACER_PARSER_H

#include "../defines.h"
#include <vector>
#include <string>

typedef struct {vec3 coord; vec2 texture;} Sommet;
typedef struct {Sommet a; Sommet b; Sommet c;} Face;

typedef struct{
	std::vector<Face> *faces;
	std::vector<std::vector<vec3>*> *textures;
	unsigned int width;
	unsigned int height;
} Model;

std::vector<std::vector<vec3>*> *ppmTab(const std::string &fname, unsigned &s_width, unsigned &s_height);
Model *objParse(std::string filename, std::string texturename);


#endif //RAYTRACER_PARSER_H