# sloppaTV visual redesign plan

## Goals

- Make every screen legible from normal couch distance at 1080p without relying on the UI text-size override.
- Make focus unmistakable at a glance with scale, outline, glow and tonal change rather than a thin outline alone.
- Reduce visual noise so artwork and primary actions dominate.
- Keep navigation predictable and D-pad friendly.
- Keep the visual language recognizably Jellyfin/media-center-like without copying the official client screen-for-screen.
- Preserve current native GLES performance characteristics and avoid heavyweight UI dependencies.

## Global design system

### Typography

Adopt a small set of reusable roles instead of per-screen ad-hoc scales:

- Display/title: 42-52 px equivalent at 1080p.
- Screen/section heading: 30-36 px.
- Card title: 24-28 px, medium weight.
- Metadata/body: 20-24 px.
- Supporting labels: 18-20 px minimum.
- Avoid tiny all-caps helper copy for important instructions.
- Use regular/medium weight for most text and reserve muted thin text for genuinely secondary metadata.

### Color and surfaces

- Keep the near-black base but separate layers more clearly.
- Background: near-black.
- Primary surface: dark charcoal.
- Elevated/focused surface: lighter charcoal with a subtle purple tint.
- Brand focus/accent: current purple family, slightly brighter than today.
- Text: near-white primary, cool gray secondary, muted gray tertiary.
- Destructive actions: red only when the destructive action is focused or confirmed.
- Backdrops should always receive a predictable dark gradient so text contrast never depends on the image.

### Focus language

Every focusable element should combine at least three cues:

1. 1.04-1.06x scale.
2. Bright 4-6 px outline with a small inset gap.
3. Soft purple glow/elevation.
4. Tonal surface change where appropriate.

Focused cards should lift above neighbours rather than simply receiving a rectangular stroke. Pressed state should briefly reduce scale to around 1.01-1.02x.

### Spacing and layout

- Establish a consistent 64-80 px 1080p safe margin before user-configurable overscan is applied.
- Use a fixed spacing rhythm rather than unique gaps on every screen.
- Prefer artwork-led clusters/rows over grids of gray rectangular containers.
- Keep one dominant interaction region per screen; avoid splitting attention between distant panels.
- Use gradients over full-screen backdrops instead of large opaque panels where content remains readable.

### Motion

- Focus transition: 120-160 ms ease-out.
- Screen/overlay transition: 160-220 ms.
- Avoid movement for static decorative elements.
- Respect reduced-motion if an accessibility setting is added later.

## Screen redesigns

### Home

Current issues:

- The My Media tiles are visually stronger than the content below, while Continue Watching and Next Up still feel crowded.
- Card captions and metadata are too small relative to artwork.
- Row headings need stronger hierarchy.
- Focus is visible but not premium-looking.

Redesign:

- Keep the three large My Media tiles, but use edge-to-edge imagery with a bottom gradient and title rather than centered floating text over a flat tint.
- Use large landscape cards for Continue Watching and Next Up.
- Show only title plus one useful secondary line per card. Move ratings and technical labels out of the card row.
- Make focused cards scale and glow, with the title becoming brighter and slightly larger.
- Increase row-heading size and spacing between rows.
- Keep the top navigation compact, but enlarge labels and give the active destination a stronger filled-pill treatment.
- Make the profile avatar a circular image/button instead of a small square initial tile when artwork exists.

### Movies and Shows browse

Current issues:

- The split artwork/text cards look like admin panels.
- Four-column layout is readable in size but wastes much of each card on dark rectangles.
- Titles wrap awkwardly because text is constrained to a narrow right-hand column.
- Filter tabs are visually weak.

Redesign:

- Keep four columns, but make artwork the dominant card surface.
- Movies/series: use large portrait artwork with title and one metadata line beneath it.
- Episodes: use wide 16:9 artwork with title and episode code beneath.
- Remove the dark text box attached to the right side of each poster.
- Use a clean horizontal filter-chip row: All, Favorites, Genres, A-Z, Collections.
- Selected filter = filled purple pill; focused filter = brighter scale/glow state.
- Make A-Z and genre pages use large text tiles with much larger labels; imagery is unnecessary there.
- Use empty-state content centered in the content region instead of tiny NO ITEMS text near the top-left.

### Collections

Current issues:

- Collection cards are mostly empty gray rectangles with undersized posters.

Redesign:

- Prefer collection backdrops when available; otherwise build a poster-collage tile from representative items.
- Use a 3-column landscape grid or 4-column large cards depending on available artwork.
- Put collection name in a bottom gradient overlay in large type.
- Focused collection card should visibly lift and brighten.

