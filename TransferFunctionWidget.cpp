#include "TransferFunctionWidget.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <unordered_map>

static struct {
    ImColor const WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
    ImColor const RED = { 1.0f, 0.0f, 0.0f, 1.0f };
    ImColor const GREEN = { 0.0f, 1.0f, 0.0f, 1.0f };
    ImColor const BLUE = { 0.0f, 0.0f, 1.0f, 1.0f };
    ImColor const GREY = { 0.5f, 0.5f, 0.5f, 1.0f };
} const IMCOLOR;

typedef struct {
    ImVec4 const Button;
    ImVec4 const ButtonHovered;
    ImVec4 const ButtonActive;
} StyleColor;

StyleColor const static STYLE_COLOR_RED = {
    ImColor::HSV(0.00f, 0.6f, 0.6f).Value,
    ImColor::HSV(0.00f, 0.7f, 0.7f).Value,
    ImColor::HSV(0.00f, 0.8f, 0.8f).Value,
};

StyleColor const static STYLE_COLOR_GREEN = {
    ImColor::HSV(0.33f, 0.6f, 0.6f).Value,
    ImColor::HSV(0.33f, 0.7f, 0.7f).Value,
    ImColor::HSV(0.33f, 0.8f, 0.8f).Value,
};

StyleColor const static STYLE_COLOR_BLUE = {
    ImColor::HSV(0.67f, 0.6f, 0.6f).Value,
    ImColor::HSV(0.67f, 0.7f, 0.7f).Value,
    ImColor::HSV(0.67f, 0.8f, 0.8f).Value,
};

StyleColor const static STYLE_COLOR_ALPHA = {
    ImColor::HSV(0.00f, 0.0f, 0.6f).Value,
    ImColor::HSV(0.00f, 0.0f, 0.7f).Value,
    ImColor::HSV(0.00f, 0.0f, 0.8f).Value,
};

static std::array<ImColor, 4> const HANDLE_COLOR = { IMCOLOR.RED, IMCOLOR.GREEN, IMCOLOR.BLUE, IMCOLOR.GREY };

TransferFunctionWidget::TransferFunctionWidget(int domainSize, float width, float height)
    : domain_(domainSize)
    , width_(width)
    , height_(height)
    , scaling_(1.6f)
    , handleRadius_(4.0f * scaling_)
    , handleBorder_(1.0f * scaling_)
    , isHandleCaptured_(false)
    , isFocusMode_(false)
    , groupActive_(GROUP_A)
    , handleActive_(0)
    , handleCountMax_(20)
    , margins_(2 * handleRadius_, 2 * handleRadius_)
    , label_("Transfer Function")
    , groups_ {}
{
    // Insert default positions of "Canvas coordinates" into groups.
    // Given the max domain and the max range, all "Canvas coordinates" are normalized and lie between (0,0) and (1,1).

    // Red
    groups_[GROUP_R].emplace_back(0.625f, 0.38f);
    groups_[GROUP_R].emplace_back(0.75f, 0.5f);
    groups_[GROUP_R].emplace_back(0.875f, 0.75f);
    groups_[GROUP_R].emplace_back(1.0f, 1.0f);
    // Green
    groups_[GROUP_G].emplace_back(0.0f, 0.0f);
    groups_[GROUP_G].emplace_back(0.125f, 0.85f);
    groups_[GROUP_G].emplace_back(0.25f, 1.0f);
    groups_[GROUP_G].emplace_back(0.5f, 0.88f);
    groups_[GROUP_G].emplace_back(0.75f, 0.0f);
    groups_[GROUP_G].emplace_back(1.0f, 0.0f);
    // Blue
    groups_[GROUP_B].emplace_back(0.0f, 1.0f);
    groups_[GROUP_B].emplace_back(0.125f, 0.5f);
    groups_[GROUP_B].emplace_back(0.24f, 0.25f);
    groups_[GROUP_B].emplace_back(0.38f, 0.125f);
    // Alpha
    groups_[GROUP_A].emplace_back(0.0f, 0.0625f);
    groups_[GROUP_A].emplace_back(0.25f, 0.0625f);
    groups_[GROUP_A].emplace_back(0.5f, 0.0625f);
    groups_[GROUP_A].emplace_back(0.75f, 0.0625f);
    groups_[GROUP_A].emplace_back(1.0f, 0.0625f);
}

