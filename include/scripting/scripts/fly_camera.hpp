#pragma once

#include "scripting/script.hpp"
#include "scene/attachments/camera.hpp"

#include "common/include.hpp"

namespace goma {

class FlyCamera : public Script {
  public:
    FlyCamera(const AttachmentIndex<Camera> camera_id, float speed = 1.0f)
        : camera_id_(camera_id), speed_(speed) {}

    virtual void Update(Engine& engine, float delta_time) override {
        auto scene = engine.scene();

        auto camera_res = scene->GetAttachment<Camera>(camera_id_);
        if (!camera_res) {
            return;  // TODO error code
        }

        auto& camera = camera_res.value().get();
        auto camera_nodes = scene->GetAttachedNodes<Camera>(camera_id_);
        glm::mat4 camera_transform = glm::mat4(1.0f);

        if (camera_nodes && camera_nodes.value().get().size() > 0) {
            auto camera_node = *camera_nodes.value().get().begin();

            // Update transform based on input
            auto transform = scene->GetTransform(camera_node).value();
            auto input_state = engine.input_system().GetFrameInput();
            const auto& keypresses = input_state.keypresses;

            auto has_key = [&keypresses](KeyInput key) {
                return keypresses.find(key) != keypresses.end();
            };

            // https://stackoverflow.com/questions/9857398/quaternion-camera-how-do-i-make-it-rotate-correctly
            if (has_key(KeyInput::Up)) {
                auto right = glm::cross(camera.look_at, camera.up);
                auto new_rotation =
                    transform.rotation * glm::quat(right * delta_time);

                // Limit camera pitch to +-85 degrees
                auto look_at = new_rotation * camera.look_at;
                if (glm::dot(look_at, camera.up) <
                    glm::sin(glm::radians(85.0f))) {
                    transform.rotation = new_rotation;
                }
            }
            if (has_key(KeyInput::Down)) {
                auto right = glm::cross(camera.look_at, camera.up);
                auto new_rotation =
                    transform.rotation * glm::quat(-right * delta_time);

                // Limit camera pitch to +-85 degrees
                auto look_at = new_rotation * camera.look_at;
                if (glm::dot(look_at, camera.up) >
                    glm::sin(glm::radians(-85.0f))) {
                    transform.rotation = new_rotation;
                }
            }
            if (has_key(KeyInput::Left)) {
                transform.rotation =
                    glm::quat(camera.up * delta_time) * transform.rotation;
            }
            if (has_key(KeyInput::Right)) {
                transform.rotation =
                    glm::quat(-camera.up * delta_time) * transform.rotation;
            }

            if (has_key(KeyInput::W)) {
                transform.position +=
                    transform.rotation * camera.look_at * speed_ * delta_time;
            }
            if (has_key(KeyInput::S)) {
                transform.position +=
                    transform.rotation * -camera.look_at * speed_ * delta_time;
            }
            if (has_key(KeyInput::A)) {
                transform.position += transform.rotation *
                                      -glm::cross(camera.look_at, camera.up) *
                                      speed_ * delta_time;
            }
            if (has_key(KeyInput::D)) {
                transform.position += transform.rotation *
                                      glm::cross(camera.look_at, camera.up) *
                                      speed_ * delta_time;
            }
            scene->SetTransform(camera_node, transform);
        }
    }

  private:
    AttachmentIndex<Camera> camera_id_{};
    float speed_{1.0f};
};

}  // namespace goma
