#include "renderer/renderer.h"
#include "camera.h"
#include "fwd.hpp"
#include "glm.hpp"
#include "renderer/gl/primitive_type.h"
#include "renderer/gl/vertex.h"
#include "ui.h"
#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <ext/matrix_transform.hpp>
#include <iostream>
#include <ostream>

using namespace Bess::Renderer2D;

namespace Bess {

std::vector<PrimitiveType> Renderer::m_AvailablePrimitives;

std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Shader>>
    Renderer::m_shaders;
std::unordered_map<PrimitiveType, std::unique_ptr<Gl::Vao>> Renderer::m_vaos;

std::shared_ptr<Camera> Renderer::m_camera;

std::vector<glm::vec4> Renderer::m_StandardQuadVertices;
std::unordered_map<PrimitiveType, size_t> Renderer::m_MaxRenderLimit;

RenderData Renderer::m_RenderData;

std::unique_ptr<Gl::Shader> Renderer::m_GridShader;
std::unique_ptr<Gl::Vao> Renderer::m_GridVao;

void Renderer::init() {

    m_GridShader = std::make_unique<Gl::Shader>(
        "assets/shaders/grid_vert.glsl", "assets/shaders/grid_frag.glsl");

    std::vector<Gl::VaoAttribAttachment> attachments;
    attachments.emplace_back(Gl::VaoAttribAttachment(
        Gl::VaoAttribType::vec3, offsetof(Gl::GridVertex, position)));
    attachments.emplace_back(Gl::VaoAttribAttachment(
        Gl::VaoAttribType::vec2, offsetof(Gl::GridVertex, texCoord)));
    attachments.emplace_back(Gl::VaoAttribAttachment(
        Gl::VaoAttribType::int_t, offsetof(Gl::GridVertex, id)));
    attachments.emplace_back(Gl::VaoAttribAttachment(
        Gl::VaoAttribType::float_t, offsetof(Gl::GridVertex, ar)));

    m_GridVao =
        std::make_unique<Gl::Vao>(8, 12, attachments, sizeof(Gl::GridVertex));

    m_AvailablePrimitives = {PrimitiveType::curve, PrimitiveType::quad,
                             PrimitiveType::circle};
    m_MaxRenderLimit[PrimitiveType::quad] = 250;
    m_MaxRenderLimit[PrimitiveType::curve] = 250;
    m_MaxRenderLimit[PrimitiveType::circle] = 250;

    std::string vertexShader, fragmentShader;

    for (auto primitive : m_AvailablePrimitives) {
        switch (primitive) {
        case PrimitiveType::quad:
            vertexShader = "assets/shaders/quad_vert.glsl";
            fragmentShader = "assets/shaders/quad_frag.glsl";
            break;
        case PrimitiveType::curve:
            vertexShader = "assets/shaders/vert.glsl";
            fragmentShader = "assets/shaders/curve_frag.glsl";
            break;
        case PrimitiveType::circle:
            vertexShader = "assets/shaders/vert.glsl";
            fragmentShader = "assets/shaders/circle_frag.glsl";
            break;
        }

        if (vertexShader.empty() || fragmentShader.empty()) {
            std::cerr << "[-] Primitive " << (int)primitive
                      << "is not available" << std::endl;
            return;
        }

        auto max_render_count = m_MaxRenderLimit[primitive];

        m_shaders[primitive] =
            std::make_unique<Gl::Shader>(vertexShader, fragmentShader);

        if (primitive == PrimitiveType::quad) {
            std::vector<Gl::VaoAttribAttachment> attachments;

            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec3, offsetof(Gl::QuadVertex, position)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec3, offsetof(Gl::QuadVertex, color)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec2, offsetof(Gl::QuadVertex, texCoord)));
            attachments.emplace_back(
                Gl::VaoAttribAttachment(Gl::VaoAttribType::float_t,
                                        offsetof(Gl::QuadVertex, borderSize)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec4,
                offsetof(Gl::QuadVertex, borderRadius)));
            attachments.emplace_back(
                Gl::VaoAttribAttachment(Gl::VaoAttribType::vec4,
                                        offsetof(Gl::QuadVertex, borderColor)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::float_t, offsetof(Gl::QuadVertex, ar)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::int_t, offsetof(Gl::QuadVertex, id)));

            m_vaos[primitive] = std::make_unique<Gl::Vao>(
                max_render_count * 4, max_render_count * 6, attachments,
                sizeof(Gl::QuadVertex));
        } else {
            std::vector<Gl::VaoAttribAttachment> attachments;
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec3, offsetof(Gl::Vertex, position)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec3, offsetof(Gl::Vertex, color)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::vec2, offsetof(Gl::Vertex, texCoord)));
            attachments.emplace_back(Gl::VaoAttribAttachment(
                Gl::VaoAttribType::int_t, offsetof(Gl::Vertex, id)));

            m_vaos[primitive] = std::make_unique<Gl::Vao>(
                max_render_count * 4, max_render_count * 6, attachments,
                sizeof(Gl::Vertex));
        }

        vertexShader.clear();
        fragmentShader.clear();
    }

    m_StandardQuadVertices = {
        {-0.5f, 0.5f, 0.f, 1.f},
        {-0.5f, -0.5f, 0.f, 1.f},
        {0.5f, -0.5f, 0.f, 1.f},
        {0.5f, 0.5f, 0.f, 1.f},
    };
}