bool TransferFunctionWidget::show()
{
    ImVec2 const btnSize(80, 28);

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Button, STYLE_COLOR_RED.Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, STYLE_COLOR_RED.ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, STYLE_COLOR_RED.ButtonActive);
    if (ImGui::Button("Red", btnSize)) {
        setActiveGroup(GROUP_R);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, STYLE_COLOR_GREEN.Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, STYLE_COLOR_GREEN.ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, STYLE_COLOR_GREEN.ButtonActive);
    if (ImGui::Button("Green", btnSize)) {
        setActiveGroup(GROUP_G);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, STYLE_COLOR_BLUE.Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, STYLE_COLOR_BLUE.ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, STYLE_COLOR_BLUE.ButtonActive);
    if (ImGui::Button("Blue", btnSize)) {
        setActiveGroup(GROUP_B);
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, STYLE_COLOR_ALPHA.Button);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, STYLE_COLOR_ALPHA.ButtonHovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, STYLE_COLOR_ALPHA.ButtonActive);
    if (ImGui::Button("Alpha", btnSize)) {
        setActiveGroup(GROUP_A);
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleColor(3);
    ImGui::Checkbox("Focus Mode", &isFocusMode_);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    // prepare canvas
    ImVec2 const canvasArea(width_ * scaling_, height_ * scaling_);
    origin_ = window->DC.CursorPos + margins_;
    ImRect bb(origin_, origin_ + canvasArea); // bounding-box
    ImRect hoverable(bb.Min - margins_, bb.Max + margins_);

    ImGui::ItemSize(hoverable);
    ImGui::ItemAdd(hoverable, 0);
    ImGuiID const windowId = window->GetID(label_.c_str());
    ImGui::ItemHoverable(hoverable, windowId);
    ImGui::RenderFrame(origin_, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg, 0.8f), true,
                       ImGui::GetStyle().FrameRounding);

    std::vector<ImVec2>& active = groups_[groupActive_];
    // Draw lines before drawing the handles
    for (std::size_t id = 0; id < groups_.size(); ++id) {
        // Draw all the groups except the active one.
        if (id == groupActive_ || groups_[id].empty())
            continue;
        ImColor lineColor = IMCOLOR.WHITE;
        auto const& group = groups_[id];
        if (isFocusMode_) {
            lineColor = colorWithAlpha(IMCOLOR.WHITE, 0.6f);
        }
        for (std::size_t p = 0; p < group.size() - 1; ++p) {
            drawList->AddLine(screenCoord(group[p]), screenCoord(group[p + 1]), lineColor, 1.2f);
        }
    }

    // Drawing handles
    for (std::size_t id = 0; id < groups_.size(); ++id) {
        // Draw all the groups except the active one.
        if (id == groupActive_ || groups_[id].empty())
            continue;
        ImColor handleColor = HANDLE_COLOR[id];
        ImColor lineColor = IMCOLOR.WHITE;
        if (isFocusMode_) {
            lineColor = colorWithAlpha(IMCOLOR.WHITE, 0.5f);
            handleColor = colorWithAlpha(HANDLE_COLOR[id], 0.5f);
        }
        for (auto const& p : groups_[id]) {
            ImVec2 const pos = screenCoord(p);
            drawList->AddCircleFilled(pos, handleRadius_, lineColor);
            drawList->AddCircleFilled(pos, handleRadius_ - handleBorder_, handleColor);
        }
    }

    // Draw the active group at the end so it always shows on top.
    auto const activeHandleColor = colorWithAlpha(HANDLE_COLOR[groupActive_], 0.6f);
    auto const activeRadius = handleRadius_ * 1.35f;
    for (std::size_t p = 0; p < active.size() - 1; ++p) {
        drawList->AddLine(screenCoord(active[p]), screenCoord(active[p + 1]), IMCOLOR.WHITE, 1.5f);
    }
    for (auto const& p : active) {
        ImVec2 const pos = screenCoord(p);
        drawList->AddCircleFilled(pos, activeRadius, IMCOLOR.WHITE);
        drawList->AddCircleFilled(pos, activeRadius - handleBorder_, activeHandleColor);
    }
    ImGui::EndGroup();

    return handleEvents();
}

