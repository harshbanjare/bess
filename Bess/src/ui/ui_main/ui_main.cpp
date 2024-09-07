#include "ui/ui_main/ui_main.h"

#include "glad/glad.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <string>

#include "components_manager/components_manager.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/dialogs.h"
#include "ui/ui_main/popups.h"
#include "ui/ui_main/properties_panel.h"
#include "ui/ui_main/settings_window.h"

#include "camera.h"

namespace Bess::UI {
    UIState UIMain::state{};

    void UIMain::draw() {
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            resetDockspace();
        }
        drawMenubar();
        drawProjectExplorer();
        drawViewport();
        ComponentExplorer::draw();
        PropertiesPanel::draw();
        drawExternalWindows();
        // ImGui::ShowDemoWindow();
    }

    void UIMain::drawStats(int fps) {
        ImGui::Begin("Stats");
        ImGui::Text("FPS:");
        ImGui::SameLine();
        ImGui::Text("%s", std::to_string(fps).c_str());
        ImGui::Text("Hover Id");
        ImGui::SameLine();
        ImGui::Text("%s", std::to_string(ApplicationState::hoveredId).c_str());
        ImGui::End();
    }

    void UIMain::setViewportTexture(GLuint64 texture) {
        state.viewportTexture = texture;
    }

    void UIMain::drawProjectExplorer() {
        ImGui::Begin("Project Explorer");

        std::string temp = !ApplicationState::simulationPaused ? Icons::FontAwesomeIcons::FA_PAUSE
                                                               : Icons::FontAwesomeIcons::FA_PLAY;
        temp += ApplicationState::simulationPaused ? " Play" : " Pause";
        if (ImGui::Button(temp.c_str())) {
            ApplicationState::simulationPaused = !ApplicationState::simulationPaused;
        }

        for (auto &id : Simulator::ComponentsManager::renderComponenets) {
            auto &entity = Simulator::ComponentsManager::components[id];
            if (ImGui::Selectable(entity->getRenderName().c_str(),
                                  entity->getId() ==
                                      ApplicationState::getSelectedId())) {
                ApplicationState::setSelectedId(entity->getId());
            }
        }

        ImGui::End();
    }

    void UIMain::drawMenubar() {
        bool newFileClicked = false, openFileClicked = false;
        ImGui::BeginMainMenuBar();
        if (ImGui::BeginMenu("File")) {
            // New File
            std::string temp_name = Icons::FontAwesomeIcons::FA_PAPER_PLANE;
            temp_name += " New";
            if (ImGui::MenuItem(temp_name.c_str())) {
                newFileClicked = true;
            };

            // Open File
            temp_name = Icons::FontAwesomeIcons::FA_FOLDER_OPEN;
            temp_name += " Open";
            if (ImGui::MenuItem(temp_name.c_str())) {
                openFileClicked = true;
            };

            // Save File
            temp_name = Icons::FontAwesomeIcons::FA_SAVE;
            temp_name += " Save";
            if (ImGui::MenuItem(temp_name.c_str())) {
                ApplicationState::currentProject->update(Simulator::ComponentsManager::components);
                ApplicationState::currentProject->save();
            };

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Settings")) {
                SettingsWindow::show();
            }
            ImGui::EndMenu();
        }

        auto menubar_size = ImGui::GetWindowSize();
        ImGui::EndMainMenuBar();

        if (newFileClicked) {
            onNewProject();
        } else if (openFileClicked) {
            onOpenProject();
        }

        Popups::PopupRes res;
        if ((res = Popups::handleUnsavedProjectWarning()) != Popups::PopupRes::none) {
            if (res == Popups::PopupRes::yes) {
                ApplicationState::saveCurrentProject();
                if (!ApplicationState::currentProject->isSaved()) {
                    state._internalData.newFileClicked = false;
                    state._internalData.openFileClicked = false;
                    return;
                }
            }

            if (res != Popups::PopupRes::cancel) {
                if (state._internalData.newFileClicked) {
                    ApplicationState::createNewProject();
                    state._internalData.newFileClicked = false;
                } else if (state._internalData.openFileClicked) {
                    ApplicationState::loadProject(state._internalData.path);
                    state._internalData.path = "";
                    state._internalData.openFileClicked = false;
                }
            }
        }
    }

    void UIMain::drawViewport() {
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoDecoration;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, -1.f}, {-1.f, -1.f});

        ImGui::Begin("Viewport", nullptr, flags);
        auto offset = ImGui::GetCursorPos();

        auto viewportPanelSize = ImGui::GetContentRegionAvail();
        state.viewportSize = {viewportPanelSize.x, viewportPanelSize.y};

        ImGui::Image((void *)state.viewportTexture,
                     ImVec2(viewportPanelSize.x, viewportPanelSize.y), ImVec2(0, 1),
                     ImVec2(1, 0));

        auto pos = ImGui::GetWindowPos();
        auto gPos = ImGui::GetMainViewport()->Pos;
        state.viewportPos = {pos.x - gPos.x + offset.x, pos.y - gPos.y + offset.y};

        ImGui::End();
        ImGui::PopStyleVar();

        // Camera controls
        {
            ImGui::SetNextWindowPos(
                {pos.x + viewportPanelSize.x - 208, pos.y + viewportPanelSize.y - 40});
            ImGui::SetNextWindowSize({208, 0});
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

            ImGui::Begin("Camera", nullptr, flags);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8);
            if (ImGui::SliderFloat("Zoom", &state.cameraZoom, Camera::zoomMin,
                                   Camera::zoomMax, nullptr,
                                   ImGuiSliderFlags_AlwaysClamp)) {
                // step size logic
                float stepSize = 0.1f;
                state.cameraZoom = roundf(state.cameraZoom / stepSize) * stepSize;
            }
            ImGui::PopStyleVar(2);
            ImGui::End();
            ImGui::PopStyleVar(2);
        }
    }

    void UIMain::resetDockspace() {
        auto mainDockspaceId = ImGui::GetID("MainDockspace");

        ImGui::DockBuilderRemoveNode(mainDockspaceId);
        ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar);

        auto dock_id_left = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Left, 0.15f, nullptr, &mainDockspaceId);
        auto dock_id_right = ImGui::DockBuilderSplitNode(mainDockspaceId, ImGuiDir_Right, 0.25f, nullptr, &mainDockspaceId);

        auto dock_id_right_bot = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.5f, nullptr, &dock_id_right);

        ImGui::DockBuilderDockWindow("Component Explorer", dock_id_left);
        ImGui::DockBuilderDockWindow("Viewport", mainDockspaceId);
        ImGui::DockBuilderDockWindow("Project Explorer", dock_id_right);
        ImGui::DockBuilderDockWindow("Properties", dock_id_right_bot);

        ImGui::DockBuilderFinish(mainDockspaceId);
    }

    void UIMain::drawExternalWindows() { SettingsWindow::draw(); }

    void UIMain::onNewProject() {
        if (!ApplicationState::currentProject->isSaved()) {
            state._internalData.newFileClicked = true;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
        } else {
            ApplicationState::createNewProject();
        }
    }

    void UIMain::onOpenProject() {

        auto filepath =
            Dialogs::showOpenFileDialog("Open BESS Project File", "*.bproj|");

        if (filepath == "" || !std::filesystem::exists(filepath))
            return;

        if (!ApplicationState::currentProject->isSaved()) {
            state._internalData.openFileClicked = true;
            state._internalData.path = filepath;
            ImGui::OpenPopup(Popups::PopupIds::unsavedProjectWarning.c_str());
        } else {
            ApplicationState::loadProject(filepath);
        }
    }

    void UIMain::onSaveProject() {}

} // namespace Bess::UI