### Details: movies and series

Current issues:

- The backdrop is useful, but the screen still contains too much small text and too many equal-weight buttons.
- Overview, metadata, cast and actions compete for attention.
- Buttons look like generic gray rectangles.

Redesign:

- Full-screen backdrop with a strong left-to-right and bottom gradient.
- Poster on the left; logo/title and metadata on the right.
- Metadata becomes compact chips: year, rating, runtime, score.
- Overview limited to 3-4 large lines; optionally reveal full text through More.
- Primary action becomes a large purple Play/Resume button.
- Keep Episodes as a prominent secondary action for series.
- Convert Favorite, Watched and Cast to smaller icon/pill actions.
- Move Refresh/Delete and other maintenance actions entirely into More.
- Make More Like This a clean horizontal artwork row rather than tiny tiles at the very bottom.

### Episode details

Current issues:

- The 16:9 artwork treatment is already better than the old portrait layout, but the title hierarchy is still weak.

Redesign:

- Enlarge the episode image and use the series title as the main heading.
- Put `S1 E1 · Episode title` immediately underneath in large text.
- Keep overview short and readable.
- Keep Resume/Play dominant and de-emphasize maintenance actions.
- Maintain a clear Back destination rather than presenting Back as a peer action button.

### Seasons and Episodes

Current issues:

- Functional but still visually resembles a data grid.

Redesign:

- Seasons: use large poster cards with season name and episode count.
- Episodes: use 16:9 image cards, large episode title, and S/E code plus runtime beneath.
- Add progress bars directly beneath episode artwork when partially watched.
- Show watched/favorite state as small corner badges rather than additional text rows.

### Cast

Current issues:

- Portraits are squeezed into rectangular text panels.
- Names and roles are too small.

Redesign:

- Use portrait-first cards with a 2:3 image or circular/rounded portrait crop.
- Name underneath at card-title size, role underneath at metadata size.
- Five cards across is preferable if the resulting name text remains comfortably readable; otherwise four.
- Focus enlarges the portrait and brightens the name.

### Person titles

- Reuse the redesigned media cards from Movies/Shows instead of maintaining a separate visual style.
- Use the person's name as a strong section heading and optionally show a portrait in the header.

### Search

Current issues:

- Too much unused black space.
- Search field and keyboard work, but results feel detached from the search interaction.

Redesign:

- Large search field across the top with search/voice iconography.
- Results immediately below in the standard media-card system.
- With the Android keyboard visible, retain a compact result row above it when possible.
- Show a centered empty state with clear wording instead of small top-left helper text.
- Preserve the real Android TV keyboard as the primary entry method.

### Settings

Current issues:

- One of the least polished screens: large dark rows but comparatively small labels/values.
- Twenty-four settings form one undifferentiated list.
- Search works but visual grouping is weak.

Redesign:

- Keep a single-column navigation flow to avoid bouncing focus across distant panes.
- Group settings with visible section headers: Playback, Video, Audio, Subtitles, Appearance, System, Account.
- Increase setting label and value sizes substantially.
- Use shorter rows with more internal padding rather than giant empty bars.
- Display the value as a right-aligned pill/toggle/choice chip.
- Add a one-line description only where the label/value is not self-explanatory.
- Keep the search field at the top, but style it as a prominent rounded input surface.
- Diagnostics and Switch User should be action rows in the System/Account sections, not visually identical to numeric settings.

### Diagnostics

Current issues:

- Useful information is rendered like tiny debug text.

Redesign:

- Group into cards: App & Server, Video, Audio & Display, Last Playback.
- Use two columns on 1080p where it improves scanability.
- Labels medium gray, values bright white.
- Highlight warnings/capability limitations using restrained amber/red accents.
- Keep this screen technical, but no text should be smaller than the normal metadata role.

### Login

Current issues:

- Very sparse and developer-like.
- Branding and hierarchy are weak.

Redesign:

- Center a single login card over a subtle Jellyfin/server-style gradient backdrop.
- Large sloppaTV mark/title at top.
- Inputs become large rounded fields with clear labels.
- Primary Log In button is purple and full-width or strongly dominant.
- Quick Connect and Discover become secondary buttons.
- Saved Users becomes a separate low-emphasis link/action.

### Quick Connect

- Make the six-digit code the visual centerpiece in very large numerals.
- Put concise two-step instructions beneath it.
- Use one quiet Cancel/Back affordance.
- Avoid small diagnostic/status copy unless an error occurs.

