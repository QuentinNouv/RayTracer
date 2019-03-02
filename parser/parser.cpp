//
// Created by Quentin Nouvel on 2019-03-01.
//

#include <string>
#include <fstream>
#include "parser.h"
#include <vector>
#include <iostream>

bool startsWith(const std::string &mainStr, const std::string &toMatch){
	return mainStr.find(toMatch) == 0;
}

vec3 *ppmTab(char *filename){
	const float quantum = 1.f/256.f;
	int i, width,height,max_value;
	char format[8];
	FILE * f = fopen(filename,"r");
	if (!f){
		fprintf(stderr,"Cannot open file %s...\n",filename);
		exit(-1);
	}
	fscanf(f,"%s\n",format);
	assert( (format[0]=='P' && format[1]=='6'));  // check P3 format
	while(fgetc(f)=='#'){
		while(fgetc(f) != '\n'); // aller jusqu'a la fin de la ligne
	}
	fseek( f, -1, SEEK_CUR);
	fscanf(f,"%d %d\n", &width, &height);
	fscanf(f,"%d\n", &max_value);
	vec3 *image = static_cast<vec3 *>(malloc(sizeof(vec3) * width * height + 1));
	int size = width*height+1;
	for(i=1 ; i<size ; i++){
		int r,g,b;
		fscanf(f,"%d %d %d", &r, &g, &b);
		image[i].r = (float) r * quantum;
		image[i].g = (float) g * quantum;
		image[i].b = (float) b * quantum;
	}
	fclose(f);
	return image;
}

std::vector<Face> *facesParse(char* filename){
	//vec3 *ppm = ppmTab(const_cast<char *>("/Users/quentin/Documents/igtai/IGTAI-RayTracer/parser/Mini_Falcon_0.ppm"));
	//std::cout << ppm->length() << std::endl;
	const std::string spacer = " ";
	const std::string slash = "/";
	vec3 points[250010];
	//vec2 textures[250010];
	auto faces = new std::vector<Face>();
	std::ifstream file(filename);
	if (!file.is_open()){
		perror("erreur ouverture fichier");
	}
	std::string str;
	std::string token;
	size_t pos;
	Face face;
	int nbPoint = 1;
	//int nbTexture = 1;
	while (std::getline(file, str))
	{
		if (startsWith(str, "v ")) {
			vec3 point;
			//std::cout << nbPoint << " ";
			str.erase(0, 1 + spacer.length());
			pos = str.find(spacer);
			token = str.substr(0, pos);
			point.x = std::stof(token);
			//std::cout << token << " ";
			str.erase(0, pos + spacer.length());

			pos = str.find(spacer);
			token = str.substr(0, pos);
			point.y = std::stof(token);
			//std::cout << token << " ";
			str.erase(0, pos + spacer.length());

			pos = str.find(spacer);
			token = str.substr(0, pos);
			point.z = std::stof(token);
			//std::cout << token << std::endl;
			points[nbPoint++] = point;
		} else if (startsWith(str, "f ")){
			str.erase(0, 1 + spacer.length());
			pos = str.find(slash);
			token = str.substr(0, pos);
			face.a = points[std::stoi(token)];
			str.erase(0, pos + slash.length());
			pos = str.find(spacer);
			str.erase(0, pos + spacer.length());

			pos = str.find(slash);
			token = str.substr(0, pos);
			face.b = points[std::stoi(token)];
			str.erase(0, pos + slash.length());
			pos = str.find(spacer);
			str.erase(0, pos + spacer.length());

			pos = str.find(slash);
			token = str.substr(0, pos);
			face.c = points[std::stoi(token)];
			faces->push_back(face);
		}
		// Process str
	}
	return faces;
}