void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                    const glm::vec3 &color, int id,
                    const glm::vec4 &borderRadius, const glm::vec4 &borderColor,
                    float borderSize) {
    Renderer::quad(pos, size, color, id, 0.f, borderRadius, borderColor,
                   borderSize);
}
void Renderer::quad(const glm::vec3 &pos, const glm::vec2 &size,
                    const glm::vec3 &color, int id, float angle,
                    const glm::vec4 &borderRadius, const glm::vec4 &borderColor,
                    float borderSize) {
    std::vector<Gl::QuadVertex> vertices(4);

    auto transform = glm::translate(glm::mat4(1.0f), pos);
    transform = glm::rotate(transform, glm::radians(angle), {0.f, 0.f, 1.f});
    transform = glm::scale(transform, {size.x, size.y, 1.f});

    for (int i = 0; i < 4; i++) {
        auto &vertex = vertices[i];
        vertex.position = transform * m_StandardQuadVertices[i];
        vertex.id = id;
        vertex.color = color;
        vertex.borderRadius = borderRadius;
        vertex.ar = size.x / size.y;
        vertex.borderColor = borderColor;
        vertex.borderSize = borderSize;
    }

    vertices[0].texCoord = {0.0f, 1.0f};
    vertices[1].texCoord = {0.0f, 0.0f};
    vertices[2].texCoord = {1.0f, 0.0f};
    vertices[3].texCoord = {1.0f, 1.0f};

    addQuadVertices(vertices);
}

void Renderer::grid(const glm::vec3 &pos, const glm::vec2 &size, int id) {
    std::vector<Gl::GridVertex> vertices(4);

    auto size_ = size;
    size_.x = std::max(size.y, size.x);
    size_.y = std::max(size.y, size.x);

    auto transform = glm::translate(glm::mat4(1.0f), pos);
    transform = glm::scale(transform, {size_.x, size_.y, 1.f});

    for (int i = 0; i < 4; i++) {
        auto &vertex = vertices[i];
        vertex.position = transform * m_StandardQuadVertices[i];
        vertex.id = id;
        vertex.ar = size_.x / size_.y;
    }

    vertices[0].texCoord = {0.0f, 1.0f};
    vertices[1].texCoord = {0.0f, 0.0f};
    vertices[2].texCoord = {1.0f, 0.0f};
    vertices[3].texCoord = {1.0f, 1.0f};

    m_GridShader->bind();
    m_GridVao->bind();

    m_GridShader->setUniformMat4("u_mvp", m_camera->getTransform());
    m_GridVao->setVertices(vertices.data(), vertices.size());
    GL_CHECK(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr));

    m_GridShader->unbind();
    m_GridVao->unbind();
}
glm::vec2 bernstine(const glm::vec2 &p0, const glm::vec2 &p1,
                    const glm::vec2 &p2, const glm::vec2 &p3, const float t) {
    auto t_ = 1 - t;

    glm::vec2 B0 = (float)std::pow(t_, 3) * p0;
    glm::vec2 B1 = (float)(3 * t * std::pow(t_, 2)) * p1;
    glm::vec2 B2 = (float)(3 * t * t * std::pow(t_, 1)) * p2;
    glm::vec2 B3 = (float)(t * t * t) * p3;

    return B0 + B1 + B2 + B3;
}

glm::vec2 Renderer::createCurveVertices(const glm::vec3 &start,
                                        const glm::vec3 &end,
                                        const glm::vec3 &color, const int id) {
    auto dx = end.x - start.x;
    auto dy = end.y - start.y;
    auto angle = std::atan(dy / dx);
    float dis = std::sqrt((dx * dx) + (dy * dy));

    float sizeX = std::max(dis, 4.f);
    sizeX = dis;

    glm::vec3 pos = {start.x, start.y - 0.005f, start.z};
    auto transform = glm::translate(glm::mat4(1.0f), pos);
    transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
    transform = glm::scale(transform, {sizeX, 4.f, 1.f});

    glm::vec3 p;

    std::vector<Gl::Vertex> vertices(4);
    for (int i = 0; i < 4; i++) {
        auto &vertex = vertices[i];
        vertex.position = transform * m_StandardQuadVertices[i];
        vertex.id = id;
        vertex.color = color;

        if (i == 3)
            p = vertex.position;
    }

    vertices[0].texCoord = {0.0f, 1.0f};
    vertices[1].texCoord = {0.0f, 0.0f};
    vertices[2].texCoord = {1.0f, 0.0f};
    vertices[3].texCoord = {1.0f, 1.0f};

    addCurveVertices(vertices);
    return p;
}

