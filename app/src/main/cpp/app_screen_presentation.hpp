#pragma once

#include "account_flow.hpp"
#include "app_screen.hpp"
#include "brand_mark.hpp"
#include "content_screen_presentation.hpp"
#include "device_capabilities.hpp"
#include "details_flow.hpp"
#include "external_player_types.hpp"
#include "player_presentation.hpp"
#include "primary_screen_presentation.hpp"
#include "screen_presentation.hpp"
#include "seerr_domain.hpp"
#include "settings_action_controller.hpp"
#include "status_overlay_renderer.hpp"
#include "virtual_keyboard.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

template <typename UiLike, typename ArtworkLike> struct AppScreenPresentationFrame {
    Screen screen = Screen::Login;
    Screen backgroundScreen = Screen::Details;
    bool screensaverActive = false;
    bool loading = false;
    bool homeLoading = false;
    bool mutationLoading = false;
    bool seerrConfigured = false;
    bool systemSearchInputActive = false;
    bool systemSettingsSearchActive = false;
    bool systemSeerrApiKeyActive = false;
    bool itemHiddenFromHome = false;
    int keyboardRow = 0;
    int keyboardCol = 0;
    int homeSlideFromFirst = 0;
    int homeSlideToFirst = 0;
    std::chrono::steady_clock::time_point homeSlideStarted{};
    int nextUpReplacementFadeIndex = -1;
    // cppcheck-suppress uninitMemberVarNoCtor -- required aggregate reference, supplied by the frame builder.
    const std::string& nextUpReplacementFadeItemId;
    std::chrono::steady_clock::time_point nextUpReplacementFadeStarted{};
    std::chrono::steady_clock::time_point lastInteraction{};
    std::string_view appVersion;
    std::string_view error;

    Renderer& renderer;
    // cppcheck-suppress uninitMemberVarNoCtor -- required aggregate reference, supplied by the frame builder.
    UiLike& ui;
    // cppcheck-suppress uninitMemberVarNoCtor -- required aggregate reference, supplied by the frame builder.
    ArtworkLike& artwork;
    BrandMark& brandMark;
    const AccountFlow& accountFlow;
    const JellyfinHomeData& home;
    const HomeScreenState& homeState;
    const JellyfinSession& session;
    const AppSettings& settings;
    const BrowseScreenState& browseState;
    const SearchScreenState& searchState;
    const SeerrDomainState& seerrDomain;
    const SettingsScreenState& settingsScreen;
    // cppcheck-suppress uninitMemberVarNoCtor -- required aggregate reference, supplied by the frame builder.
    const std::vector<ExternalPlayerApp>& externalPlayers;
    const DeviceCodecSupport& deviceCodecSupport;
    const JellyfinServerInfo& serverInfo;
    const DetailsFlow& detailsFlow;
    NativeMediaPlayer& player;
    VideoSurface& videoSurface;
    PlaybackCoordinator& playbackCoordinator;
    PlayerScreenState& playerScreenState;
    TrickplayPreviewState& trickplayState;
    PlaybackQueueState& queueState;
    StatusOverlayState& statusOverlayState;
};

