#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif


#include "image.h"
#include "kdtree.h"
#include "scene_types.h"
#include <glm/gtx/rotate_vector.hpp>


#include <iostream>
#include <omp.h>


#define MAX_DEPTH 5
float ALIAS = 1.f;

/// acne_eps is a small constant used to prevent acne when computing
/// intersection
//  or boucing (add this amount to the position before casting a new ray !
const float acne_eps = 1e-4;

bool intersectPlane(Ray *ray, Intersection *intersection, Object *obj){
	float div = dot(ray->dir, obj->geom.plane.normal);
	if (div == 0) return false;
	float t = -(dot(ray->orig, obj->geom.plane.normal) + obj->geom.plane.dist)/ div;
	if (!(t > ray->tmin && t < ray->tmax)) return false;
	intersection->position = ray->orig + ray->dir * t;
	intersection->normal = obj->geom.plane.normal;
	intersection->obj = obj;
	ray->tmax = t;
	return true;
}

float chooseT(const float *t){
	if (t[0] < t[1]) {
		if (t[0] < 0.f) {
			return t[1]; // t[1] solution
		} else {
			return t[0];// t[0] solution
		}

	} else {
		if (t[1] < 0.f) {
			return t[1]; // t[1] solution
		} else {
			return t[0];// t[0] solution
		}
	}
}

bool intersectSphere(Ray *ray, Intersection *intersection, Object *obj){
	vec3 vecRay = ray->orig - obj->geom.sphere.center;
	float solution;
	float b = 2.f * dot(ray->dir, vecRay), c = dot(vecRay, vecRay) - pow(obj->geom.sphere.radius, 2);
	float delta = pow(b, 2.f) - (4.f * c);//TODO Si origine dans sphère issou
	if (delta ==  0.f){
		solution = -(b/2.f);
	} else if (delta > 0.f){
		float sqrtd = sqrt(delta);
		float t[2] = {(-b-sqrtd)/2, (-b+sqrtd)/2};
		if (t[0] < 0. && t[1] < 0.){
			return false;
		}
		solution = chooseT(t);
	} else return false;
	if (!(ray->tmin <=  solution && solution <= ray->tmax)) return false;
	intersection->position = ray->orig + ray->dir * solution;// vecteur * t
	intersection->obj = obj;
	intersection->normal = normalize(intersection->position - obj->geom.sphere.center);
	ray->tmax = solution;
	return true;
}

bool intersectTriangle(Ray *ray, Intersection *intersection, Object *obj) {
	vec3 a = obj->geom.triangle.a;
	vec3 b = obj->geom.triangle.b;
	vec3 c = obj->geom.triangle.c;

	vec3 ab = b-a;
	vec3 ac = c-a;

	vec3 pvec = glm::cross(ray->dir,ac);
	float det = glm::dot(pvec,ab);

	if(det == 0.f) return false;

	vec3 tvec = ray->orig - a;

	float u = glm::dot(pvec,tvec)/det;
	if (u < 0 || u > 1) return false;

	vec3 qvec = glm::cross(tvec,ab);

	float v = glm::dot(ray->dir,qvec)/det;
	if (v < 0 || u + v > 1) return false;

	float t = glm::dot(ac,qvec)/det;


	if(t<ray->tmin || t>ray->tmax) return false;

	ray->tmax = t;
	intersection->position = rayAt(*ray,t);
	intersection->obj = obj;
	intersection->normal = obj->geom.triangle.normal;

	return true;
}

bool intersectScene(const Scene *scene, Ray *ray, Intersection *intersection){
	bool hasIntersection = false;
	for(auto const &object : scene->objects){
		switch (object->geom.type){
			case PLANE:
				hasIntersection |= intersectPlane(ray, intersection, object);
				break;
			case KDFREE_SPHERE:
			case SPHERE:
				hasIntersection |= intersectSphere(ray, intersection, object);
				break;
			case TRIANGLE:
				hasIntersection |= intersectTriangle(ray, intersection, object);
				break;
		}
	}

	return hasIntersection;
}

/* ---------------------------------------------------------------------------
 */
/*
 *	The following functions are coded from Cook-Torrance bsdf model
 *description and are suitable only
 *  for rough dielectrics material (RDM. Code has been validated with Mitsuba
 *renderer)
 */