int calculateSegments(const glm::vec2 &p1, const glm::vec2 &p2) {
    return (int)(glm::distance(p1 / UI::state.viewportSize,
                               p2 / UI::state.viewportSize) /
                 0.005f);
}

void Renderer::curve(const glm::vec3 &start, const glm::vec3 &end,
                     const glm::vec3 &color, const int id) {
    const int segments = (int)(calculateSegments(start, end));
    double dx = end.x - start.x;
    double offsetX = dx * 0.5;
    glm::vec2 cp2 = {end.x - offsetX, end.y};
    glm::vec2 cp1 = {start.x + offsetX, start.y};
    auto prev = start;
    for (int i = 1; i <= segments; i++) {
        glm::vec2 bP =
            bernstine(start, cp1, cp2, end, (float)i / (float)segments);
        glm::vec3 p = {bP.x, bP.y, start.z};
        createCurveVertices(prev, p, color, id);
        prev = p;
    }
}

void Renderer::circle(const glm::vec3 &center, const float radius,
                      const glm::vec3 &color, const int id) {
    glm::vec2 size = {radius * 2, radius * 2};

    std::vector<Gl::Vertex> vertices(4);

    auto transform = glm::translate(glm::mat4(1.0f), center);
    transform = glm::scale(transform, {size.x, size.y, 1.f});

    for (int i = 0; i < 4; i++) {
        auto &vertex = vertices[i];
        vertex.position = transform * m_StandardQuadVertices[i];
        vertex.id = id;
        vertex.color = color;
    }

    vertices[0].texCoord = {0.0f, 1.0f};
    vertices[1].texCoord = {0.0f, 0.0f};
    vertices[2].texCoord = {1.0f, 0.0f};
    vertices[3].texCoord = {1.0f, 1.0f};

    addCircleVertices(vertices);
}

void Renderer::addCircleVertices(const std::vector<Gl::Vertex> &vertices) {
    auto max_render_count = m_MaxRenderLimit[PrimitiveType::circle];

    auto &primitive_vertices = m_RenderData.circleVertices;

    if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
        flush(PrimitiveType::circle);
    }

    primitive_vertices.insert(primitive_vertices.end(), vertices.begin(),
                              vertices.end());
}

void Renderer::addQuadVertices(const std::vector<Gl::QuadVertex> &vertices) {
    auto max_render_count = m_MaxRenderLimit[PrimitiveType::quad];

    auto &primitive_vertices = m_RenderData.quadVertices;

    if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
        flush(PrimitiveType::quad);
    }

    primitive_vertices.insert(primitive_vertices.end(), vertices.begin(),
                              vertices.end());
}

void Renderer::addCurveVertices(const std::vector<Gl::Vertex> &vertices) {
    auto max_render_count = m_MaxRenderLimit[PrimitiveType::curve];

    auto &primitive_vertices = m_RenderData.curveVertices;

    if (primitive_vertices.size() >= (max_render_count - 1) * 4) {
        flush(PrimitiveType::curve);
    }

    primitive_vertices.insert(primitive_vertices.end(), vertices.begin(),
                              vertices.end());
}
void Renderer::flush(PrimitiveType type) {
    auto &vao = m_vaos[type];
    auto &shader = m_shaders[type];
    auto selId = Simulator::ComponentsManager::compIdToRid(
        ApplicationState::getSelectedId());

    vao->bind();
    shader->bind();

    shader->setUniformMat4("u_mvp", m_camera->getTransform());
    shader->setUniform1i("u_SelectedObjId", selId);

    switch (type) {
    case PrimitiveType::quad: {
        auto &vertices = m_RenderData.quadVertices;
        vao->setVertices(vertices.data(), vertices.size());
        GL_CHECK(glDrawElements(GL_TRIANGLES,
                                (GLsizei)(vertices.size() / 4) * 6,
                                GL_UNSIGNED_INT, nullptr));
        vertices.clear();
    } break;
    case PrimitiveType::curve: {
        auto &vertices = m_RenderData.curveVertices;
        vao->setVertices(vertices.data(), vertices.size());
        GL_CHECK(glDrawElements(GL_TRIANGLES,
                                (GLsizei)(vertices.size() / 4) * 6,
                                GL_UNSIGNED_INT, nullptr));
        vertices.clear();
    } break;
    case PrimitiveType::circle: {
        auto &vertices = m_RenderData.circleVertices;
        vao->setVertices(vertices.data(), vertices.size());
        GL_CHECK(glDrawElements(GL_TRIANGLES,
                                (GLsizei)(vertices.size() / 4) * 6,
                                GL_UNSIGNED_INT, nullptr));
        vertices.clear();
    } break;
    }

    vao->unbind();
    shader->unbind();
}

void Renderer::begin(std::shared_ptr<Camera> camera) { m_camera = camera; }

void Renderer::end() {
    for (auto primitive : m_AvailablePrimitives) {
        flush(primitive);
    }
}
} // namespace Bess
