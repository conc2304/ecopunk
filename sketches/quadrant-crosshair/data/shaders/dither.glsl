#version 120
uniform sampler2D tex;
uniform vec2      resolution;
uniform float     alpha;           // fixed arc position for this run (0=clean, 0.5=peak dither)
uniform float     opacity;         // layer fade 0→1→0 driven by slot lifecycle
uniform float     maxPixelation;   // fixed pixel block size for this run
varying vec2 vTexCoord;

// Bayer 8x8 ordered dithering — GPU equivalent of Floyd-Steinberg
float bayer8(vec2 p) {
    int x = int(mod(p.x, 8.0));
    int y = int(mod(p.y, 8.0));
    float m[64];
    m[ 0]= 0.0/63.0; m[ 1]=32.0/63.0; m[ 2]= 8.0/63.0; m[ 3]=40.0/63.0;
    m[ 4]= 2.0/63.0; m[ 5]=34.0/63.0; m[ 6]=10.0/63.0; m[ 7]=42.0/63.0;
    m[ 8]=48.0/63.0; m[ 9]=16.0/63.0; m[10]=56.0/63.0; m[11]=24.0/63.0;
    m[12]=50.0/63.0; m[13]=18.0/63.0; m[14]=58.0/63.0; m[15]=26.0/63.0;
    m[16]=12.0/63.0; m[17]=44.0/63.0; m[18]= 4.0/63.0; m[19]=36.0/63.0;
    m[20]=14.0/63.0; m[21]=46.0/63.0; m[22]= 6.0/63.0; m[23]=38.0/63.0;
    m[24]=60.0/63.0; m[25]=28.0/63.0; m[26]=52.0/63.0; m[27]=20.0/63.0;
    m[28]=62.0/63.0; m[29]=30.0/63.0; m[30]=54.0/63.0; m[31]=22.0/63.0;
    m[32]= 3.0/63.0; m[33]=35.0/63.0; m[34]=11.0/63.0; m[35]=43.0/63.0;
    m[36]= 1.0/63.0; m[37]=33.0/63.0; m[38]= 9.0/63.0; m[39]=41.0/63.0;
    m[40]=51.0/63.0; m[41]=19.0/63.0; m[42]=59.0/63.0; m[43]=27.0/63.0;
    m[44]=49.0/63.0; m[45]=17.0/63.0; m[46]=57.0/63.0; m[47]=25.0/63.0;
    m[48]=15.0/63.0; m[49]=47.0/63.0; m[50]= 7.0/63.0; m[51]=39.0/63.0;
    m[52]=13.0/63.0; m[53]=45.0/63.0; m[54]= 5.0/63.0; m[55]=37.0/63.0;
    m[56]=63.0/63.0; m[57]=31.0/63.0; m[58]=55.0/63.0; m[59]=23.0/63.0;
    m[60]=61.0/63.0; m[61]=29.0/63.0; m[62]=53.0/63.0; m[63]=21.0/63.0;
    return m[y * 8 + x];
}

// Sample the texture at the center of the pixelation block containing vTexCoord
vec3 samplePixelated(float pixSize) {
    if (pixSize <= 1.0) return texture2D(tex, vTexCoord).rgb;
    vec2 blockUV = (floor(vTexCoord * resolution / pixSize) + 0.5) * pixSize / resolution;
    return texture2D(tex, blockUV).rgb;
}

// Convert rgb to binary grayscale via Bayer dither.
// pixSize > 1: all pixels in the same block share one threshold so each block
// is uniformly black or white — matching the TypeScript pixelate-then-dither order.
vec3 applyDither(vec3 rgb, float pixSize) {
    float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
    luma = min(luma, 0.82);  // cap so bright regions always retain some dark cells
    vec2 pos = (pixSize <= 1.0)
        ? vTexCoord * resolution
        : (floor(vTexCoord * resolution / pixSize) + 0.5) * pixSize;
    float d = step(bayer8(pos), luma);
    return vec3(d * 0.88);   // white cells top out at 0.88 rather than 1.0
}

// Multi-phase transition matching the TypeScript DitherTransitionPlaylist:
//
//  alpha  0.00–0.20  clean image
//  alpha  0.20–0.35  cross-fade: clean → dithered
//  alpha  0.35–0.45  cross-fade: dithered → pixelated-dithered (pixelation grows)
//  alpha  0.45–0.55  maximum pixelation + dithering (transition midpoint)
//  alpha  0.55–0.65  cross-fade: pixelated-dithered → dithered (pixelation shrinks)
//  alpha  0.65–0.80  cross-fade: dithered → clean
//  alpha  0.80–1.00  clean image
//
// At alpha = 1 (ACTIVE dwell) the image is clean — the effect plays out over the
// fade-in and fade-out windows and peaks at the centre of the alpha arc.
void main() {
    float p = alpha;

    vec3 cleanColor   = texture2D(tex, vTexCoord).rgb;
    vec3 ditheredColor = applyDither(cleanColor, 1.0);

    vec3 finalColor;

    if (p < 0.2) {
        finalColor = cleanColor;
    } else if (p < 0.35) {
        float t = (p - 0.2) / 0.15;
        finalColor = mix(cleanColor, ditheredColor, t);
    } else if (p < 0.45) {
        float t = (p - 0.35) / 0.1;
        float pixSize = 1.0 + t * (maxPixelation - 1.0);
        vec3 pixDithered = applyDither(samplePixelated(pixSize), pixSize);
        finalColor = mix(ditheredColor, pixDithered, t);
    } else if (p < 0.55) {
        finalColor = applyDither(samplePixelated(maxPixelation), maxPixelation);
    } else if (p < 0.65) {
        float t = (p - 0.55) / 0.1;
        float pixSize = maxPixelation - t * (maxPixelation - 1.0);
        vec3 pixDithered = applyDither(samplePixelated(pixSize), pixSize);
        finalColor = mix(pixDithered, ditheredColor, t);
    } else if (p < 0.8) {
        float t = (p - 0.65) / 0.15;
        finalColor = mix(ditheredColor, cleanColor, t);
    } else {
        finalColor = cleanColor;
    }

    gl_FragColor = vec4(finalColor, opacity);
}