// Shadowing and masking function. Linked with the NDF. Here, Smith function,
// suitable for Beckmann NDF
float RDM_chiplus(float c){
	return (c > 0.f) ? 1.f : 0.f;
}

/** Normal Distribution Function : Beckmann
 * NdotH : Norm . Half
 */
float RDM_Beckmann(float NdotH, float alpha){
	if (NdotH <= 0.f) return 0.f;
	float alphaSq = pow(alpha, 2.f);
	float NdotHSq = pow(NdotH, 2.f);
	return exp(-((1.f - NdotHSq) / NdotHSq) / alphaSq) / (pi<float>() * alphaSq * pow(NdotHSq, 2.f));
}

// Fresnel term computation. Implantation of the exact computation. we can use
// the Schlick approximation
// LdotH : Light . Half
float RDM_Fresnel(float LdotH, float extIOR, float intIOR){
	float sin2Ot = pow(extIOR/intIOR, 2.f) * (1.f - pow(LdotH, 2.f));
	if (sin2Ot > 1.f) return 1.f;
	float cosOt = sqrt(1.f - sin2Ot);
	float Rs = pow(extIOR * LdotH - intIOR * cosOt, 2.f) / pow(extIOR * LdotH + intIOR * cosOt, 2.f);
	float Rp = pow(extIOR * cosOt - intIOR * LdotH, 2.f) / pow(extIOR * cosOt + intIOR * LdotH, 2.f);
	float ret = (Rs + Rp)/2.f;
	/*float r0 = pow((extIOR-intIOR)/(extIOR+intIOR), 2.f);
	float ret = r0 + (1.f - r0)*pow((1.f - LdotH), 5.f);*/
	return clamp(ret, 0.f, 1.f);
}

// DdotH : Dir . Half
// HdotN : Half . Norm
float RDM_G1(float DdotH, float DdotN, float alpha){
	float k = DdotH/DdotN;
	float b = 1.f / (alpha * (sqrt(1.f - pow(DdotN, 2.f))/DdotN));
	if (b >= 1.6f || b != b) return RDM_chiplus(k); // b!=b true si b = NaN
	float bSq = pow(b, 2.f);
	return k >= 0.f ? (3.535f * b + 2.181f * bSq) / (1.f + 2.276f * b + 2.577f * bSq) : 0.f;

}

// LdotH : Light . Half
// LdotN : Light . Norm
// VdotH : View . Half
// VdotN : View . Norm
float RDM_Smith(float LdotH, float LdotN, float VdotH, float VdotN, float alpha){
	return RDM_G1(LdotH, LdotN, alpha) * RDM_G1(VdotH, VdotN, alpha);
}

// Specular term of the Cook-torrance bsdf
// LdotH : Light . Half
// NdotH : Norm . Half
// VdotH : View . Half
// LdotN : Light . Norm
// VdotN : View . Norm
color3 RDM_bsdf_s(float LdotH, float NdotH, float VdotH, float LdotN, float VdotN, Material *m){
	float D = RDM_Beckmann(NdotH, m->roughness);
	float F = RDM_Fresnel(LdotH, 1.0f, m->IOR);
	float G = RDM_Smith(LdotH, LdotN, VdotH, VdotN, m->roughness);
	//! \todo specular term of the bsdf, using D = RDB_Beckmann, F = RDM_Fresnel, G
	//! = RDM_Smith
	return m->specularColor * ((D * F * G) / (4.f * LdotN * VdotN));

}

// diffuse term of the cook torrance bsdf
color3 RDM_bsdf_d(Material *m){
	return m->diffuseColor/pi<float>();
}

// The full evaluation of bsdf(wi, wo) * cos (thetai)
// LdotH : Light . Half
// NdotH : Norm . Half
// VdotH : View . Half
// LdotN : Light . Norm
// VdtoN : View . Norm
// compute bsdf * cos(Oi)
color3 RDM_bsdf(float LdotH, float NdotH, float VdotH, float LdotN, float VdotN, Material *m){
	return RDM_bsdf_d(m) + RDM_bsdf_s(LdotH, NdotH, VdotH, LdotN, VdotN, m);
}