template <typename UiLike, typename ArtworkLike>
void renderAppScreenFrame(AppScreenPresentationFrame<UiLike, ArtworkLike> frame) {
    auto renderKeyboard = [&](float top) {
        renderKeyboardPresentation(frame.renderer, frame.ui, keyboardRows(), frame.keyboardRow, frame.keyboardCol, top,
                                   Renderer::logicalHeight());
    };

    if (frame.screensaverActive) {
        frame.renderer.setUiTransform(0.0f, 1.0f);
        renderScreensaverPresentation(frame.renderer, frame.ui, frame.settings.clock24Hour, Renderer::logicalWidth(),
                                      Renderer::logicalHeight());
        return;
    }

    frame.renderer.setUiTransform(uiSafeAreaFraction(frame.settings.safeAreaPercent),
                                  uiTextScale(frame.settings.uiTextSize));
    frame.ui.beginFrame(frame.session, frame.settings, frame.lastInteraction,
                        frame.screen == Screen::Home && frame.homeState.centerPending());

    auto renderScreen = [&](Screen target) {
        switch (target) {
        case Screen::Login:
            renderLoginPresentation(frame.renderer, frame.ui, frame.accountFlow.state(), frame.loading,
                                    static_cast<int>(frame.accountFlow.savedSessionCount()), Renderer::logicalWidth(),
                                    Renderer::logicalHeight(), renderKeyboard);
            break;
        case Screen::Profiles:
            renderProfilesPresentation(
                frame.renderer, frame.ui, static_cast<int>(frame.accountFlow.savedSessionCount()),
                frame.accountFlow.state().profileSelection(), frame.accountFlow.state().profileAction(),
                [&](int index) { return frame.accountFlow.savedSession(static_cast<size_t>(index)); });
            break;
        case Screen::Home:
            renderContentHome(
                frame.renderer, frame.ui, frame.home, frame.homeState, frame.session, frame.settings, frame.homeLoading,
                frame.homeSlideFromFirst, frame.homeSlideToFirst, frame.homeSlideStarted,
                frame.nextUpReplacementFadeIndex, frame.nextUpReplacementFadeItemId, frame.nextUpReplacementFadeStarted,
                [&](float x, float y, float size) { return frame.brandMark.draw(frame.renderer, x, y, size); });
            break;
        case Screen::Browse:
            renderContentBrowse(frame.renderer, frame.ui, frame.browseState, frame.settings, frame.loading);
            break;
        case Screen::Search:
            renderContentSearch(frame.renderer, frame.ui, frame.searchState, frame.settings, frame.seerrConfigured,
                                frame.systemSearchInputActive, frame.seerrDomain.searchLoading(),
                                frame.seerrDomain.searchError(), frame.seerrDomain.storageLoading(),
                                frame.seerrDomain.storageTargets(), frame.lastInteraction, renderKeyboard);
            break;
        case Screen::Settings: {
            const std::string externalPlayer =
                SettingsActionController::externalPlayerLabel(frame.settings, frame.externalPlayers);
            renderSettingsPresentation(frame.renderer, frame.ui, frame.settingsScreen, frame.settings,
                                       frame.deviceCodecSupport.maxAudioOutputChannels, externalPlayer,
                                       frame.session.username, frame.systemSettingsSearchActive,
                                       frame.systemSeerrApiKeyActive);
            break;
        }
        case Screen::Diagnostics:
            renderDiagnosticsPresentation(frame.renderer, frame.ui, frame.deviceCodecSupport, frame.appVersion,
                                          frame.session.server, frame.serverInfo.name, frame.serverInfo.version,
                                          frame.loading, frame.playbackCoordinator.session().lastPlaybackSummary());
            break;
        case Screen::Details:
        case Screen::ItemMenu:
            renderContentDetails(
                frame.renderer, frame.ui, frame.detailsFlow.item(), frame.detailsFlow.state(),
                frame.detailsFlow.detailActions(frame.playbackCoordinator.continuation().stillWatchingPrompt()),
                frame.settings, frame.playbackCoordinator.continuation().stillWatchingPrompt(),
                frame.screen == Screen::ItemMenu);
            break;
        case Screen::Cast:
            renderContentCast(frame.renderer, frame.ui, frame.detailsFlow.item(), frame.detailsFlow.state(),
                              frame.settings);
            break;
        case Screen::PersonItems:
            renderContentPersonItems(frame.renderer, frame.ui, frame.detailsFlow.state(), frame.settings,
                                     frame.loading);
            break;
        case Screen::Seasons:
            renderContentSeasons(frame.renderer, frame.ui, frame.detailsFlow.state(), frame.settings, frame.loading);
            break;
        case Screen::Episodes:
            renderContentEpisodes(frame.renderer, frame.ui, frame.detailsFlow.state(), frame.settings, frame.loading);
            break;
        case Screen::SeerrDrivePicker:
            renderContentSeerrDrivePicker(frame.renderer, frame.ui,
                                          seerrDrivePickerViewModel(frame.seerrDomain.pendingStorageRequest(),
                                                                    frame.seerrDomain.storageDriveChoices(),
                                                                    frame.seerrDomain.storageDriveSelection()));
            break;
        case Screen::Player:
            renderPlayerPresentation(frame.renderer, frame.player, frame.videoSurface, frame.playbackCoordinator,
                                     frame.playerScreenState, frame.trickplayState, frame.settings, frame.session,
                                     frame.artwork, frame.lastInteraction);
            break;
        }
    };

    if (frame.screen == Screen::ItemMenu) {
        renderScreen(frame.backgroundScreen);
        std::vector<std::string> actions;
        if (!frame.detailsFlow.state().deleteConfirmation()) {
            actions = frame.detailsFlow.itemMenuActions(
                isSeerrItem(frame.detailsFlow.item()),
                SettingsActionController::selectedExternalPlayer(frame.settings, frame.externalPlayers).has_value(),
                !frame.queueState.empty(), frame.itemHiddenFromHome);
        }
        renderItemMenuPresentation(frame.renderer, frame.ui, frame.detailsFlow.item(), frame.detailsFlow.state(),
                                   actions, Renderer::logicalWidth(), Renderer::logicalHeight());
    } else {
        renderScreen(frame.screen);
    }

    if (frame.queueState.overlayActive()) {
        renderQueueOverlayPresentation(frame.renderer, frame.ui, frame.queueState);
    }

    const StatusOverlayRenderState status =
        frame.statusOverlayState.renderState(frame.error, frame.loading || frame.homeLoading || frame.mutationLoading,
                                             frame.screen == Screen::Player, std::chrono::steady_clock::now());
    renderStatusOverlay(
        frame.renderer, status,
        StatusOverlayRenderStyle<Color>{
            .cornerMedium = material_tv::cornerMedium,
            .panelElevated = material_tv::surfaceContainerHigh,
            .focus = material_tv::primary,
            .error = material_tv::error,
            .errorOutline = Color{material_tv::error.r, material_tv::error.g, material_tv::error.b, 0.55f},
            .text = material_tv::onSurface,
        },
        [&](std::string_view value, float scale, float width, int lines) {
            return frame.ui.fitTextLines(value, scale, width, lines);
        });
}
