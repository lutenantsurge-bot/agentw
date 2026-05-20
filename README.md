llama-box + agent.cpp Combined Project
======================================

This project combines llama-box (multimodal inference server) with agent.cpp (C++ agent framework) into a single unified binary.

Architecture
------------
```
GGUF model (+ mmproj)
        |
        v
  llama-box server (port 8080)
  - Inference backend
  - Tool call parsing  
  - Multimodal support (vision)
  - OpenAI-compatible /v1 API
        |
        v
  agent.cpp client library
  - Agent loop
  - Tool execution
  - Memory management
        |
        v
  main.cpp (your application)
  - Tool implementations
  - Business logic
```

Components
----------
1. **llama-box**: Multimodal inference server from gpustack
   - Supports GGUF models
   - Tool calling with native function parsing
   - Vision support via mmproj files
   - REST API at port 8080

2. **agent.cpp**: C++ agent framework from Mozilla AI
   - HTTP client to llama-box
   - Tool call dispatcher
   - Cross-session memory
   - Agent state management

3. **Stable Diffusion**: Image generation via stable-diffusion.cpp
   - Latent diffusion models
   - Text-to-image generation

Directory Structure
-------------------
```
llama-box-agent/
├── CMakeLists.txt          # Unified build configuration
├── main.cpp                # Application entry point
├── src/
│   ├── agent.cpp           # Agent loop implementation
│   ├── agent.h             # Agent interface
│   ├── model.cpp           # Model interface
│   ├── model.h             # Model interface
│   └── callback.h          # Callback definitions
├── llama-box/              # llama-box server (subdir)
│   └── llama-box/          # Server source
├── llama.cpp/              # llama.cpp (subdir)
└── stable-diffusion.cpp/   # SD (subdir)
```

Building
--------
```bash
cd ~/Desktop/llama-box-agent
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j $(nproc)
```

The binary will be at: build/llama-box-agent

Running
-------
```bash
# Terminal 1: Start llama-box server
./build/llama-box/llama-box \
  --model ~/models/model.gguf \
  --mmproj ~/models/mmproj.gguf \
  --host 127.0.0.1 \
  --port 8080 \
  -c 32768 \
  -np 4 \
  --n-gpu-layers 99

# Terminal 2: Run agent
./build/llama-box-agent
```

Tool Calling
------------
The project defines three built-in tools:

1. **file_search(query, path)**: Search files by name in directory tree
2. **read_file(filepath)**: Read full file contents
3. **write_file(filepath, content)**: Write content to file

These are defined in main.cpp with JSON Schema and dispatched by name.

Configuration
-------------
- Context window: 32768 tokens (configurable)
- Temperature: 0.6 (recommended for tool calling)
- Parallel slots: 4 (configurable)
- GPU layers: 99 (offload all layers)

KV Cache Quant Trick (saves ~30% VRAM)
---------------------------------------
Use cache quantization for large contexts:
```bash
llama-box --cache-type-k q8_0 --cache-type-v q8_0
```

Troubleshooting
---------------
**Empty tool responses**: Check model supports function calling (Qwen2.5, Hermes3, Llama3.1)

**OOM errors**: Add --cache-type-k q8_0 --cache-type-v q8_0 or reduce model size

**Multimodal issues**: Verify mmproj matches model family exactly

**Agent can't find server**: Confirm http://127.0.0.1:8080/v1/models returns JSON

Built: May 2026
