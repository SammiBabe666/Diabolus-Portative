# Media tools

How `docs/diabolus.gif` and `docs/diabolus-witch-house.mp4` were made. Run from
this folder with the SDK beside the repository as `../open-xy`; needs g++,
Node with Playwright, and ffmpeg.

```sh
I="-I../../../open-xy/sdk/include -I../.."
# The song: the machine's engine.cpp renders every organ note offline.
g++ -std=c++14 -O2 $I song.cpp -o song && ./song          # song.f32, organ.f32, timeline.jsonl
# The screen: the machine's display.cpp, driven by the song's timeline.
g++ -std=c++11 -O1 $I screen.cpp -o screen && ./screen < timeline.jsonl > screen.jsonl
node video.js ../../panel.svg.fragment screen.jsonl frames 0 99999
ffmpeg -framerate 25 -i frames/v%05d.png -f f32le -ar 44100 -ac 2 -i song.f32 \
  -c:v libx264 -crf 18 -pix_fmt yuv420p -c:a aac -b:a 256k -shortest tour.mp4
# The GIF: a scripted 14 s of the screen.
g++ -std=c++11 -O1 $I frames.cpp -o frames && ./frames 14 > gif.jsonl
node render.js ../../panel.svg.fragment gif.jsonl gif 2
ffmpeg -framerate 25 -i gif/f%04d.png -vf "split[a][b];[a]palettegen=max_colors=8[p];[b][p]paletteuse=dither=none" diabolus.gif
```

The OP-XY's amp envelope release and stock tremolo are emulated in `song.cpp`;
the sub bass and drums are synthesized there for the song only.
