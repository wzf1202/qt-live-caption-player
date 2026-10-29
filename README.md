# VideoPlayer

Qt 视频播放器，支持 qmediaplayer / ffmpeg 后端。

## 实时字幕（sherpa-onnx Online）

仅在 `PLAYER_BACKEND=qmediaplayer` 且 `ENABLE_SHERPA_SUBTITLE=ON`（默认）时启用。

### 准备

1. 克隆 sherpa-onnx（若尚未存在，CMake 也可 FetchContent 拉取）：

```text
cd videoplayer/thirdparty
git clone --branch v1.13.7 --depth 1 https://github.com/k2-fsa/sherpa-onnx.git
```

2. 下载流式 Zipformer 中英双语模型并解压到：

```text
videoplayer/thirdparty/models/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/
```

目录内需包含：

- `encoder-epoch-99-avg-1.int8.onnx`
- `decoder-epoch-99-avg-1.onnx`
- `joiner-epoch-99-avg-1.int8.onnx`
- `tokens.txt`

发布包示例：

```text
https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20.tar.bz2
```

构建后会拷贝到运行目录：

```text
<exe目录>/models/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20/
```

也可设置环境变量指向模型目录：

```text
VIDEOPLAYER_SHERPA_MODEL_DIR=D:\path\to\sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20
```

### 说明

- 启动时会设置 `QT_MEDIA_BACKEND=ffmpeg`（`QAudioBufferOutput` 需要）。
- 播放时从 `QAudioBufferOutput` 取 16 kHz PCM，约每 80 ms 喂入 sherpa OnlineRecognizer。
- 识别中间结果（partial）即时叠在画面底部；端点检测触发后发出定稿（final）并覆盖显示。
- 未找到模型时仍可正常播放，仅无字幕识别。
- 默认使用 CPU provider（`SHERPA_ONNX_ENABLE_GPU=OFF`）。Windows 下与 Qt 对齐使用动态 CRT（`/MD`）。

### 构建示例（Ninja）

```text
call "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd videoplayer
cmake -S . -B build-sherpa -G Ninja -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_PREFIX_PATH=D:/Qt/6.11.1/msvc2022_64 ^
  -DENABLE_SHERPA_SUBTITLE=ON
cmake --build build-sherpa
```
