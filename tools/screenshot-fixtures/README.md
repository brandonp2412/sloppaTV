# Screenshot fixture media

These files exist only to make the automated Android TV screenshot suite exercise real artwork and video paths without depending on the network at test time.

The fixture uses Blender Foundation open-movie material:

- `big-buck-bunny-poster.jpg` — Big Buck Bunny poster, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Big_buck_bunny_poster_big.jpg. Attribution: (c) copyright Blender Foundation | www.bigbuckbunny.org.
- `big-buck-bunny-backdrop.png` — frame from Big Buck Bunny, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:BBB-Bunny.png. Attribution: (c) copyright Blender Foundation | www.bigbuckbunny.org.
- `sintel-poster.jpg` — Sintel poster, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Sintel_poster.jpg. Attribution: © copyright Blender Foundation – durian.blender.org.
- `sintel-backdrop.png` — frame from Sintel, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Sintel_Screenshot_1.png. Attribution: © copyright Blender Foundation www.sintel.org.
- `tears-of-steel-poster.png` — Tears of Steel poster. Source: https://commons.wikimedia.org/wiki/File:Tos-poster.png. Attribution: (CC) Blender Foundation | Project Mango.
- `caminandes-backdrop.png` — frame from Caminandes: Gran Dillama, CC BY 3.0. Source: https://commons.wikimedia.org/wiki/File:Caminandes_gran_dillama.png. Attribution: Francesco Siddi and Pablo Vazquez / Blender Foundation.
- `media/big-buck-bunny-clip.mp4` — H.264 transcode of the 7.3-second CC BY 3.0 Big Buck Bunny bird clip at https://commons.wikimedia.org/wiki/File:Big_Buck_Bunny_8_seconds_bird_clip.ogv. Attribution: (c) copyright Blender Foundation | www.bigbuckbunny.org. The transcode removes audio and changes container/codec only.

`build_fixture_artwork.py` creates smaller crops and variants from those sources for library tiles, the fixture profile, Caminandes season/episode cards, and a Tears of Steel backdrop. Those derivatives retain the source license and attribution above. No fixture media is downloaded during CI.