### Users & Servers

Current issues:

- Rows are readable but visually flat and over-wide.

Redesign:

- Use large profile avatars and server/user text.
- Make `Use` the obvious focused action.
- Move `Forget` behind an overflow/context action unless explicitly focused.
- Add Another Account becomes a large final card/row with a plus icon.

### Item options and delete confirmation

Current issues:

- Full-screen options feel heavy for a contextual action.
- Destructive confirmation occupies a lot of space but still uses small text.

Redesign:

- Use a right-side overlay or compact centered panel over the existing backdrop.
- Put common actions in a short vertical list.
- Hide server-maintenance/destructive actions below the normal media actions.
- Delete confirmation should be a compact modal with large warning copy, one red destructive button and one neutral Cancel button.
- Keep Cancel focused by default.

### Player overlay

Current issues:

- Large opaque top/bottom bars hide too much video.
- The three controls are readable but still look like generic blocks.
- Helper text is too small.

Redesign:

- Replace hard bars with top/bottom gradients.
- Title at top-left; episode subtitle directly beneath.
- Progress bar becomes thicker and more prominent.
- Current/total time sits near the bar, not at distant screen edges.
- Center three large pill/icon controls for Play/Pause, Audio and Subtitles.
- Focused control scales/glows; unfocused controls remain translucent.
- Keep seek hints minimal and larger.
- Trickplay preview should appear directly above the progress position with a clear timestamp.
- Skip Intro/Credits should use a large high-contrast floating pill near the lower-right.
- Next Up should be a compact artwork card with countdown, not a text-only box.

### Playback queue

Current issues:

- The seven-action strip is the weakest part of the player UI: too many tiny buttons compete at once.
- Queue rows are readable but feel like a table.

Redesign:

- Keep queue as a large overlay, but make rows taller and artwork-aware.
- Current/Next badges become visual chips.
- Show three primary actions at once: Play Now, Play Next, Remove.
- Move reorder, shuffle and repeat into a More/options action or dedicated header controls.
- Keep action text large enough to read without scanning the entire bottom edge.
- Consider a right-side queue sheet on top of live video once the interaction model is stable.

### Screensaver

Current issues:

- Functional and clean, but the clock card is too small and framed like a diagnostic widget.

Redesign:

- Remove the rectangular panel and outline.
- Use a large sloppaTV wordmark with a much larger clock below it.
- Keep the whole group moving between safe positions every 30 seconds.
- Retain the dark background and first-key dismissal.

## Implementation order

### Phase 1: shared visual primitives

1. Typography roles.
2. Palette/surface tokens.
3. Focus animation and glow/outline helpers.
4. Standard media card components: portrait, landscape, text tile.
5. Buttons/pills/chips.
6. Backdrop gradient treatment.

Do this first so later screens stop reimplementing style constants independently.

### Phase 2: highest-visibility screens

1. Home.
2. Movies/Shows browse and filters.
3. Movie/Series/Episode Details.
4. Seasons/Episodes.
5. Search.

These screens define the product's visual identity and cover most normal use.

### Phase 3: playback

1. Player overlay.
2. Trickplay/skip/next-up states.
3. Queue.

Playback must keep full-frame video and avoid introducing frame-time regressions.

### Phase 4: utility and account screens

1. Settings.
2. Diagnostics.
3. Login/Quick Connect.
4. Users & Servers.
5. Item options/delete dialog.
6. Cast/person pages.
7. Screensaver.

## Acceptance criteria

For every redesigned surface:

- Capture a 1920x1080 Waydroid screenshot in default text size.
- Verify all primary text is comfortably readable at couch distance.
- Verify exactly one focus target is visually dominant.
- Check focused and unfocused states in screenshots.
- Verify navigation path remains predictable with D-pad only.
- Verify no clipping with Extra Large UI text + 6% safe area.
- Check a dark and bright backdrop/media image where applicable.
- Compare before/after screenshots side-by-side.
- Run the existing rapid-DPAD/frame-time benchmark after shared rendering/focus changes and after the player redesign.
- Keep the physical Google TV Streamer for final visual/performance validation after Waydroid acceptance.

## Recommended first implementation slice

Implement the shared visual tokens and media-card/focus primitives, then redesign Home + Movies + one Movie Details screen as a coherent visual vertical slice. Capture before/after screenshots of those three screens and adjust typography, spacing, focus glow and artwork density before propagating the design system across the remaining UI.
