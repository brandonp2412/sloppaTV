#pragma once

#include "app_screen_presentation.hpp"
#include "content_mutation_flow.hpp"
#include "home_visibility.hpp"
#include "navigation_stack.hpp"
#include "settings_flow.hpp"
#include "system_text_input.hpp"
#include "system_text_input_controller.hpp"

#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

template <typename Api, typename Ui, typename Artwork, typename SearchFlowT, typename SeerrAppT, typename HomeScreensT>
class AppFrameCoordinator {
public:
    AppFrameCoordinator(std::recursive_mutex& stateMutex, Renderer& renderer, Api& api, Screen& screen,
                        NavigationStack<Screen>& navigation, bool& screensaverActive, bool& loading, bool& homeLoading,
                        ContentMutationFlow& contentMutation, AppSettings& settings, SeerrAppT& seerrApp,
                        SystemTextInputController& textInput, HomeVisibility& homeVisibility,
                        VirtualKeyboardState& keyboard, HomeScreensT& homeScreens,
                        std::chrono::steady_clock::time_point& lastInteraction, std::string& error, Ui& ui,
                        Artwork& artwork, BrandMark& brandMark, AccountFlow& accountFlow, JellyfinHomeData& home,
                        HomeScreenState& homeState, JellyfinSession& session, BrowseScreenState& browseState,
                        SearchFlowT& searchFlow, SeerrDomainState& seerrDomain, SettingsFlow& settingsFlow,
                        JellyfinServerInfo& serverInfo, DetailsFlow& detailsFlow, NativeMediaPlayer& player,
                        VideoSurface& videoSurface, PlaybackCoordinator& playbackCoordinator,
                        PlayerScreenState& playerScreenState, TrickplayPreviewState& trickplayState,
                        PlaybackQueueState& queueState, StatusOverlayState& statusOverlayState)
        : stateMutex_(stateMutex), renderer_(renderer), api_(api), screen_(screen), navigation_(navigation),
          screensaverActive_(screensaverActive), loading_(loading), homeLoading_(homeLoading),
          contentMutation_(contentMutation), settings_(settings), seerrApp_(seerrApp), textInput_(textInput),
          homeVisibility_(homeVisibility), keyboard_(keyboard), homeScreens_(homeScreens),
          lastInteraction_(lastInteraction), error_(error), ui_(ui), artwork_(artwork), brandMark_(brandMark),
          accountFlow_(accountFlow), home_(home), homeState_(homeState), session_(session), browseState_(browseState),
          searchFlow_(searchFlow), seerrDomain_(seerrDomain), settingsFlow_(settingsFlow), serverInfo_(serverInfo),
          detailsFlow_(detailsFlow), player_(player), videoSurface_(videoSurface),
          playbackCoordinator_(playbackCoordinator), playerScreenState_(playerScreenState),
          trickplayState_(trickplayState), queueState_(queueState), statusOverlayState_(statusOverlayState) {}

    void render(std::string_view appVersion) {
        std::scoped_lock lock(stateMutex_);
        renderer_.beginFrame();

        DeviceCodecSupport codecSupport;
        if (screen_ == Screen::Settings || screen_ == Screen::Diagnostics) codecSupport = api_.deviceCodecSupport();

        renderAppScreenFrame(AppScreenPresentationFrame<Ui, Artwork>{
            .screen = screen_,
            .backgroundScreen = navigation_.previousOr(Screen::Details),
            .screensaverActive = screensaverActive_,
            .loading = loading_,
            .homeLoading = homeLoading_,
            .mutationLoading = contentMutation_.loading(),
            .seerrConfigured = seerrApp_.endpoint().configured(),
            .systemSearchInputActive = textInput_.mode() == kTextInputSearch,
            .systemSettingsSearchActive = textInput_.mode() == kTextInputSettingsSearch,
            .systemSeerrApiKeyActive = textInput_.mode() == kTextInputSeerrApiKey,
            .itemHiddenFromHome = homeVisibility_.isHidden(detailsFlow_.item()),
            .keyboardRow = keyboard_.row(),
            .keyboardCol = keyboard_.column(),
            .homeSlideFromFirst = homeScreens_.slideFromFirst(),
            .homeSlideToFirst = homeScreens_.slideToFirst(),
            .homeSlideStarted = homeScreens_.slideStarted(),
            .nextUpReplacementFadeIndex = contentMutation_.nextUpReplacementFadeIndex(),
            .nextUpReplacementFadeItemId = contentMutation_.nextUpReplacementFadeItemId(),
            .nextUpReplacementFadeStarted = contentMutation_.nextUpReplacementFadeStarted(),
            .lastInteraction = lastInteraction_,
            .appVersion = appVersion,
            .error = error_,
            .renderer = renderer_,
            .ui = ui_,
            .artwork = artwork_,
            .brandMark = brandMark_,
            .accountFlow = accountFlow_,
            .home = home_,
            .homeState = homeState_,
            .session = session_,
            .settings = settings_,
            .browseState = browseState_,
            .searchState = searchFlow_.state(),
            .seerrDomain = seerrDomain_,
            .settingsScreen = settingsFlow_.state(),
            .externalPlayers = settingsFlow_.externalPlayers(),
            .deviceCodecSupport = codecSupport,
            .serverInfo = serverInfo_,
            .detailsFlow = detailsFlow_,
            .player = player_,
            .videoSurface = videoSurface_,
            .playbackCoordinator = playbackCoordinator_,
            .playerScreenState = playerScreenState_,
            .trickplayState = trickplayState_,
            .queueState = queueState_,
            .statusOverlayState = statusOverlayState_,
        });

        renderer_.endFrame();
    }

private:
    std::recursive_mutex& stateMutex_;
    Renderer& renderer_;
    Api& api_;
    Screen& screen_;
    NavigationStack<Screen>& navigation_;
    bool& screensaverActive_;
    bool& loading_;
    bool& homeLoading_;
    ContentMutationFlow& contentMutation_;
    AppSettings& settings_;
    SeerrAppT& seerrApp_;
    SystemTextInputController& textInput_;
    HomeVisibility& homeVisibility_;
    VirtualKeyboardState& keyboard_;
    HomeScreensT& homeScreens_;
    std::chrono::steady_clock::time_point& lastInteraction_;
    std::string& error_;
    Ui& ui_;
    Artwork& artwork_;
    BrandMark& brandMark_;
    AccountFlow& accountFlow_;
    JellyfinHomeData& home_;
    HomeScreenState& homeState_;
    JellyfinSession& session_;
    BrowseScreenState& browseState_;
    SearchFlowT& searchFlow_;
    SeerrDomainState& seerrDomain_;
    SettingsFlow& settingsFlow_;
    JellyfinServerInfo& serverInfo_;
    DetailsFlow& detailsFlow_;
    NativeMediaPlayer& player_;
    VideoSurface& videoSurface_;
    PlaybackCoordinator& playbackCoordinator_;
    PlayerScreenState& playerScreenState_;
    TrickplayPreviewState& trickplayState_;
    PlaybackQueueState& queueState_;
    StatusOverlayState& statusOverlayState_;
};
