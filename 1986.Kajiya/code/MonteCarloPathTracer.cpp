#include "MonteCarloPathTracer.h"

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <random>

#include "MaterialBase.h"
#include "framework/PinholeCamera.h"
#include "sampling//UniformSampling.h"
#include "sampling/JitteringSampling.h"

namespace RayTracingHistory {

constexpr float FLOAT_MAX = std::numeric_limits<float>::max();
constexpr uint32_t SPP_ROOT = 10;
constexpr uint32_t MAX_BOUNCES = 5;

void MonteCarloPathTracer::_init(SDL_Window* pWnd) {
  TiledRenderer::_init(pWnd);

  int SPP = SPP_ROOT * SPP_ROOT;
  mInfo = std::string(" - SPP: ");
  mInfo.append(std::to_string(SPP));

  if (MAX_BOUNCES == 0)
    mInfo.append(", Light Source ");
  else if (MAX_BOUNCES == 1)
    mInfo.append(", Direct Lighting ");
  else if (MAX_BOUNCES > 1) {
    mInfo.append(", Bounces: ");
    mInfo.append(std::to_string(MAX_BOUNCES));
  }
}

std::string MonteCarloPathTracer::getInfo() const { return mInfo; }

float MonteCarloPathTracer::getProgress() const {
  auto SPP = SPP_ROOT * SPP_ROOT;
  auto total = mFrameWidth * mFrameHeight * SPP;
  if (mPixelCount == total)
    return 1.0f;
  else
    return (float)mPixelCount / total;
}

bool MonteCarloPathTracer::isDone() const {
  return mPresentLine == mFrameHeight && getProgress() == 1.0f;
}

void MonteCarloPathTracer::_tileRenderThread(Tile tile, MyScene::Ptr scene,
                                             MyCamera::Ptr camera) {
  PinholeCamera* pCamera = static_cast<PinholeCamera*>(camera.get());
  MySceneWithLight* pScene = dynamic_cast<MySceneWithLight*>(scene.get());

  std::random_device randDevice;
  std::mt19937 stdRand(randDevice());
  std::uniform_real_distribution<float> uniformDist(0, 1);

  float W = mFrameWidth;
  float H = mFrameHeight;
  int MAX_SPP = SPP_ROOT * SPP_ROOT;

  int SPP = 0;
  std::vector<glm::vec3> tileBuffer;

  int tileW = tile.right - tile.left;
  int tileH = tile.bottom - tile.top;
  tileBuffer.resize(tileW * tileH, glm::vec3(0));

#if 0
  auto xi1 = JitteringSampling::generateSamples(SPP_ROOT, false);
  auto xi2 = JitteringSampling::generateSamples(SPP_ROOT, true);
#else
  auto xi1 = UniformSampling::generateSamples(SPP_ROOT);
  auto xi2 = UniformSampling::generateSamples(SPP_ROOT);
#endif

  while (SPP < MAX_SPP) {
    int index = 0;
    float scale = 1.0f / (SPP + 1);

    for (int y = tile.top; y < tile.bottom; y++)
      for (int x = tile.left; x < tile.right; x++) {
        if (!mRuning) break;

        const glm::vec2& xi = xi1[SPP];
        Ray primaryRay =
            pCamera->generateViewingRay((x + xi.x) / W, (y + xi.y) / H);

        auto& buf = tileBuffer[index++];
        buf += _traceRay(primaryRay, pScene, xi2[SPP], 1.0f, 0);

        _writePixel(x, y, glm::vec4(buf / float(SPP + 1), 1), 1);
        mPixelCount++;
      }

    SPP++;
  }  // end of while
}

glm::vec3 MonteCarloPathTracer::_traceRay(const Ray& wo,
                                          MySceneWithLight* pScene,
                                          const glm::vec2& xi, float weight,
                                          int depth) {
  const glm::vec3 bgColor(0.f, 0.f, 0.f);

  if (depth >= MAX_BOUNCES) return bgColor;

  HitRecord hitRec;
  bool bHit = pScene->closestHit(wo, 0.001f, FLOAT_MAX, hitRec);
  if (!bHit) return bgColor;

  MaterialBase* pMtl = static_cast<MaterialBase*>(hitRec.mtl);

  // error check
  if (!pMtl) return glm::abs(hitRec.normal);

  const AreaLight* pLight = pScene->getMainLight();

  if (depth == 0) {
    // hit light
    if (pMtl->isLight()) return glm::vec3(pLight->getIntensity());
  }

  // bounces == 0: light source
  if (MAX_BOUNCES == 0) return bgColor;

  //-- direct lighting

  // visibility between the shading point and the light
  float visibilityTerm = 1.0f;
  constexpr float SHADOW_E = 0.002f;

  auto shadowRet = pLight->generateShadowRay(hitRec.p, hitRec.normal, xi);
  Ray shadowRay = std::get<0>(shadowRet);
  float lightDistance = std::get<1>(shadowRet);
  glm::vec3 lightNormal = std::get<2>(shadowRet);

  auto stopWithAnyHit = [](const HitRecord&) { return true; };
  bool bShadow =
      pScene->anyHit(shadowRay, SHADOW_E, lightDistance, stopWithAnyHit);
  if (bShadow) visibilityTerm = 0.125f;

  // geometry term
  const float sysUnit = pScene->systemUnit();
  glm::vec3 lightDir = shadowRay.direction;
  float R = lightDistance / sysUnit;
  float geometryTerm = glm::max(0.0f, glm::dot(lightDir, hitRec.normal)) *
                       glm::max(0.0f, glm::dot(lightDir, lightNormal)) /
                       (R * R);

  glm::vec3 color = pMtl->getBaseColor(hitRec.uv, hitRec.p);
  float fr = pMtl->BRDF(lightDir, wo.direction);
  float A = pLight->getArea() / sysUnit / sysUnit;
  float Li = pLight->getIntensity();

  glm::vec3 directLighting = Li * A * visibilityTerm * geometryTerm * color;

  // bounces==1: direct lighting, bounces>1: indirect lighting
  glm::vec3 indirectLighting(0);
  if (MAX_BOUNCES > 1 && weight > glm::epsilon<float>()) {
    glm::vec3 p = hitRec.p;
    glm::vec3 d = pMtl->scatter(wo.direction, hitRec.normal);

    float reflectance =
        pMtl->BRDF(d, wo.direction) * glm::dot(d, hitRec.normal);
    indirectLighting =
        _traceRay(Ray(p, d), pScene, xi, reflectance * weight, depth + 1);
    indirectLighting = weight * indirectLighting;
  }

  return (directLighting + indirectLighting);
}

}  // namespace RayTracingHistory
