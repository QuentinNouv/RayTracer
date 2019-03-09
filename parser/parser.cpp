//
// Created by Quentin Nouvel on 2019-03-01.
//

#include <string>
#include "parser.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>

bool startsWith(const std::string &mainStr, const std::string &toMatch){
	return mainStr.find(toMatch) == 0;
}

std::vector<std::vector<vec3>*> *ppmTab(const std::string &fname, unsigned &s_width, unsigned &s_height) {
	std::ifstream inp(fname.c_str(), std::ios::in | std::ios::binary);
	unsigned int width, height, max_col_val, size;
	std::vector<std::vector<vec3>*> *ret;
	std::vector<vec3> *pixelLine;
	if (inp.is_open()) {
		std::string line;
		std::getline(inp, line);
		if (line != "P6") {
			std::cout << "Error. Unrecognized file format." << std::endl;
			perror("erreur : ppm");
		}
		std::getline(inp, line);
		while (line[0] == '#') {
			std::getline(inp, line);
		}
		std::stringstream dimensions(line);

		try {
			dimensions >> width;
			dimensions >> height;
		} catch (std::exception &e) {
			std::cout << "Header file format error. " << e.what() << std::endl;
			perror("fileformat");
			exit(-1);
		}

		std::getline(inp, line);
		std::stringstream max_val(line);
		try {
			max_val >> max_col_val;
		} catch (std::exception &e) {
			std::cout << "Header file format error. " << e.what() << std::endl;
			perror("erreur header file format");
			exit(-1);
		}

		s_width = width;
		s_height = height;

		size = width*height;

		ret = new std::vector<std::vector<vec3>*>;
		const float quantum = 1.f/256.f;

		char aux;
		for (unsigned int i = 0; i < height; ++i) {
			pixelLine = new std::vector<vec3>();
			for (unsigned j = 0; j < width; ++j) {
				pixelLine->push_back(vec3());
				inp.read(&aux, 1);
				pixelLine->at(j).r = ((unsigned char) aux) * quantum;
				inp.read(&aux, 1);
				pixelLine->at(j).g = ((unsigned char) aux) * quantum;
				inp.read(&aux, 1);
				pixelLine->at(j).b = ((unsigned char) aux) * quantum;
			}
			//std::reverse(pixelLine->begin(), pixelLine->end());
			ret->push_back(pixelLine);
		}
		std::reverse(ret->begin(), ret->end());
	} else {
		std::cout << "Error. Unable to open " << fname << std::endl;
		exit(-1);
	}
	inp.close();
	return ret;
}

Model *objParse(std::string filename, std::string texturename){
	//TODO free les bidules attribué  avec new:s
	auto *model = new Model();
	auto ppm = ppmTab(texturename, model->width, model->height);
	const std::string spacer = " ";
	const std::string slash = "/";
	std::vector<vec3> points;
	std::vector<vec2> vt;
	auto faces = new std::vector<Face>();
	std::ifstream file(filename);
	if (!file.is_open()){
		perror("erreur ouverture fichier");
	}
	vec3 point;
	vec2 coord;
	points.push_back(point);//pas d'index 0 dans un .obj c'est un peu du bricolage mais bon :d
	vt.push_back(coord);//pareil :d
	std::string str;
	std::string token;
	size_t pos;
	Face face;
	while (std::getline(file, str))
	{
		if (startsWith(str, "v ")) {
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
			points.push_back(point);
		} else if (startsWith(str, "f ")){
			str.erase(0, 1 + spacer.length());
			pos = str.find(slash);
			token = str.substr(0, pos);
			face.a.coord = points[std::stoi(token)];
			str.erase(0, pos + slash.length());
			pos = str.find(spacer);
			token = str.substr(0, pos);
			face.a.texture = vt[std::stoi(token)];
			str.erase(0, pos + spacer.length());

			pos = str.find(slash);
			token = str.substr(0, pos);
			face.b.coord = points[std::stoi(token)];
			str.erase(0, pos + slash.length());
			pos = str.find(spacer);
			token = str.substr(0, pos);
			face.b.texture = vt[std::stoi(token)];
			str.erase(0, pos + spacer.length());

			pos = str.find(slash);
			token = str.substr(0, pos);
			face.c.coord = points[std::stoi(token)];
			str.erase(0, pos + slash.length());
			pos = str.find(spacer);
			token = str.substr(0, pos);
			face.c.texture = vt[std::stoi(token)];
			faces->push_back(face);
		} else if (startsWith(str, "vt ")){
			str.erase(0, 2 + spacer.length());
			pos = str.find(spacer);
			token = str.substr(0, pos);
			coord.x = std::stof(token);
			str.erase(0, pos + spacer.length());

			pos = str.find(spacer);
			token = str.substr(0, pos);
			coord.y = std::stof(token);
			vt.push_back(coord);
		}
		// Process str
	}
	printf("%d\n", faces->size());
	model->faces = faces;
	model->textures = ppm;
	return model;
}