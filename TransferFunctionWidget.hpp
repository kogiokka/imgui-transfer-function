#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

class TransferFunctionWidget
{
    enum { GROUP_R = 0, GROUP_G = 1, GROUP_B = 2, GROUP_A = 3 };

    int domain_;
    float width_;
    float height_;
    float scaling_;
    float handleRadius_;
    float handleBorder_;
    bool isHandleCaptured_;
    bool isFocusMode_;
    std::size_t groupActive_;
    std::size_t handleActive_;
    std::size_t handleCountMax_;
    ImVec2 margins_;
    ImVec2 origin_;
    std::string label_;
    std::array<std::vector<ImVec2>, 4> groups_;

    ImVec2 canvasCoord(ImVec2 const& pos) const;
    ImVec2 screenCoord(ImVec2 const& pos) const;
    ImColor colorWithAlpha(ImColor const& color, float alpha) const;
    void setActiveGroup(int groupId);
    bool handleEvents();

public:
    TransferFunctionWidget(int domainSize, float width = 256.0f, float height = 256.0f);
    bool show();
    void setDomain(int domainSize);
    std::vector<float> transferFunction() const;
    void exportToFile(std::string const& dataName) const;
};
