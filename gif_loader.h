#pragma once
#include <d3d11.h>
#include <wincodec.h>
#include <vector>
#include <imgui.h>
#include <memory>
#include <string>

struct GifFrame {
    ID3D11ShaderResourceView* srv = nullptr;
    float delay = 0.1f; // delay in seconds
};

class GifImage {
public:
    std::vector<GifFrame> frames;
    UINT width = 0;
    UINT height = 0;
    float aspect_ratio = 1.0f;
    float total_duration = 0.0f;

    void shutdown() {
        for (auto& frame : frames) {
            if (frame.srv) {
                frame.srv->Release();
                frame.srv = nullptr;
            }
        }
        frames.clear();
        width = 0;
        height = 0;
        aspect_ratio = 1.0f;
        total_duration = 0.0f;
    }

    bool load(ID3D11Device* device, const char* filepath) {
        shutdown();
        if (!device) return false;

        IWICImagingFactory* wic_factory = nullptr;
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWICImagingFactory, (void**)&wic_factory);
        if (FAILED(hr) || !wic_factory) return false;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, filepath, -1, nullptr, 0);
        auto wpath = std::make_unique<wchar_t[]>(wlen);
        MultiByteToWideChar(CP_UTF8, 0, filepath, -1, wpath.get(), wlen);

        IWICBitmapDecoder* decoder = nullptr;
        hr = wic_factory->CreateDecoderFromFilename(
            wpath.get(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

        if (FAILED(hr) || !decoder) {
            wic_factory->Release();
            return false;
        }

        UINT frame_count = 0;
        decoder->GetFrameCount(&frame_count);
        if (frame_count == 0) {
            decoder->Release();
            wic_factory->Release();
            return false;
        }

        for (UINT i = 0; i < frame_count; ++i) {
            IWICBitmapFrameDecode* frame = nullptr;
            hr = decoder->GetFrame(i, &frame);
            if (FAILED(hr) || !frame) continue;

            if (i == 0) {
                frame->GetSize(&width, &height);
                if (width == 0 || height == 0) {
                    frame->Release();
                    break;
                }
                aspect_ratio = (float)width / (float)height;
            }

            IWICFormatConverter* converter = nullptr;
            hr = wic_factory->CreateFormatConverter(&converter);
            if (FAILED(hr) || !converter) {
                frame->Release();
                continue;
            }

            hr = converter->Initialize(
                frame, GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0.0f,
                WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) {
                converter->Release();
                frame->Release();
                continue;
            }

            UINT w = 0, h = 0;
            converter->GetSize(&w, &h);
            UINT stride = w * 4;
            UINT buf_size = stride * h;
            auto pixels = std::make_unique<uint8_t[]>(buf_size);

            hr = converter->CopyPixels(nullptr, stride, buf_size, pixels.get());
            if (FAILED(hr)) {
                converter->Release();
                frame->Release();
                continue;
            }

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = w;
            desc.Height = h;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA init_data = {};
            init_data.pSysMem = pixels.get();
            init_data.SysMemPitch = stride;

            ID3D11Texture2D* tex = nullptr;
            hr = device->CreateTexture2D(&desc, &init_data, &tex);
            if (FAILED(hr) || !tex) {
                converter->Release();
                frame->Release();
                continue;
            }

            ID3D11ShaderResourceView* srv = nullptr;
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = desc.Format;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(tex, &srv_desc, &srv);
            tex->Release();
            converter->Release();

            if (SUCCEEDED(hr) && srv) {
                float delay = 0.1f; // fallback delay: 100ms
                IWICMetadataQueryReader* metadata_reader = nullptr;
                if (SUCCEEDED(frame->GetMetadataQueryReader(&metadata_reader))) {
                    PROPVARIANT prop_value;
                    PropVariantInit(&prop_value);
                    if (SUCCEEDED(metadata_reader->GetMetadataByName(L"/grctlext/Delay", &prop_value))) {
                        if (prop_value.vt == VT_UI2) {
                            if (prop_value.uiVal > 0) {
                                delay = prop_value.uiVal * 0.01f;
                            }
                        }
                        PropVariantClear(&prop_value);
                    }
                    metadata_reader->Release();
                }

                GifFrame gif_frame;
                gif_frame.srv = srv;
                gif_frame.delay = delay;
                frames.push_back(gif_frame);
                total_duration += delay;
            }

            frame->Release();
        }

        decoder->Release();
        wic_factory->Release();
        return !frames.empty();
    }

    ID3D11ShaderResourceView* get_current_frame(float time) {
        if (frames.empty()) return nullptr;
        if (total_duration <= 0.0f) return frames[0].srv;

        float t = fmodf(time, total_duration);
        float accum = 0.0f;
        for (const auto& frame : frames) {
            accum += frame.delay;
            if (t <= accum) {
                return frame.srv;
            }
        }
        return frames.back().srv;
    }
};

inline GifImage g_menu_cat_gif;
