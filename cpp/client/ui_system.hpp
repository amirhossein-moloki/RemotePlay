#pragma once

#include "launcher.hpp"
#include <string>
#include <vector>

namespace Client::UI {

enum class Severity { Info, Success, Warning, Error };

struct ValidationMessage {
    std::string field;
    std::string message;
    Severity severity = Severity::Error;
};

struct Notification {
    Severity severity = Severity::Info;
    std::string title;
    std::string message;
};

struct DesignTokens {
    float spaceXs = 4.0f;
    float spaceSm = 8.0f;
    float spaceMd = 12.0f;
    float spaceLg = 16.0f;
    float spaceXl = 24.0f;
    float radiusSm = 6.0f;
    float radiusMd = 10.0f;
    float radiusLg = 14.0f;
    float controlHeight = 40.0f;
    float primaryActionHeight = 46.0f;
};

const DesignTokens& Tokens();
void ApplyTheme();
std::vector<ValidationMessage> ValidateLauncherConfig(const LauncherConfig& config, const std::vector<Network::InterfaceInfo>& interfaces);
bool HasBlockingErrors(const std::vector<ValidationMessage>& messages);

#ifdef _WIN32
void SectionHeader(const char* title, const char* description);
void BeginCard(const char* id, float height);
void EndCard();
void RenderNotification(const Notification& notification);
void RenderValidationMessages(const std::vector<ValidationMessage>& messages, const char* field);
#endif

} // namespace Client::UI
