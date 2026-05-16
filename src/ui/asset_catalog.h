#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace jarvis {

struct AssetSummary {
    std::size_t fonts = 0;
    std::size_t icons = 0;
    std::size_t sounds = 0;
};

class AssetCatalog {
public:
    explicit AssetCatalog(std::filesystem::path root);
    AssetSummary scan() const;
    std::string summaryText() const;
    const std::filesystem::path& root() const;
    std::vector<std::filesystem::path> fontFiles() const;
    std::vector<std::filesystem::path> iconFiles() const;
    std::vector<std::filesystem::path> soundFiles() const;
    std::filesystem::path soundFileByStem(std::string_view stem) const;

private:
    std::vector<std::filesystem::path> filesIn(const std::filesystem::path& path) const;

    std::filesystem::path root_;
};

}  // namespace jarvis
