whisper.cpp: https://github.com/ggml-org/whisper.cpp
Pinned source revision: d1be6fde11ac6e0407606b4e42fe72d34add8037 (v1.9.5).
CMake downloads and verifies the source archive. Only the CPU backend is used.

English speech model: OpenAI Whisper tiny.en, converted and Q5_1-quantized by
https://huggingface.co/ggerganov/whisper.cpp . The model downloads into private
app storage on first use and must match the size and SHA-256 in
subtitle_speech_model.hpp. Audio and recognized words stay on the device.
