#include <iostream>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <ctime>

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "Bitmap.h"
#include "vectors.h"
#include "objects.h"
#include "functions.h"

const uint32_t RED = 0x000000FF;
const uint32_t GREEN = 0x0000FF00;
const uint32_t BLUE = 0x00FF0000;

struct Settings
{
  int width;
  int height;
  float fov;
  int maxDepth;
  Vec3f backgroundColor;
  float bias;
  float AA;
};

bool scene_intersect(const Vec3f &orig, const Vec3f &dir, const std::vector<std::unique_ptr<Object>> &objects, Vec3f &hit, Vec3f &N, Material &material)
{
  float objects_dist = std::numeric_limits<float>::max();
  for (size_t i = 0; i < objects.size(); i++)
  {
    float dist_i;
    if (objects[i]->intersection(orig, dir, dist_i) && dist_i < objects_dist)
    {
      objects_dist = dist_i;
      hit = orig + dir * dist_i;
      objects[i]->getData(hit,N ,material);
    }
  }

  float checkerboard_dist = std::numeric_limits<float>::max();
  if (fabs(dir.y) > 1e-3)
  {

    float angle = deg2rad(0); // angel of rotatating
    float d = -(orig.y + 4) / dir.y; // the checkerboard plane has equation y = -4
    Vec3f pt = orig + dir * d;
    if (d > 0 && d  < objects_dist)
    {
      checkerboard_dist = d;
      hit = pt;
      N = Vec3f(0, 1, 0);

      material.specular =100000;
       material.diffuse_color = (int(.75 * hit.x + 1000) + int(.75 * hit.z)) & 1 ? Vec3f(1, 1, 1) : Vec3f(0, 0, 0);
      material.diffuse_color = material.diffuse_color;
    }
  }
  return std::min(objects_dist, checkerboard_dist) < 1000;
  //return spheres_dist < 1000;
}

