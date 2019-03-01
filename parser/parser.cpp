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

std::vector<Face> *facesParse(char* filename){
	const std::string spacer = " ";
	const std::string slash = "/";
	vec3 points[250010];
	auto faces = new std::vector<Face>();
	std::ifstream file(filename);
	std::string str;
	std::string token;
	size_t pos;
	Face face;
	int nbPoint = 1;
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
			face.a = points[std::stoi(token)];
			str.erase(0, pos + slash.length());
			pos = str.find(spacer);
			str.erase(0, pos + spacer.length());

			pos = str.find(slash);
			token = str.substr(0, pos);
			face.a = points[std::stoi(token)];
			faces->push_back(face);
		}
		// Process str
	}
	return faces;
}