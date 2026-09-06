# Screenshot fixture media

These files exist only to make the automated Android TV screenshot suite exercise real artwork and video paths without depending on the network at test time.

The fixture uses Blender Foundation / Blender Studio open-movie material. Blender Studio describes its published production content as generally Creative Commons Attribution material, and the individual Commons pages below carry the specific licenses used here.

- `big-buck-bunny-poster.jpg` — Big Buck Bunny poster, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Big_buck_bunny_poster_big.jpg. Attribution: (c) copyright Blender Foundation | www.bigbuckbunny.org.
- `big-buck-bunny-backdrop.png` — frame from Big Buck Bunny, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:BBB-Bunny.png. Attribution: (c) copyright Blender Foundation | www.bigbuckbunny.org.
- `sintel-poster.jpg` — Sintel poster, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Sintel_poster.jpg. Attribution: © copyright Blender Foundation – durian.blender.org.
- `sintel-backdrop.png` — frame from Sintel, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Sintel_Screenshot_1.png. Attribution: © copyright Blender Foundation www.sintel.org.
- `tears-of-steel-poster.png` — Tears of Steel poster. Source: https://commons.wikimedia.org/wiki/File:Tos-poster.png. Attribution: (CC) Blender Foundation | Project Mango.
- `caminandes-backdrop.png` — frame from Caminandes: Gran Dillama, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Caminandes_gran_dillama.png. Attribution: Francesco Siddi and Pablo Vazquez / Blender Foundation.
- `elephants-dream-poster.jpg` — official Elephants Dream poster, CC BY 2.5. Source: https://commons.wikimedia.org/wiki/File:ElephantsDreamPoster.jpg. Attribution: Bassam Kurdali, Andy Goralczyk and Blender Foundation.
- `spring-poster.jpg` — Spring pillar poster, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:Spring2019PillarPosterBlender.jpg. Attribution: Blender Foundation / Blender Animation Studio.
- `spring-backdrop.jpg` — Spring final-scene frame, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:SpringOpenMovie-final_scene.jpg. Attribution: Blender Foundation / Blender Animation Studio.
- `coffee-run-poster.png` — Coffee Run poster, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:Coffee_Run-movie_poster.png. Attribution: Blender Foundation / Blender Animation Studio.
- `coffee-run-backdrop.png` — Coffee Run intro frame, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:Coffee_Run_-_screenshot-intro.png. Attribution: Blender Foundation / Blender Animation Studio.
- `sprite-fright-poster.jpg` — Sprite Fright poster, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:Sprite_Fright-movie_poster.jpg. Attribution: Blender Foundation / Blender Studio.
- `glass-half-backdrop.png` — Glass Half intro frame, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:Glass_Half_-_screenshot-intro_scene.png. Attribution: Blender Foundation.
- `daily-dweebs-poster.png` — The Daily Dweebs poster, CC BY 4.0. Source: https://commons.wikimedia.org/wiki/File:The_Daily_Dweebs-movie_poster.png. Attribution: Blender Foundation.
- `media/big-buck-bunny-clip.mp4` — H.264 transcode of the 7.3-second CC BY 3.0 Big Buck Bunny bird clip at https://commons.wikimedia.org/wiki/File:Big_Buck_Bunny_8_seconds_bird_clip.ogv. Attribution: (c) copyright Blender Foundation | www.bigbuckbunny.org. The transcode removes audio and changes container/codec only.

`build_fixture_artwork.py` creates smaller crops and variants from those sources for library tiles, the fixture profile, series/season cards, episode cards and landscape backdrops. Those derivatives retain the source license and attribution above. The additional `Open Movie Classics`, `Open Worlds`, `Blender Shorts`, and `Modern Open Movies` series are mock Jellyfin anthology groupings used only by the screenshot fixture; their episode titles, descriptions and artwork are based on the openly licensed films above. No fixture media is downloaded during CI.
