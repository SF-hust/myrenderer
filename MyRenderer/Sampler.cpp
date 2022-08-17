#include "Sampler.h"

template <class T>
T Sampler2D<T>::sample(const Texture2D<T> &tex, Vec2f uv, Vec2f ddxUV, Vec2f ddyUV, const PipelineState &pipelineState) const
{
    T result = {};
    switch (mipmapMode)
    {
    case MIPMAP_MODE_NO_MIPMAP:
        result = sampleFromUVLevel(tex, uv, 0, 0);
        break;
    case MIPMAP_MODE_NEAREST:
        float scale = (ddxUV.x * (float)tex.width + ddyUV.y * (float)tex.height) / 2.0f;
        int mip = fmodf(scale, 1.0f) > 0.5f ? (int)scale : (int)scale - 1;
        mip = std::min(std::max(mip, 0), tex.mipmapLevel);
        result = sampleFromUVLevel(tex, uv, mip, mip);
        break;
    case MIPMAP_MODE_LINEAR:
    {
        float scale = (ddxUV.x * (float)tex.width + ddyUV.y * (float)tex.height) / 2.0f;
        float factor = fmodf(scale, 1.0f);
        int mip1 = std::min(std::max((int)scale - 1, 0), tex.mipmapLevel);
        int mip2 = std::min(std::max((int)scale, 0), tex.mipmapLevel);
        result += (1.0f - factor) * sampleFromUVLevel(tex, uv, mip1, mip1);
        result += factor * sampleFromUVLevel(tex, uv, mip2, mip2);
    }
    break;
    }
    return result;
}
template <class T>
T Sampler2D<T>::sampleFromUVLevel(const Texture2D<T>& tex, Vec2f rawUV, int ul, int vl) const
{
    Vec2f uv;
    T result;
    switch (addressMode)
    {
    case ADDRESS_MODE_REPEAT:
        uv.u = rawUV.u >= 0.0f ? fmod(rawUV.u, 1.0f) : fmod(rawUV.u, 1.0f) + 1.0f;
        uv.v = rawUV.v >= 0.0f ? fmod(rawUV.v, 1.0f) : fmod(rawUV.v, 1.0f) + 1.0f;
        break;
    case ADDRESS_MODE_MIRRORED_REPEAT:
        uv.u = 1.0f - std::abs(fmod(std::abs(rawUV.u), 2.0f) - 1.0f);
        uv.u = 1.0f - std::abs(fmod(std::abs(rawUV.v), 2.0f) - 1.0f);
        break;
    case ADDRESS_MODE_CLAMP_TO_EDGE:
        uv.u = clamp(rawUV.u, 0.0f, 1.0f);
        uv.v = clamp(rawUV.v, 0.0f, 1.0f);
        break;
    case ADDRESS_MODE_CLAMP_TO_BORDER:
        if (rawUV.u > 1.0f || rawUV.u < 0.0f || rawUV.v > 1.0f || rawUV.v < 0.0f)
        {
            return borderColor;
        }
        uv = rawUV;
        break;
    }

    int stx = 0, edx = tex.width, sty = 0, edy = tex.height;
    if (tex.mipmapLevel != 0)
    {
        stx = (int)tex.rawWidth - (1 << ((int)tex.mipmapLevel - ul + 1));
        edx = (int)tex.rawWidth - (1 << ((int)tex.mipmapLevel - ul));
        sty = (int)tex.rawHeight - (1 << ((int)tex.mipmapLevel - vl + 1));
        edy = (int)tex.rawHeight - (1 << ((int)tex.mipmapLevel - vl));
    }

    Vec2f uvInTex = {float(stx) + uv.u * float(edx - stx), float(sty) + uv.v * float(edy - sty)};
    switch (filterMode)
    {
    case FILTER_MODE_POINT:
        result = tex.at(int(uvInTex.x), int(uvInTex.y));
        break;
    case FILTER_MODE_LINEAR:
    {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float ku, kv;
        float xf = uvInTex.u * float(edx - stx);
        float yf = uvInTex.v * float(edy - sty);

        // calculate 4 pixel coords and 2 factors to blend
        if (uv.u == 1.0f)
        // if address mode is ADDRESS_MODE_REPEAT, uv.u can't be 1.0f
        {
            ku = 1.0f;
            x0 = x1 = edx - 1;
        }
        else if (fmodf(uvInTex.u, 1.0f) < 0.5f)
        // left half
        {
            ku = 0.5f - fmodf(uvInTex.u, 1.0f);
            x1 = int(uvInTex.u);
            if (addressMode == ADDRESS_MODE_REPEAT && x1 == stx)
            // when x1 is the first left and address mode is ADDRESS_MODE_REPEAT, the x0 should be the first right
            {
                x0 = edx - 1;
            }
            else
            {
                x0 = std::max(x1 - 1, stx);
            }
        }
        else
        // right half
        {
            ku = 1.5f - fmodf(uvInTex.u, 1.0f);
            x0 = int(uvInTex.u);
            if (addressMode == ADDRESS_MODE_REPEAT && x0 == edx - 1)
            // when x0 is the first right and address mode is ADDRESS_MODE_REPEAT, the x1 should be the first left
            {
                x1 = stx;
            }
            else
            {
                x1 = std::min(x0 + 1, int(edx - 1));
            }
        }

        if (uv.v == 1.0f)
        // if address mode is ADDRESS_MODE_REPEAT, uv.v can't be 1.0f
        {
            kv = 1.0f;
            y0 = y1 = edy - 1;
        }
        else if (fmodf(uvInTex.v, 1.0f) < 0.5f)
        // bottom half
        {
            kv = 0.5f - fmodf(uvInTex.v, 1.0f);
            y1 = int(uvInTex.v);
            if (addressMode == ADDRESS_MODE_REPEAT && y1 == sty)
            // when y1 is the first bottom and address mode is ADDRESS_MODE_REPEAT, the y0 should be the first top
            {
                y0 = edy - 1;
            }
            else
            {
                y0 = std::max(y1 - 1, sty);
            }
        }
        else
        // top half
        {
            kv = 1.5f - fmodf(uvInTex.v, 1.0f);
            y0 = int(uvInTex.v);
            if (addressMode == ADDRESS_MODE_REPEAT && y0 == edy - 1)
            // when y0 is the first top and address mode is ADDRESS_MODE_REPEAT, the y1 should be the first bottom
            {
                y1 = sty;
            }
            else
            {
                y1 = std::min(y0 + 1, int(edy - 1));
            }
        }
        // blend the colors
        T r = {};
        r += ku * kv * tex.at(x0, y0);
        r += (1 - ku) * kv * tex.at(x1, y0);
        r += ku * (1 - kv) * tex.at(x0, y1);
        r += (1 - ku) * (1 - kv) * tex.at(x1, y1);
        result = r;
    }
    break;
    case FILTER_MODE_ANISOTROPIC:
    {
        // TODO:
    }
    break;
    }
    return result;
}