#pragma once

#include "browse_async_executor.hpp"
#include "browse_screen.hpp"

#include <optional>
#include <string>
#include <string_view>

struct BrowseCompletionEffects {
    bool finishLoading = false;
    bool clearError = false;
    bool prefetchArtwork = false;
    std::optional<std::string> error;
};

class BrowseCompletionController {
public:
    [[nodiscard]] static BrowseCompletionEffects apply(BrowsePageCompletion& completion, bool active,
                                                       bool browseScreenActive, std::string_view activeContainerId,
                                                       BrowseScreenState& state, int pageSize);
};