color3 shade(vec3 n, vec3 v, vec3 l, color3 lc, Intersection *intersection){
	//float cosO = dot(l, n);
	//if (cosO <= 0.f) return vec3(0.f);
	const auto h = normalize(v+l);
	const float LdotH = dot(l, h),
				VdotH = dot(v, h);
	float 		NdotH = dot(n, h),
				LdotN = dot(l, n),
				VdotN = dot(v, n);

	color3 bsdf;

	if (intersection->obj->mat.hasTexture){
		switch (intersection->obj->mat.type){
			case TRIANGLE:
				{
					Material matTexture;
					matTexture.IOR = intersection->obj->mat.IOR;
					matTexture.roughness = intersection->obj->mat.roughness;

					//float pu = (intersection->obj->mat.coord.coordA.x + intersection->obj->mat.coord.coordB.x + intersection->obj->mat.coord.coordC.x)/3.f;
					//float pv = (intersection->obj->mat.coord.coordA.y + intersection->obj->mat.coord.coordB.y + intersection->obj->mat.coord.coordC.y)/3.f;

					point3 inter = intersection->position;
					point3 A = intersection->obj->geom.triangle.a;
					point3 B = intersection->obj->geom.triangle.b;
					point3 C = intersection->obj->geom.triangle.c;
					auto distA = static_cast<float>(pow(inter.x - A.x, 2) + pow(inter.y - A.y, 2) + pow(inter.z - A.z, 2));
					auto distB = static_cast<float>(pow(inter.x - B.x, 2) + pow(inter.y - B.y, 2) + pow(inter.z - B.z, 2));
					auto distC = static_cast<float>(pow(inter.x - C.x, 2) + pow(inter.y - C.y, 2) + pow(inter.z - C.z, 2));
					float normalDiv = (distA + distB + distC);

					distA /= normalDiv;
					distB /= normalDiv;
					distC /= normalDiv;

					float detier = 0.6666666f;

					distA = detier - distA;
					distB = detier - distB;
					distC = detier - distC;

					vec2 coordA = intersection->obj->mat.coord.coordA;
					vec2 coordB = intersection->obj->mat.coord.coordB;
					vec2 coordC = intersection->obj->mat.coord.coordC;

					vec2 coord = vec2(coordA.x * distA + coordB.x * distB + coordC.x * distC,
									  coordA.y * distA + coordB.y * distB + coordC.y * distC);

					//auto uU = static_cast<unsigned long>(intersection->obj->mat.model->width * pu); coordonnées du milieu de la texture à projeter

					//auto uV = static_cast<unsigned long>(intersection->obj->mat.model->height * pv);
					auto uUInter = static_cast<unsigned long>(intersection->obj->mat.model->width * coord.x);
					auto uVInter = static_cast<unsigned long>(intersection->obj->mat.model->height * coord.y);

					color3 color = intersection->obj->mat.model->textures->at(uVInter)->at(uUInter);
					matTexture.diffuseColor = color;
					matTexture.specularColor = color;
					bsdf = RDM_bsdf(LdotH, NdotH, VdotH, LdotN, VdotN, &matTexture);
				}
				break;
			case KDFREE_SPHERE:
				{
					Material matTexture;
					matTexture.IOR = intersection->obj->mat.IOR;
					matTexture.roughness = intersection->obj->mat.roughness;

					point3 inter = rotateY(intersection->position, degreesToRadians(-70.f));

					float mu = 0.5f + atan2(inter.z, inter.x)/(2.f*glm::pi<float>());
					//float as = asin(inter.y/intersection->obj->geom.sphere.radius);
					float mv = 0.5f - asin(inter.y/intersection->obj->geom.sphere.radius)/glm::pi<float>();

					unsigned height = intersection->obj->mat.model->height;
					unsigned width = intersection->obj->mat.model->width;

					unsigned s = (int) round(mu * (width*1))%width;
					unsigned t = (int) round(mv * (height*1))%height;

					color3 color = intersection->obj->mat.model->textures->at(t)->at(s);

					matTexture.specularColor = color;
					matTexture.diffuseColor = color;

					NdotH = -NdotH;
					LdotN = -LdotN;
					VdotN = -VdotN;


					bsdf = RDM_bsdf(LdotH, NdotH, VdotH, LdotN, VdotN, &matTexture);
				}
				break;
			default:
				bsdf = RDM_bsdf(LdotH, NdotH, VdotH, LdotN, VdotN, &intersection->obj->mat);
				break;
		}
	}else {
		bsdf = RDM_bsdf(LdotH, NdotH, VdotH, LdotN, VdotN, &intersection->obj->mat);
	}
	return clamp(lc * bsdf * LdotN, 0.f, pi<float>());
	//return (mat->diffuseColor/glm::pi<float>())*cosO*lc;
}

