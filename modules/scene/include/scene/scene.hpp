#pragma once

#include "common/include.hpp"
#include "common/gen_vector.hpp"

#include "scene/node.hpp"
#include "scene/attachments/camera.hpp"
#include "scene/attachments/light.hpp"
#include "scene/attachments/material.hpp"
#include "scene/attachments/mesh.hpp"
#include "scene/attachments/texture.hpp"

namespace goma {

class Scene {
  public:
    template <typename T>
    using vec_type = gen_vector<T>;

    Node& root_node() { return root_node_; }

    vec_type<Camera>& cameras() { return cameras_; }
    vec_type<Light>& lights() { return lights_; }
    vec_type<Material>& materials() { return materials_; }
    vec_type<Mesh>& meshes() { return meshes_; }
    vec_type<Texture>& textures() { return textures_; }

    Node* find(const std::string& name) { return root_node_.find(name); }

  private:
    Node root_node_ = {"Root"};

    vec_type<Camera> cameras_;
    vec_type<Light> lights_;
    vec_type<Material> materials_;
    vec_type<Mesh> meshes_;
    vec_type<Texture> textures_;
};

}  // namespace goma
