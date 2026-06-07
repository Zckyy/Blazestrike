#pragma once
#include <d3d11.h>
#include <wincodec.h>
#include <vector>
#include <imgui.h>
#include <memory>
#include <string>
#include <algorithm>

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

        // Get overall dimensions from the first frame
        {
            IWICBitmapFrameDecode* first_frame = nullptr;
            if (SUCCEEDED(decoder->GetFrame(0, &first_frame)) && first_frame) {
                first_frame->GetSize(&width, &height);
                first_frame->Release();
            }
        }

        if (width == 0 || height == 0) {
            decoder->Release();
            wic_factory->Release();
            return false;
        }

        aspect_ratio = (float)width / (float)height;
        UINT stride = width * 4;
        std::vector<uint8_t> canvas_pixels(width * height * 4, 0);

        for (UINT i = 0; i < frame_count; ++i) {
            IWICBitmapFrameDecode* frame = nullptr;
            hr = decoder->GetFrame(i, &frame);
            if (FAILED(hr) || !frame) continue;

            UINT frame_w = 0, frame_h = 0;
            frame->GetSize(&frame_w, &frame_h);

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

            // Get frame offsets (for optimized sub-frame GIFs)
            UINT left = 0;
            UINT top = 0;
            IWICMetadataQueryReader* metadata_reader = nullptr;
            if (SUCCEEDED(frame->GetMetadataQueryReader(&metadata_reader))) {
                PROPVARIANT prop_value;
                PropVariantInit(&prop_value);
                if (SUCCEEDED(metadata_reader->GetMetadataByName(L"/imgdesc/Left", &prop_value))) {
                    if (prop_value.vt == VT_UI2) left = prop_value.uiVal;
                    PropVariantClear(&prop_value);
                }
                PropVariantInit(&prop_value);
                if (SUCCEEDED(metadata_reader->GetMetadataByName(L"/imgdesc/Top", &prop_value))) {
                    if (prop_value.vt == VT_UI2) top = prop_value.uiVal;
                    PropVariantClear(&prop_value);
                }
                metadata_reader->Release();
            }

            // Copy frame pixels
            UINT frame_stride = frame_w * 4;
            UINT frame_buf_size = frame_stride * frame_h;
            auto frame_pixels = std::make_unique<uint8_t[]>(frame_buf_size);
            hr = converter->CopyPixels(nullptr, frame_stride, frame_buf_size, frame_pixels.get());

            if (SUCCEEDED(hr)) {
                // Get disposal method to see if we should clear the background
                UINT disposal = 0;
                if (SUCCEEDED(frame->GetMetadataQueryReader(&metadata_reader))) {
                    PROPVARIANT prop_value;
                    PropVariantInit(&prop_value);
                    if (SUCCEEDED(metadata_reader->GetMetadataByName(L"/grctlext/Disposal", &prop_value))) {
                        if (prop_value.vt == VT_UI1) disposal = prop_value.bVal;
                        PropVariantClear(&prop_value);
                    }
                    metadata_reader->Release();
                }

                if (disposal == 2) {
                    std::fill(canvas_pixels.begin(), canvas_pixels.end(), 0);
                }

                // Compose current frame onto full canvas with offsets and transparency check
                for (UINT y = 0; y < frame_h; ++y) {
                    UINT dest_y = top + y;
                    if (dest_y >= height) break;
                    for (UINT x = 0; x < frame_w; ++x) {
                        UINT dest_x = left + x;
                        if (dest_x >= width) break;

                        UINT src_idx = (y * frame_w + x) * 4;
                        UINT dest_idx = (dest_y * width + dest_x) * 4;

                        uint8_t alpha = frame_pixels[src_idx + 3];
                        if (alpha > 0) {
                            canvas_pixels[dest_idx + 0] = frame_pixels[src_idx + 0];
                            canvas_pixels[dest_idx + 1] = frame_pixels[src_idx + 1];
                            canvas_pixels[dest_idx + 2] = frame_pixels[src_idx + 2];
                            canvas_pixels[dest_idx + 3] = alpha;
                        }
                    }
                }

                // Create Direct3D texture from composed full canvas
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = width;
                desc.Height = height;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                D3D11_SUBRESOURCE_DATA init_data = {};
                init_data.pSysMem = canvas_pixels.data();
                init_data.SysMemPitch = stride;

                ID3D11Texture2D* tex = nullptr;
                hr = device->CreateTexture2D(&desc, &init_data, &tex);
                if (SUCCEEDED(hr) && tex) {
                    ID3D11ShaderResourceView* srv = nullptr;
                    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                    srv_desc.Format = desc.Format;
                    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                    srv_desc.Texture2D.MipLevels = 1;

                    hr = device->CreateShaderResourceView(tex, &srv_desc, &srv);
                    tex->Release();

                    if (SUCCEEDED(hr) && srv) {
                        float delay = 0.1f;
                        if (SUCCEEDED(frame->GetMetadataQueryReader(&metadata_reader))) {
                            PROPVARIANT prop_value;
                            PropVariantInit(&prop_value);
                            if (SUCCEEDED(metadata_reader->GetMetadataByName(L"/grctlext/Delay", &prop_value))) {
                                if (prop_value.vt == VT_UI2 && prop_value.uiVal > 0) {
                                    delay = prop_value.uiVal * 0.01f;
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
                }
            }

            converter->Release();
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