//! if tree is not null, use intersectKdTree to compute the intersection instead
//! of intersect scene
color3 trace_ray(Scene *scene, Ray *ray, KdTree *tree){
	Intersection intersection;
	if (intersectKdTree(scene, tree, ray, &intersection)) {
	//if (intersectScene(scene, ray, &intersection)) {
		color3 ret = vec3(0.f);
		vec3 l;
		vec3 vecL;
		Ray bounce;
		Intersection ombre;
		for (auto const &light : scene->lights) {
			vecL = light->position - intersection.position;
			l = normalize(vecL);
			rayInit(&bounce, intersection.position + acne_eps * l, l, 0, length(vecL));
			if (!intersectKdTree(scene, tree, &bounce, &ombre))// opti ne pas calculer solution
			//if (!intersectScene(scene, &bounce, &ombre))// opti ne pas calculer solution
				ret += shade(intersection.normal, -ray->dir, l, light->color, &intersection);
		}
		if (ray->depth < MAX_DEPTH) {
			vec3 reflet = reflect(ray->dir, intersection.normal);
			rayInit(&bounce, intersection.position + acne_eps * intersection.normal, reflet, 0, 1000000, ray->depth + 1);
			float fresnel = RDM_Fresnel(dot(bounce.dir, intersection.normal), 1.f, intersection.obj->mat.IOR);
			vec3 rayC = clamp(trace_ray(scene, &bounce, tree), 0.f, 7.f);
			ret += fresnel * rayC * intersection.obj->mat.specularColor;
		}
		return ret;
		//return 0.5f * intersection.normal + 0.5f;
	} else {
		return scene->skyColor;
	}
}

void renderImage(Image *img, Scene *scene){

	//! This function is already operational, you might modify it for antialiasing
	//! and kdtree initializaion
	float aspect = 1.f / scene->cam.aspect;
	double start = omp_get_wtime(), stop;
	KdTree *tree = initKdTree(scene);
	stop = omp_get_wtime();
	std::cout << stop-start << std::endl;
	float delta_y = 1.f / (img->height * 0.5f);   //! one pixel size
	vec3 dy = delta_y * aspect * scene->cam.ydir; //! one pixel step
	vec3 ray_delta_y = (0.5f - img->height * 0.5f) / (img->height * 0.5f) *
					   aspect * scene->cam.ydir;

	float delta_x = 1.f / (img->width * 0.5f);
	vec3 dx = delta_x * scene->cam.xdir;
	vec3 ray_delta_x =
			(0.5f - img->width * 0.5f) / (img->width * 0.5f) * scene->cam.xdir;

	start = omp_get_wtime();
	for (size_t j = (0); j < img->height; j++) {//600-460
		if (j != 0)
			printf("\033[A\r");
		float progress = (float) j / img->height * 100.f;
		printf("progress\t[");
		int cpt = 0;
		for (cpt = 0; cpt < progress; cpt += 5)
			printf(".");
		for (; cpt < 100; cpt += 5)
			printf(" ");
		printf("]\n");
#pragma omp parallel for
		for (size_t i = 0; i < img->width; i++) {//200
			color3 *ptr = getPixelPtr(img, (img->width)-i, j);// Inversion du sens pour avoir le modèle 3d dans le bon sens
			//color3 *ptr = getPixelPtr(img, -i, j);
			color3 pix = vec3(0.f);
			float nb_pix = ALIAS*ALIAS;
			for (size_t ali = 0; ali < ALIAS; ++ali)
				for (size_t alj = 0; alj < ALIAS; ++alj){
					vec3 ray_dir = scene->cam.center + ray_delta_x + ray_delta_y +
							(float(i)+(ali/ALIAS)) * dx + (float(j)+(alj/ALIAS)) * dy;
					Ray rx;
					rayInit(&rx, scene->cam.position, normalize(ray_dir));
					// pb G rdm smith G1
					pix += trace_ray(scene, &rx, tree);
				}
			*ptr = pix/nb_pix;
		}
	}
	stop = omp_get_wtime();
	printf("%f\n", stop-start);
}
