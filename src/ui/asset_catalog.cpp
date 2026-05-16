#include "ui/asset_catalog.h"

#include "util/common.h"

#include <algorithm>
#include <sstream>

namespace jarvis {

AssetCatalog::AssetCatalog(std::filesystem::path root) : root_(std::move(root)) {}

AssetSummary AssetCatalog::scan() const {
    AssetSummary summary;
    summary.fonts = fontFiles().size();
    summary.icons = iconFiles().size();
    summary.sounds = soundFiles().size();
    return summary;
}

std::string AssetCatalog::summaryText() const {
    const AssetSummary summary = scan();
    std::ostringstream out;
    out << "Assets"
        << " | fonts: " << summary.fonts
        << " | icons: " << summary.icons
        << " | sounds: " << summary.sounds;
    return out.str();
}

const std::filesystem::path& AssetCatalog::root() const {
    return root_;
}

std::vector<std::filesystem::path> AssetCatalog::fontFiles() const {
    return filesIn(root_ / "fonts");
}

std::vector<std::filesystem::path> AssetCatalog::iconFiles() const {
    return filesIn(root_ / "icons");
}

std::vector<std::filesystem::path> AssetCatalog::soundFiles() const {
    return filesIn(root_ / "sounds");
}

std::filesystem::path AssetCatalog::soundFileByStem(std::string_view stem) const {
    const std::string wanted = util::toLower(util::trim(stem));
    if (wanted.empty()) {
        return {};
    }

    for (const auto& file : soundFiles()) {
        if (util::toLower(file.stem().string()) == wanted) {
            return file;
        }
    }
    return {};
}

std::vector<std::filesystem::path> AssetCatalog::filesIn(const std::filesystem::path& path) const {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(path)) {
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace jarvis
