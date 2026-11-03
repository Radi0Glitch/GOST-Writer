#pragma once

#include <SDL3_ttf/SDL_ttf.h>

#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace md {

enum class Family { Sans = 0, Mono = 1, Serif = 2 };

// Loads font faces on demand and caches them by (family, bold, italic, strike, size).
// Owns every TTF_Font it returns.
class FontBook {
public:
    bool init(const std::string& dir);
    void clear();

    // Document zoom: every font requested through get() is multiplied by it.
    // Clamped to [0.5, 2.0] so the cache stays bounded.
    void set_zoom(float zoom);
    float zoom() const { return zoom_; }

    void set_base_pt(int pt) { base_pt_ = pt; }
    int base_pt() const { return base_pt_; }

    TTF_Font* get(Family family, bool bold, bool italic, float size, bool strike = false);
    TTF_Font* get_ref(Family family, bool bold, bool italic, float size, bool strike = false);

private:
    std::string dir_;
    float zoom_ = 1.0f;
    int base_pt_ = 14;
    std::map<std::tuple<int, bool, bool, bool, int>, TTF_Font*> cache_;
    // Fallback faces (arrows, superscripts) shared by every face of one size and style.
    std::map<std::tuple<bool, bool, bool, int>, std::vector<TTF_Font*>> fallbacks_;
};

} // namespace md
