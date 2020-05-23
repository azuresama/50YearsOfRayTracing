#pragma once
#include "geometry/Ray.h"

struct MyLight {
  float ambient = 0.1f;  // ambient
  float intensity = 2.0f;

  virtual Ray generateShadowRay(const glm::vec3& shadingPt) = 0;
  virtual glm::vec3 blinnPhongShading(const glm::vec3& shadingPt,
                                      const glm::vec3& normal,
                                      const glm::vec3& viewDir, float n) = 0;
};

struct PointLight : public MyLight {
  glm::vec3 pos;
  float range = 15;

  PointLight(const glm::vec3& inPos) : pos(inPos) {}

  virtual Ray generateShadowRay(const glm::vec3& shadingPt) override {
    glm::vec3 L = glm::normalize(pos - shadingPt);
    return Ray(shadingPt, L);
  }

  virtual glm::vec3 blinnPhongShading(const glm::vec3& shadingPt,
                                      const glm::vec3& normal,
                                      const glm::vec3& viewDir,
                                      float n) override {
    glm::vec3 L = glm::normalize(pos - shadingPt);
    glm::vec3 H = glm::normalize(L - viewDir);
    float NdotH = glm::dot(normal, H);
    float NdotL = glm::dot(normal, L);

    float falloff = 1.0f;
    float d = glm::distance(pos, shadingPt);
    if (d > range) {
      float r = range / d;
      falloff = r * r;
    }

    float diffuse = std::max(0.0f, NdotL) * falloff * intensity;
    float specular = std::powf(NdotH, n) * falloff * intensity;
    return glm::vec3(ambient, diffuse, specular);
  }
};

struct DirectionalLight : public MyLight {
  glm::vec3 dir;

  DirectionalLight(const glm::vec3& inDir) : dir(glm::normalize(inDir)) {}

  virtual Ray generateShadowRay(const glm::vec3& shadingPt) override {
    glm::vec3 L = -dir;
    return Ray(shadingPt, L);
  }

  virtual glm::vec3 blinnPhongShading(const glm::vec3& shadingPt,
                                      const glm::vec3& normal,
                                      const glm::vec3& viewDir,
                                      float n) override {
    glm::vec3 L = -dir;
    glm::vec3 H = glm::normalize(L - viewDir);
    float NdotH = glm::dot(normal, H);
    float NdotL = glm::dot(normal, L);

    float diffuse = std::max(0.0f, NdotL) * intensity;
    float specular = std::powf(NdotH, n) * intensity;
    return glm::vec3(ambient, diffuse, specular);
  }
};