inline bool TransferFunctionWidget::handleEvents()
{
    // Skip event-handling if the mouse is not even hovering over the canvas.
    if (!ImGui::IsItemHovered() && !isHandleCaptured_)
        return false;

    bool const isLeftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool const isRightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool const isLeftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool const isRightReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
    bool const isLeftDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    bool const isClicked = isLeftClicked || isRightClicked;
    bool const isReleased = isLeftReleased || isRightReleased;

    ImVec2 const mousePos = canvasCoord(ImGui::GetIO().MousePos);
    std::vector<ImVec2>& active = groups_[groupActive_];

    // A flag returned to check whether the transfer function is changed by user
    bool changed = false;
    float const radiusWithinReach = 4 * handleRadius_ * handleRadius_ / (width_ * width_ + height_ * height_);

    float distSqrMin = std::numeric_limits<float>::max();
    auto groupNear = groupActive_;
    auto handleNear = handleActive_;

    // The active group has priority to respond to events.

    // Find the handle closest to mouse from the active group.
    for (std::size_t i = 0; i < active.size(); ++i) {
        // Comparing distances between mouse and handles in "Canvas coordinates".
        ImVec2 const& d = mousePos - active[i];
        float const distSqr = d.x * d.x + d.y * d.y;
        if (distSqr < distSqrMin) {
            distSqrMin = distSqr;
            handleNear = i;
        }
    }
    // If the closest handle from the active group is reachable,
    if (distSqrMin < radiusWithinReach) {
        // show tooltip of the handle coordinates.
        ImGui::SetTooltip("(%.0f, %.4f)", domain_ * groups_[groupNear][handleNear].x, groups_[groupNear][handleNear].y);
        // And when clicking,
        if (isClicked) {
            // capture the handle.
            handleActive_ = handleNear;
            isHandleCaptured_ = true;
        }
    } else {
        // If none of the handles from the active group is within reach, we then check the other groups.
        // Find the handle that is closest to mouse from the other groups.
        for (std::size_t i = 0; i < groups_.size(); ++i) {
            if (groupActive_ == i)
                continue;
            for (std::size_t j = 0; j < groups_[i].size(); ++j) {
                ImVec2 const& d = mousePos - groups_[i][j];
                float const distSqr = d.x * d.x + d.y * d.y;
                if (distSqr < distSqrMin) {
                    distSqrMin = distSqr;
                    groupNear = i;
                    handleNear = j;
                }
            }
        }
        // If the closest handle from one of the other groups is reachable,
        if (distSqrMin < radiusWithinReach) {
            // show tooltip of the handle coordinates.
            ImGui::SetTooltip("(%.0f, %.4f)", domain_ * groups_[groupNear][handleNear].x,
                              groups_[groupNear][handleNear].y);
            // If the nearest group is the active one and a mouse button is clicked, capture the handle.
            if (isClicked) {
                if (groupNear == groupActive_) {
                    handleActive_ = handleNear;
                    isHandleCaptured_ = true;
                } else {
                    // If the nearest group is not active and left-button is clicked, change the active group and do not
                    // grab any handle. Else if focus mode is on, we do nothing.
                    if (isFocusMode_) {
                    } else {
                        if (isLeftClicked) {
                            setActiveGroup(groupNear);
                            isHandleCaptured_ = false;
                        }
                    }
                }
            }
        }
    }

    if (isReleased) {
        isHandleCaptured_ = false;
    }

    // Move, insert, or erase a handle from the active group.
    if (isHandleCaptured_) {
        // While a handle is captured by user, (The mouse event position is close enough to the handle).
        if (isRightClicked) {
            // Erase the handle by right button.
            if (active.size() > 1) {
                active.erase(active.begin() + handleActive_);
                isHandleCaptured_ = false;
                changed = true;
            }
        } else if (isLeftDragging) {
            // Move the handle around by dragging it with left button.
            // User does not lose the control of handle util the mouse button is released.
            ImVec2 posNext = mousePos;
            float xMax = 1.0f;
            float xMin = 0.0f;

            if (handleActive_ > 0)
                xMin = active[handleActive_ - 1].x;
            if (handleActive_ < active.size() - 1)
                xMax = active[handleActive_ + 1].x;

            if (posNext.x < xMin)
                posNext.x = xMin;
            else if (posNext.x > xMax)
                posNext.x = xMax;
            if (posNext.y < 0.0f)
                posNext.y = 0.0f;
            else if (posNext.y > 1.0f)
                posNext.y = 1.0f;

            active[handleActive_] = posNext;
            changed = true;
        }
    } else if (isRightClicked && active.size() < handleCountMax_) {
        // While there is no handle selected, pressing right button will create a new handle.
        if (mousePos.x > 0.0f && mousePos.x < 1.0f && mousePos.y > 0.0f && mousePos.y < 1.0f) {
            // Append the new handle (control point) at the end of the group
            active.emplace_back(mousePos.x, mousePos.y);
            // Sort all the handles by x-coordinate
            std::sort(active.begin(), active.end(), [](auto const& a, auto const& b) { return a.x < b.x; });
        }
        changed = true;
    }

    return changed;
}

