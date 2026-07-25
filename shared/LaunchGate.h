#pragma once

#include <filesystem>
#include <string>

namespace mc::launchgate {
std::filesystem::path CreateLaunchTicket(const std::filesystem::path& gameExecutable);
bool ValidateAndConsumeLaunchTicket(const std::filesystem::path& ticketPath, const std::filesystem::path& gameExecutable, std::wstring& error);
} // namespace mc::launchgate
