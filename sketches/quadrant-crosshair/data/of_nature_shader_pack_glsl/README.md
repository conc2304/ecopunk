# openFrameworks Nature Shader Pack

Raspberry Pi friendly GLSL 120 shaders using `sampler2DRect` for openFrameworks.

## Shared vertex shader
Use `passthrough.glsl` with each fragment shader.

## Required setup
Most shaders expect:

```cpp
shader.begin();
shader.setUniformTexture("tex0", source.getTexture(), 0);
shader.setUniform2f("resolution", source.getWidth(), source.getHeight());
shader.setUniform1f("time", ofGetElapsedTimef());
source.draw(0, 0);
shader.end();
```

For `temporal_trails.glsl`, ping-pong two FBOs and bind:

```cpp
shader.setUniformTexture("currentTex", current.getTexture(), 0);
shader.setUniformTexture("previousTex", previousFbo.getTexture(), 1);
```

Then draw into the next trail FBO and swap.

## Notes
- These use GLSL `#version 120` and `texture2DRect`, which is usually the least painful path for OF desktop/Raspberry Pi compatibility.
- Pixel sorting here is an approximation, not true row sorting. Real sorting is better done CPU-side or with multi-pass GPU buffers.
- Keep full-screen passes low. On Raspberry Pi, prefer 1-3 active passes, or render expensive passes at half resolution.


Note: All shader files are packaged with `.glsl` extensions as requested. Use `passthrough.glsl` as the shared vertex shader and the named effect `.glsl` files as fragment shaders.