Vec3f newcast_ray(
    const Vec3f &orig, const Vec3f &dir,
    const std::vector<std::unique_ptr<Object>> &objects,
    const std::vector<Light> &lights,
    const Settings &settings,
    size_t depth = 0)
{
  if (depth > settings.maxDepth)
    return settings.backgroundColor;

  Vec3f hit_point, N;
  Material material;
  Vec3f PhongColor = 0;
  if (scene_intersect(orig, dir, objects, hit_point, N, material))
  {

    switch (material.materialType)
    {

    case GLOSSY:
    {

      float kr;
      fresnel(dir, N, material.refract, kr);
      Vec3f reflect_dir = normalize(reflect(dir, N));
      Vec3f refract_dir = normalize(refract(dir, N, material.refract));
      Vec3f reflect_orig = (dotProduct(reflect_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3; // offset the original point to avoid occlusion by the object itself
      Vec3f refract_orig = (dotProduct(refract_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3;
      Vec3f reflect_color = newcast_ray(reflect_orig, reflect_dir, objects, lights, settings, depth + 1);
      Vec3f refract_color = newcast_ray(refract_orig, refract_dir, objects, lights, settings, depth + 1);

      Vec3f diffuse = 0, specular = 0;
      Vec3f shadow_orig = (dotProduct(dir, N) < 0) ? hit_point + N * 1e-3 : hit_point - N * 1e-3;
      for (uint32_t i = 0; i < lights.size(); ++i)
      {
        Vec3f light_dir = normalize(lights[i].position - hit_point);
        float light_dist = norma(lights[i].position - hit_point);
        Vec3f shadow_point, shadow_N;
        Material temp_material;
        bool inShadow = scene_intersect(shadow_orig, light_dir, objects, shadow_point, shadow_N, temp_material) &&
                        norma(shadow_point - shadow_orig) < light_dist;
        diffuse += (1 - inShadow) * lights[i].intensity * std::max(0.f, dotProduct(light_dir, N));
        Vec3f R = reflect(-light_dir, N);
        specular += powf(std::max(0.f, -dotProduct(R, dir)), material.specular) * lights[i].intensity;
      }
      PhongColor = diffuse*material.diffuse_color*0.5   + material.diffuse_color*specular + material.diffuse_color*reflect_color*0.40;
      break;
    }

    case REFLECTION_AND_REFRACTION:
    {
      float kr;
      fresnel(dir, N, material.refract, kr);
      Vec3f reflect_dir = normalize(reflect(dir, N));
      Vec3f refract_dir = normalize(refract(dir, N, material.refract));
      Vec3f reflect_orig = (dotProduct(reflect_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3; // offset the original point to avoid occlusion by the object itself
      Vec3f refract_orig = (dotProduct(refract_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3;
      Vec3f reflect_color = newcast_ray(reflect_orig, reflect_dir, objects, lights, settings, depth + 1);
      Vec3f refract_color = newcast_ray(refract_orig, refract_dir, objects, lights, settings, depth + 1);
      PhongColor = reflect_color * kr + refract_color * (1 - kr);
      break;
    }

    case REFLECTION:
    {
      Vec3f reflect_dir = normalize(reflect(dir, N));
      Vec3f reflect_orig = (dotProduct(reflect_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3; // offset the original point to avoid occlusion by the object itself
      Vec3f reflect_color = newcast_ray(reflect_orig, reflect_dir, objects, lights, settings, depth + 1);
      PhongColor += reflect_color * 0.8;
      break;
    }

    /* case DIFFUSE_REFLECTION:
    {
      Vec3f diffuse = 0, specular = 0;
      Vec3f shadow_orig = (dotProduct(dir, N) < 0) ? hit_point + N * 1e-3 : hit_point - N * 1e-3;

      Vec3f reflect_dir = normalize(reflect(dir, N));
      Vec3f reflect_orig = (dotProduct(reflect_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3; // offset the original point to avoid occlusion by the object itself
      Vec3f reflect_color = newcast_ray(reflect_orig, reflect_dir, spheres, lights, settings, depth + 1);
      for (uint32_t i = 0; i < lights.size(); ++i)
      {
        Vec3f lightA = lights[i].color * lights[i].intensity;
        Vec3f light_dir = normalize(lights[i].position - hit_point);
        float light_dist = norma(lights[i].position - hit_point);
        Vec3f shadow_point, shadow_N;
        Material temp_material;
        bool inShadow = scene_intersect(shadow_orig, light_dir, spheres, shadow_point, shadow_N, temp_material) &&
                        norma(shadow_point - shadow_orig) < light_dist;
        diffuse += lightA * (1 - inShadow) * std::max(0.f, dotProduct(light_dir, N));
        Vec3f R = reflect(-light_dir, N);
        specular += lightA * powf(std::max(0.f, -dotProduct(R, dir)), material.specular);
      }
      PhongColor = diffuse * material.diffuse_color * 0.8 + specular * 0.2; //Kd = 0.8 Ks = 0.2
      //PhongColor = specular*0.3; //Kd = 0.8 Ks = 0.2 albedo[0] + Vec3f(1., 1., 1.)*specular_light_intensity * material.albedo[1] + reflect_color*material.albedo[2];
      break;
    } */



    default:
    {

      Vec3f diffuse = 0, specular = 0;
      Vec3f shadow_orig = (dotProduct(dir, N) < 0) ? hit_point + N * 1e-3 : hit_point - N * 1e-3;

      Vec3f reflect_dir = normalize(reflect(dir, N));
      Vec3f reflect_orig = (dotProduct(reflect_dir, N) < 0) ? hit_point - N * 1e-3 : hit_point + N * 1e-3; // offset the original point to avoid occlusion by the object itself
      Vec3f reflect_color = newcast_ray(reflect_orig, reflect_dir, objects, lights, settings, depth + 1);
      for (uint32_t i = 0; i < lights.size(); ++i)
      {
        Vec3f lightA = lights[i].color * lights[i].intensity;
        Vec3f light_dir = normalize(lights[i].position - hit_point);
        float light_dist = norma(lights[i].position - hit_point);
        Vec3f shadow_point, shadow_N;
        Material temp_material;
        bool inShadow = scene_intersect(shadow_orig, light_dir, objects, shadow_point, shadow_N, temp_material) &&
                        norma(shadow_point - shadow_orig) < light_dist;
        diffuse += lightA * (1 - inShadow) * std::max(0.f, dotProduct(light_dir, N));
        Vec3f R = reflect(-light_dir, N);
        specular += lightA * powf(std::max(0.f, -dotProduct(R, dir)), material.specular);
      }
      PhongColor = diffuse * material.diffuse_color * 0.8 + specular * 0.2; //Kd = 0.8 Ks = 0.2
      //PhongColor = specular*0.3; //Kd = 0.8 Ks = 0.2 albedo[0] + Vec3f(1., 1., 1.)*specular_light_intensity * material.albedo[1] + reflect_color*material.albedo[2];
      break;
    }
    }
  }
  else
  {
    PhongColor = settings.backgroundColor;
  }

  return PhongColor;
}

int main(int argc, const char **argv)
{

  std::unordered_map<std::string, std::string> cmdLineParams;

  for (int i = 0; i < argc; i++)
  {
    std::string key(argv[i]);

    if (key.size() > 0 && key[0] == '-')
    {
      if (i != argc - 1) // not last argumen
      {
        cmdLineParams[key] = argv[i + 1];
        i++;
      }
      else
        cmdLineParams[key] = "";
    }
  }

  std::string outFilePath = "zout.bmp";
  if (cmdLineParams.find("-out") != cmdLineParams.end())
    outFilePath = cmdLineParams["-out"];

  int sceneId = 0;
  if (cmdLineParams.find("-scene") != cmdLineParams.end())
    sceneId = atoi(cmdLineParams["-scene"].c_str());

  int threads = 1;
  if (cmdLineParams.find("-threads") != cmdLineParams.end())
    threads = atoi(cmdLineParams["-threads"].c_str());

  Settings settings;

/*    settings.width = 512;
  settings.height = 512; */ 

     settings.width = 1024;
  settings.height = 796;  

/*      settings.width = 1920;
  settings.height = 1080; */ 
/*       settings.width = 3840;
  settings.height = 2160; */    

  settings.fov = 90;
  settings.maxDepth = 4;
  settings.backgroundColor = Vec3f(0.0, 0.0, 0.0); // light blue Vec3f(0.2, 0.7, 0.8);
  settings.AA = 1;

  Material orange(Vec3f(1, 0.4, 0.3), DIFFUSE, 1.0, 1.5);
  Material red(Vec3f(0.40, 0.0, 0.0), GLOSSY, 10.0, 1.5);
  Material blue(Vec3f(0.0, 0.00, 0.4), GLOSSY, 20.0, 1.5);
  Material ivory(Vec3f(0.4, 0.4, 0.3), DIFFUSE, 50.0, 1.5);
  Material gold(Vec3f(0.5, 0.4, 0.1), GLOSSY, 20.0, 1.5);
  Material mirror(Vec3f(0.0, 10.0, 0.8), REFLECTION, 1.0, 1.5);
  Material glass(Vec3f(0.0, 10.0, 0.8), REFLECTION_AND_REFRACTION, 1.0, 1.5);

  std::vector<std::unique_ptr<Object>> objects; 
  //std::vector<Sphere> spheres;
  std::vector<Light> lights;

  if (sceneId == 1)
  {
    objects.push_back(std::unique_ptr<Object>(new Cylinder (Vec3f(0,0, -7), 1.5 ,4, orange)));
    objects.push_back(std::unique_ptr<Object>(new Cone (Vec3f(-4,-4, -7), 2 ,2, gold)));
    //objects.push_back(std::unique_ptr<Object>( new Sphere(Vec3f(-5, 0, -5), 3, ivory)));


     lights.push_back(Light(Vec3f(-20, 20, 20), 1.5, Vec3f(1, 1, 1)));
    //lights.push_back(Light(Vec3f(30, 50, -25), 1.2, Vec3f(1, 1, 1)));
    //lights.push_back(Light(Vec3f(30, 20, -20), 4.3, Vec3f(1, 1, 1)));
  }

  else if (sceneId == 2)
  {
    objects.push_back(std::unique_ptr<Object>( new Sphere(Vec3f(-5, 0, -5), 3, ivory)));
    objects.push_back(std::unique_ptr<Object>( new  Sphere(Vec3f(4, 2, -4), 2, orange)));

    lights.push_back(Light(Vec3f(1, 3, -4), 0.5, Vec3f(1, 1, 1)));
    lights.push_back(Light(Vec3f(0, 5, -6), 1, Vec3f(1, 1, 1)));
  }

  else if (sceneId == 3)
  {
    objects.push_back(std::unique_ptr<Object>( new Sphere(Vec3f(-2, 0, -2), 0.5, red)));
    objects.push_back(std::unique_ptr<Object>(new Sphere(Vec3f(2, 0, -5), 1, glass)));
    objects.push_back(std::unique_ptr<Object>(new Sphere(Vec3f(-3, 7, -20), 6, gold)));
    objects.push_back(std::unique_ptr<Object>(new Sphere(Vec3f(-3, 6, -12), 2, ivory)));
    objects.push_back(std::unique_ptr<Object>(new Sphere(Vec3f(15, -2, -18), 3, orange))); 
    objects.push_back(std::unique_ptr<Object>(new Sphere(Vec3f(1.25, 12, -40), 10, mirror)));
    objects.push_back(std::unique_ptr<Object>(new Cylinder (Vec3f(5,0, -15), 1 ,6, red)));


    lights.push_back(Light(Vec3f(-20, 20, 20), 0.8, Vec3f(1, 1, 1)));
    //lights.push_back(Light(Vec3f(30, 50, -25), 1.2, Vec3f(1, 1, 1)));
    lights.push_back(Light(Vec3f(30, 20, -20), 2.3, Vec3f(1, 1, 1)));
  }

  std::vector<uint32_t> image(settings.height * settings.width);

  float scale = tan(deg2rad(settings.fov * 0.5));
  float imageAspectRatio = settings.width / (float)settings.height;

  std:: cout << threads << std:: endl;
  #pragma omp parallel for num_threads (threads)
  for (size_t j = 0; j < settings.height; j++) // actual rendering loop
  {
    for (size_t i = 0; i < settings.width; i++)
    {
      Vec3f temp = Vec3f(0,0,0);
        for (size_t k = 0; k < settings.AA; k++)
        {
            float x = (2 * (i + 0.5 + k*0.25) / (float)settings.width -  1) * imageAspectRatio * scale ;
            float y = (2 * (j + 0.5 - k*0.25) / (float)settings.height - 1) * scale;

            Vec3f dir = normalize(Vec3f(x, y, -1));
            temp += newcast_ray(Vec3f(0, 0, 0), dir, objects, lights, settings);
        }
      temp = temp * (1.0 / settings.AA);
      float max = std::max(temp.x, std::max(temp.y, temp.z));
      if (max > 1)
        temp = temp * (1. / max);
      image[i + j * settings.width] = (uint32_t)(255 * std::max(0.f, std::min(1.f, temp.z))) << 16 | (uint32_t)(255 * std::max(0.f, std::min(1.f, temp.y))) << 8 | (uint32_t)(255 * std::max(0.f, std::min(1.f, temp.x)));
    }
  }



  SaveBMP(outFilePath.c_str(), image.data(), settings.width, settings.height);

  std::cout << "end." << std::endl;

  return 0;
}