void TransferFunctionWidget::setDomain(int domainSize)
{
    domain_ = domainSize;
}

std::vector<float> TransferFunctionWidget::transferFunction() const
{
    using namespace std;
    int const groupCount = groups_.size();
    vector<float> function(domain_ * groupCount, 0.0f);

    ImVec2 const scaleToDomain(domain_, 1.0f);

    for (auto groupId = 0; groupId < groupCount; ++groupId) {
        auto const& group = groups_[groupId];
        for (std::size_t idx = 1; idx < group.size(); ++idx) {
            ImVec2 const b = group[idx] * scaleToDomain;
            ImVec2 const a = group[idx - 1] * scaleToDomain;
            int const interval = static_cast<int>(round(b.x) - round(a.x));
            float const slope = (b.y - a.y) / interval;

            for (int handleId = 0; handleId < interval; ++handleId) {
                int const x = groupId + groupCount * (handleId + static_cast<int>(round(a.x)));
                function[x] = (handleId * slope + a.y);
            }
        }
    }

    return function;
}

inline void TransferFunctionWidget::setActiveGroup(int groupId)
{
    groupActive_ = groupId;
}

/**
 * Accept an ImColor and return it with specified alpha value.
 *
 * @param color  The ImColor on which the returned ImColor based.
 * @param alpha  The alpha value of the new ImColor.
 *
 * @return  A new ImColor with RGB values of @param color and A value from @param alpha.
 */
inline ImColor TransferFunctionWidget::colorWithAlpha(ImColor const& color, float alpha) const
{
    assert(alpha >= 0.0f && alpha <= 1.0f);
    return ImColor { color.Value.x, color.Value.y, color.Value.z, alpha };
}

/**
 * A helper function that maps Screen coordinates to Canvas coordinates
 *
 * @param pos  Position in Screen coordinates
 * @return The corresponding position in Canvas coordinates
 */
inline ImVec2 TransferFunctionWidget::canvasCoord(ImVec2 const& pos) const
{
    float const height = height_ * scaling_;
    float const px = (pos.x - origin_.x) / scaling_;
    float const py = -((pos.y - origin_.y - height) / scaling_);
    return ImVec2(px / width_, py / height_);
}

/**
 * A helper function that maps Canvas coordinates to Screen coordinates
 *
 * @param pos  Position in Canvas coordinates
 * @return The corresponding position in Screen coordinates
 */
inline ImVec2 TransferFunctionWidget::screenCoord(ImVec2 const& pos) const
{
    float const height = height_ * scaling_;
    return (ImVec2((pos.x * width_) * scaling_, (height - (pos.y * height_) * scaling_)) + origin_);
}

void TransferFunctionWidget::exportToFile(std::string const& dataName) const
{
    using namespace std;

    auto const colSize = groups_.size();
    auto const function = transferFunction();

    stringstream maxValueStr;
    maxValueStr << domain_;
    int const valColWidth = maxValueStr.str().size() + 1;

    ofstream file;
    file.open(dataName + "_TF.dat");
    file << "# Transfer Function\n";
    file << "# Value    R    G    B    A\n";
    file << fixed << setprecision(4);
    for (int row = 0; row < domain_; ++row) {
        file << setw(valColWidth) << row;
        for (std::size_t col = 0; col < colSize; ++col) {
            file << setw(8) << function[row * colSize + col];
        }
        file << "\n";
    }
    file.close();
}
