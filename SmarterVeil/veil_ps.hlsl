cbuffer constantBuffer : register(b0)
{
    float threshold;
    float brightness;
};

Texture2D<float4> new_desktop : register(t0);
Texture2D<float4> previous_veil : register(t1);

//TODO(fran): OPTIMIZE: LinearTosRGB and undo_sRGB
//  https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
//  http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
float3 LinearTosRGB(float3 lin) {
    //Reference: https://entropymine.com/imageworsener/srgbformula/
    float3 res;//sRGB
    res.r = (lin.r > 0.0031308) ? 1.055f * pow(lin.r, 1.f / 2.4f) - 0.055f : lin.r * 12.92f;
    res.g = (lin.g > 0.0031308) ? 1.055f * pow(lin.g, 1.f / 2.4f) - 0.055f : lin.g * 12.92f;
    res.b = (lin.b > 0.0031308) ? 1.055f * pow(lin.b, 1.f / 2.4f) - 0.055f : lin.b * 12.92f;
    return res;
}

float3 undo_sRGB(float3 srgb) {//sRGBtoLinear
    float3 res;//linear
    res.r = (srgb.r > 0.04045f) ? pow((srgb.r + 0.055f) / 1.055f, 2.4f) : srgb.r / 12.92f;
    res.g = (srgb.g > 0.04045f) ? pow((srgb.g + 0.055f) / 1.055f, 2.4f) : srgb.g / 12.92f;
    res.b = (srgb.b > 0.04045f) ? pow((srgb.b + 0.055f) / 1.055f, 2.4f) : srgb.b / 12.92f;
    return res;
}

float4 main(float4 screen_pos:SV_POSITION) : SV_TARGET
{
    uint2 pixel_pos = screen_pos.xy;
    float4 new_desktop_texel = new_desktop[pixel_pos];
    float4 old_veil_texel = previous_veil[pixel_pos];

    //NOTE(fran): since windows doesnt wait for us to see the frame before we can change color it means that the new frame has our old veil on top, for that reason we have to try to restore the real desktop image without the old veil
    //Premultiplied alpha format:
    //result.RGB = source.RGB + dest.RGB * (1 - source.A)
    //new_desktop_texel.rgb = old_veil_texel.rgb + x.rgb * (1- old_veil_texel.a)
    //new_desktop_texel.rgb - old_veil_texel.rgb = x.rgb * (1- old_veil_texel.a)
    //x.rgb = (new_desktop_texel.rgb - old_veil_texel.rgb) / (1- old_veil_texel.a)
    //old_veil_texel.rgb is always 0: x.rgb = (new_desktop_texel.rgb - 0) / (1- old_veil_texel.a)
    //NOTE(fran): there's a divide by zero case here but it will never happen since that'd mean we obscured a pixel completely, the UI wont allow that since it could cause the entire screen to get locked in black
    //NOTE(fran): also we need to previously move the desktop pixel from sRGB to linear, undo the premultiplication step there, and then convert back to sRGB
    new_desktop_texel.rgb = undo_sRGB(new_desktop_texel.rgb);
    float3 unaltered_new_desktop_texel = new_desktop_texel.rgb / (1 - old_veil_texel.a);
    unaltered_new_desktop_texel = LinearTosRGB(unaltered_new_desktop_texel);

    //TODO(fran): TEST: test more whether the sRGBtoLinear and back to sRGB steps are really helping

    float4 output_color=0;

    float pixel_weight = (unaltered_new_desktop_texel.r + unaltered_new_desktop_texel.g + unaltered_new_desktop_texel.b) / 3.0;
    if (pixel_weight > threshold) {
        output_color.a = brightness - threshold * brightness / pixel_weight;
    }
    //output_color.a *= 1.5f;//TEST
    //TODO(fran): we can hide extra information in the lowest possible floating point values for r,g and b to help us know next time through what we did last time and restore the current desktop image more accurately (we could also simply have a second texture that we write to that stores exactly what happened to each pixel)
    return output_color;
}