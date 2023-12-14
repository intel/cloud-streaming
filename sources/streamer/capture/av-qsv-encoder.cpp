// Copyright (C) 2023 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "av-qsv-encoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_qsv.h"
}

#include "mfxdispatcher.h"

#include "dx-utils.h"

#if 1 // [fixme] bitstream dump
#include <fstream>
#endif

AVQSVEncoder::~AVQSVEncoder() {
    // [fixme] destructor
}

std::unique_ptr<AVQSVEncoder> AVQSVEncoder::create(const EncoderParams& enc_params) {
    // [fixme] move to separate func
    // validate enc params
    if (enc_params.codec == EncoderParams::Codec::unknown) {
        ga_logger(Severity::ERR, __FUNCTION__ ": codec is not set\n");
        return nullptr;
    }
    if (enc_params.target_bitrate == 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": target bitrate is not set\n");
        return nullptr;
    }
    if (enc_params.frame_rate == 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": frame rate is not set\n");
        return nullptr;
    }

    auto instance = std::unique_ptr<AVQSVEncoder>(new AVQSVEncoder);
    instance->m_desc = enc_params;

    // query target adapter
    CComPtr<IDXGIAdapter> adapter;
    HRESULT result = utils::enum_adapter_by_luid(&adapter, enc_params.adapter_luid);
    if (adapter == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::enum_adapter_by_luid failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_adapter = adapter;

    DXGI_ADAPTER_DESC adapter_desc = {};
    result = adapter->GetDesc(&adapter_desc);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": IDXGIAdapter->GetDesc failed, result = 0x%08x\n", result);
        return nullptr;
    }
    instance->m_adapter_desc = adapter_desc;

    // check vendor
    constexpr UINT vendor_intel = 0x8086;
    if (adapter_desc.VendorId != vendor_intel) {
        ga_logger(Severity::ERR, __FUNCTION__ ": unsupported adapter, this encoder supports Intel devices only\n");
        return nullptr;
    }

    // create d3d11 encoder device
    result = utils::create_d3d11_device(adapter, &instance->m_device, &instance->m_device_context, &instance->m_device_context_lock);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": utils::create_d3d11_device failed, result = 0x%08x\n", result);
        return nullptr;
    }

    result = instance->m_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, __uuidof(ID3D11Fence), reinterpret_cast<void**>(&instance->m_fence));
    if (FAILED(result) || instance->m_fence == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Device->CreateFence failed, result = 0x%08x\n", result);
        return nullptr;
    }

    result = instance->m_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &instance->m_fence_shared_handle);
    if (FAILED(result) || instance->m_fence_shared_handle == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11Fence->CreateSharedHandle failed, result = 0x%08x\n", result);
        return nullptr;
    }

    return instance;
}

bool AVQSVEncoder::is_format_supported(DXGI_FORMAT format) const {
    switch (format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return true;
    }
    return false;
}

HRESULT AVQSVEncoder::start() {
    // do nothing here
    // encode starts on first frame received
    return S_OK;
}

void AVQSVEncoder::stop() {
    // [fixme] drain encoder
}

static AVPixelFormat dxgi_format_to_av_pixel_format(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_NV12:
        return AV_PIX_FMT_NV12;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return AV_PIX_FMT_BGRA;
    }
    return AV_PIX_FMT_NONE;
}

static std::string get_codec_name(const EncoderParams::Codec& codec) {
    switch (codec) {
    case EncoderParams::Codec::avc:
        return "h264_qsv";
    case EncoderParams::Codec::hevc:
        return "hevc_qsv";
    case EncoderParams::Codec::av1:
        return "av1_qsv";
    }

    return std::string();
}

HRESULT AVQSVEncoder::init_av_context(uint32_t frame_width, uint32_t frame_height, DXGI_FORMAT frame_format) {
    // [fixme] cleanup prev context

    // fetch encoder config
    auto& enc_params = m_desc;

    // init av context
    int av_error = 0;

    // find codec
    auto codec_name = get_codec_name(enc_params.codec);
    if (codec_name.empty()) {
        ga_logger(Severity::ERR, __FUNCTION__ ": codec is not supported\n");
        return E_FAIL;
    }

    const AVCodec* av_codec = avcodec_find_encoder_by_name(codec_name.c_str());
    if (av_codec == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": avcodec_find_encoder_by_name failed\n");
        return E_FAIL;
    }

    // allocate codec context
    auto av_context = std::unique_ptr<AVCodecContext, utils::deleter::av_context>(avcodec_alloc_context3(av_codec));
    if (av_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": avcodec_alloc_context3 failed, context is nullptr\n");
        return E_FAIL;
    }

    // fill codec params
    // profile
    std::string opt_profile = "main";
    if (enc_params.profile != EncoderParams::Profile::unknown)
        opt_profile = to_string(enc_params.codec, enc_params.profile);
    av_opt_set(av_context->priv_data, "profile", opt_profile.c_str(), 0);
    // target bitrate
    av_context->bit_rate = enc_params.target_bitrate;
    // gop params
    av_context->gop_size = enc_params.key_frame_interval;
    av_context->max_b_frames = 0;
    // frame rate
    av_context->time_base = { 1, enc_params.frame_rate };
    av_context->framerate = { enc_params.frame_rate, 1 };
    // quality preset
    av_opt_set(av_context->priv_data, "preset", "medium", 0);
    // force IDR frame when pict_type is AV_PICTURE_TYPE_I
    av_opt_set(av_context->priv_data, "forced_idr", "1", 0);
    // P-ref strategy : 0 - default, 1 - simple, 2 - pyramid
    av_opt_set(av_context->priv_data, "p_strategy", "1", 0);
    // B-ref strategy : 0 - default, 1 - off, 2 - pyramid
    av_opt_set(av_context->priv_data, "b_strategy", "1", 0);
    // set qsv op queue depth to 1 for low-latency encode
    av_opt_set(av_context->priv_data, "async_depth", "1", 0);
    // resolution
    av_context->width = frame_width;
    av_context->height = frame_height;
    // pixel format
    av_context->pix_fmt = AV_PIX_FMT_QSV;
    av_context->sw_pix_fmt = dxgi_format_to_av_pixel_format(frame_format);

    // init hw device context
    HRESULT result = init_av_hw_device_context(&av_context->hw_device_ctx);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": init_av_hw_device_context failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // init hw frame context
    result = init_av_hw_frames_context(&av_context->hw_frames_ctx, av_context->hw_device_ctx,
        frame_width, frame_height, frame_format);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": init_av_hw_frames_context failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // init av context
    av_error = avcodec_open2(av_context.get(), av_codec, nullptr);
    if (av_error < 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": avcodec_open2 failed, result = %d, what = %s\n", 
            av_error, utils::av_error_to_string(av_error).c_str());
        return E_FAIL;
    }

    // set new av context
    m_av_context.reset();
    m_av_context = std::move(av_context);
    // set new frame format
    m_frame_width = frame_width;
    m_frame_height = frame_height;
    m_frame_format = frame_format;
    return S_OK;
}

HRESULT AVQSVEncoder::init_av_hw_device_context(AVBufferRef** hw_device_ctx) {
    if (hw_device_ctx == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (*hw_device_ctx != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    auto context = std::unique_ptr<AVBufferRef, utils::deleter::av_buffer_ref>(av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_QSV));
    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hwdevice_ctx_alloc failed\n");
        return E_FAIL;
    }

    AVHWDeviceContext* av_hw_context = reinterpret_cast<AVHWDeviceContext*>(context->data);
    if (av_hw_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hw_context is nullptr\n");
        return E_FAIL;
    }

    AVQSVDeviceContext* av_qsv_context = reinterpret_cast<AVQSVDeviceContext*>(av_hw_context->hwctx);
    if (av_qsv_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_qsv_context is nullptr\n");
        return E_FAIL;
    }

    // init qsv hw device context
    HRESULT result = init_av_qsv_device_context(av_qsv_context);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": init_av_qsv_device_context failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    int av_error = av_hwdevice_ctx_init(context.get());
    if (av_error < 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hwdevice_ctx_init failed, result = %d, what = %s\n",
            av_error, utils::av_error_to_string(av_error).c_str());
        return E_FAIL;
    }

    *hw_device_ctx = context.release();
    return S_OK;
}

HRESULT AVQSVEncoder::init_av_hw_frames_context(AVBufferRef** hw_frames_ctx, AVBufferRef* hw_device_ctx,
    uint32_t frame_width, uint32_t frame_height, DXGI_FORMAT frame_format) {
    // check input arguments
    if (hw_frames_ctx == nullptr || hw_device_ctx == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (*hw_frames_ctx != nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }
    if (frame_width == 0 || frame_height == 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    // init hw device context
    auto context = std::unique_ptr<AVBufferRef, utils::deleter::av_buffer_ref>(av_hwframe_ctx_alloc(hw_device_ctx));
    if (context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hwframe_ctx_alloc failed\n");
        return E_FAIL;
    }

    AVHWFramesContext* av_hw_context = reinterpret_cast<AVHWFramesContext*>(context->data);
    if (av_hw_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hw_context is nullptr\n");
        return E_FAIL;
    }

    av_hw_context->format = AV_PIX_FMT_QSV;
    av_hw_context->sw_format = dxgi_format_to_av_pixel_format(frame_format);
    av_hw_context->width = frame_width;
    av_hw_context->height = frame_height;
    // QSV encoder uses fixed pool size
    av_hw_context->initial_pool_size = m_init_pool_size;

    AVQSVFramesContext* av_qsv_context = reinterpret_cast<AVQSVFramesContext*>(av_hw_context->hwctx);
    if (av_qsv_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_qsv_context is nullptr\n");
        return E_FAIL;
    }

    // init qsv hw device context
    HRESULT result = init_av_qsv_frames_context(av_qsv_context);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": init_av_qsv_frames_context failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    int av_error = av_hwframe_ctx_init(context.get());
    if (av_error < 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hwframe_ctx_init failed, result = %d, what = %s\n",
            av_error, utils::av_error_to_string(av_error).c_str());
        return E_FAIL;
    }

    *hw_frames_ctx = context.release();
    return S_OK;
}

namespace utils {

    // return mfxStatus desc string
    static std::string mfx_status_to_string(mfxStatus mfx_status) {
        switch (mfx_status) {
            // success code
        case MFX_ERR_NONE: return "success";
            // error codes
        case MFX_ERR_UNKNOWN: return "unknown error";
        case MFX_ERR_NULL_PTR: return "null pointer";
        case MFX_ERR_UNSUPPORTED: return "unsupported feature";
        case MFX_ERR_MEMORY_ALLOC: return "failed to allocate memory";
        case MFX_ERR_NOT_ENOUGH_BUFFER: return "insufficient buffer at input/output";
        case MFX_ERR_INVALID_HANDLE: return "invalid handle";
        case MFX_ERR_LOCK_MEMORY: return "failed to lock the memory block";
        case MFX_ERR_NOT_INITIALIZED: return "member function called before initialization";
        case MFX_ERR_NOT_FOUND: return "object is not found";
        case MFX_ERR_MORE_DATA: return "expected more data at input";
        case MFX_ERR_MORE_SURFACE: return "expected more surfaces at output";
        case MFX_ERR_ABORTED: return "operation aborted";
        case MFX_ERR_DEVICE_LOST: return "hardware acceleration device lost";
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: return "incompatible video parameters";
        case MFX_ERR_INVALID_VIDEO_PARAM: return "invalid video parameters";
        case MFX_ERR_UNDEFINED_BEHAVIOR: return "undefined behavior";
        case MFX_ERR_DEVICE_FAILED: return "device operation failure";
        case MFX_ERR_MORE_BITSTREAM: return "expected more bitstream buffers at output";
        case MFX_ERR_GPU_HANG: return "device operation failure caused by GPU hang";
        case MFX_ERR_REALLOC_SURFACE: return "bigger output surface required";
        case MFX_ERR_RESOURCE_MAPPED: return "write access is already acquired and user requested another write access, or read access with MFX_MEMORY_NO_WAIT flag";
        case MFX_ERR_NOT_IMPLEMENTED: return "function not implemented";
            // warnings
        case MFX_WRN_IN_EXECUTION: return "previous async operation is still executing";
        case MFX_WRN_DEVICE_BUSY: return "hardware acceleration device is busy";
        case MFX_WRN_VIDEO_PARAM_CHANGED: return "video parameters changed";
        case MFX_WRN_PARTIAL_ACCELERATION: return "partial software acceleration is in use";
        case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM: return "incompatible video parameters";
        case MFX_WRN_VALUE_NOT_CHANGED: return "value is saturated based on its valid range";
        case MFX_WRN_OUT_OF_RANGE: return "value is out of range";
        case MFX_WRN_FILTER_SKIPPED: return "one of requested filters has been skipped";
        case MFX_ERR_NONE_PARTIAL_OUTPUT: return "frame is not ready, but bitstream contains partial output";
        case MFX_WRN_ALLOC_TIMEOUT_EXPIRED: return "timeout expired for internal frame allocation";
            // threading status
        case MFX_TASK_WORKING: return "task is still executing";
        case MFX_TASK_BUSY: return "task is waiting for resources";
            // plugin status
        case MFX_ERR_MORE_DATA_SUBMIT_TASK: return "return MFX_ERR_MORE_DATA but submit internal asynchronous task";
        default: break;
        };

        if (mfx_status > 0)
            return "unknown warning";

        return "unknown error";
    }

    namespace deleter {

        // mfxSession deleter for std::unique_ptr
        struct mfx_session {
            void operator()(_mfxSession* p) const {
                mfxStatus mfx_status = MFXClose(p);
                if (mfx_status != MFX_ERR_NONE)
                    ga_logger(Severity::ERR, __FUNCTION__ ": MFXClose failed, result = %d\n", mfx_status);
            }
        };

        // mfxLoader deleter for std::unique_ptr
        struct mfx_loader {
            void operator()(_mfxLoader* p) const {
                MFXUnload(p);
            }
        };

    } // namespace deleter

} // namespace utils

HRESULT AVQSVEncoder::init_av_qsv_device_context(AVQSVDeviceContext* qsv_context) {
    if (qsv_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    mfxStatus mfx_status = MFX_ERR_NONE;

    // create loader
    auto mfx_loader = std::unique_ptr<_mfxLoader, utils::deleter::mfx_loader>(MFXLoad());
    if (mfx_loader == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": MFXLoad failed\n");
        return E_FAIL;
    }

    // create mfx config
    auto mfx_config = MFXCreateConfig(mfx_loader.get());
    if (mfx_config == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": MFXCreateConfig failed\n");
        return E_FAIL;
    }

    // set implementation config
    // request hardware impl
    {
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_U32;
        mfx_variant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxImplDescription.Impl"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set impl type, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
            return E_FAIL;
        }
    }

    // request required api version
    {
        mfxVersion mfx_version = {};
        mfx_version.Major = 2;
        mfx_version.Minor = 0;
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_U32;
        mfx_variant.Data.U32 = mfx_version.Version;
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxImplDescription.ApiVersion.Version"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set api version, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
            return E_FAIL;
        }
    }

    // request vendor
    {
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_U16;
        mfx_variant.Data.U16 = 0x8086; // Intel device only
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxExtendedDeviceId.VendorID"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set vendor id, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        }
    }

    // request device id
    {
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_U16;
        mfx_variant.Data.U16 = m_adapter_desc.DeviceId;
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxExtendedDeviceId.DeviceID"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set device id, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        }
    }

    // request adapter luid
    {
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_PTR;
        mfx_variant.Data.Ptr = &m_adapter_desc.AdapterLuid;
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxExtendedDeviceId.DeviceLUID"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set device luid, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        }
    }

    // request node mask
    {
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_U32;
        mfx_variant.Data.U32 = 0x0001;
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxExtendedDeviceId.LUIDDeviceNodeMask"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set device node mask, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        }
    }

    // request d3d11 acceleration
    {
        mfxVariant mfx_variant = {};
        mfx_variant.Type = MFX_VARIANT_TYPE_U32;
        mfx_variant.Data.U32 = MFX_ACCEL_MODE_VIA_D3D11;
        mfx_status = MFXSetConfigFilterProperty(mfx_config, reinterpret_cast<const mfxU8*>("mfxImplDescription.AccelerationMode"), mfx_variant);
        if (mfx_status != MFX_ERR_NONE) {
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXSetConfigFilterProperty failed to set device node mask, mfx_status = %d, what = %s\n",
                mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        }
    }

    // create session from loader
    std::unique_ptr<_mfxSession, utils::deleter::mfx_session> mfx_session;
    for (mfxU32 impl_idx = 0; mfx_status != MFX_ERR_NOT_FOUND; ++impl_idx) {
        // enumerate all implementations
        mfxHDL mfx_impl_desc = nullptr;
        mfx_status = MFXEnumImplementations(mfx_loader.get(), impl_idx, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, &mfx_impl_desc);
        if (mfx_status != MFX_ERR_NONE) {
            if (mfx_status != MFX_ERR_NOT_FOUND)
                ga_logger(Severity::ERR, __FUNCTION__ ": MFXEnumImplementations failed, mfx_status = %d, what = %s\n",
                    mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
            continue;
        }

        // create session
        mfxSession session = nullptr;
        mfx_status = MFXCreateSession(mfx_loader.get(), impl_idx, &session);
        if (session != nullptr)
            mfx_session.reset(session);

        mfxStatus release_status = MFXDispReleaseImplDescription(mfx_loader.get(), mfx_impl_desc);
        if (release_status != MFX_ERR_NONE)
            ga_logger(Severity::ERR, __FUNCTION__ ": MFXEnumImplementations failed, mfx_status = %d, what = %s\n",
                release_status, utils::mfx_status_to_string(release_status).c_str());
        if (mfx_status == MFX_ERR_NONE)
            break; // session created
    }

    // check creation status
    if (mfx_status != MFX_ERR_NONE) {
        ga_logger(Severity::ERR, __FUNCTION__ ": MFXCreateSession failed, mfx_status = %d, what = %s\n",
            mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        return E_FAIL;
    }

    // query impl version
    mfxVersion mfx_impl_version = {};
    mfx_status = MFXQueryVersion(mfx_session.get(), &mfx_impl_version);
    if (mfx_status != MFX_ERR_NONE) {
        ga_logger(Severity::ERR, __FUNCTION__ ": MFXQueryVersion failed, mfx_status = %d, what = %s\n",
            mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        return E_FAIL;
    }

    // log session
    ga_logger(Severity::INFO, __FUNCTION__ ": initialized MFX session, api version %d.%d\n",
        mfx_impl_version.Major, mfx_impl_version.Minor);

    // set device handle
    mfx_status = MFXVideoCORE_SetHandle(mfx_session.get(), MFX_HANDLE_D3D11_DEVICE, m_device);
    if (mfx_status != MFX_ERR_NONE) {
        ga_logger(Severity::ERR, __FUNCTION__ ": MFXVideoCORE_SetHandle failed, mfx_status = %d, what = %s\n",
            mfx_status, utils::mfx_status_to_string(mfx_status).c_str());
        return E_FAIL;
    }

    qsv_context->loader = mfx_loader.release();
    qsv_context->session = mfx_session.release();
    return S_OK;
}

HRESULT AVQSVEncoder::init_av_qsv_frames_context(AVQSVFramesContext* qsv_context) {
    if (qsv_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": invalid argument\n");
        return E_INVALIDARG;
    }

    qsv_context->frame_type = MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    return S_OK;
}

HRESULT AVQSVEncoder::copy_src_surface_to_encode(ID3D11Texture2D* dst, ID3D11Texture2D* src) {
    if (src == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": src surface is nullptr\n");
        return E_INVALIDARG;
    }
    if (dst == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": dst surface is nullptr\n");
        return E_INVALIDARG;
    }

    if (m_device_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (m_device_context_lock == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context lock is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    D3D11_TEXTURE2D_DESC src_desc = {};
    src->GetDesc(&src_desc);

    D3D11_TEXTURE2D_DESC dst_desc = {};
    dst->GetDesc(&dst_desc);

    if (src_desc.Format != dst_desc.Format) {
        ga_logger(Severity::ERR, __FUNCTION__ ": texture format mismatch\n");
        return E_INVALIDARG;
    }

    m_device_context_lock->Enter();
    if (src_desc.Width == dst_desc.Width && src_desc.Height == dst_desc.Height) {
        // same size - copy whole resource
        m_device_context->CopyResource(dst, src);
    } else {
        // copy subresource regino
        m_device_context->CopySubresourceRegion(dst, 0, 0, 0, 0, src, 0, nullptr);
    }
    m_device_context_lock->Leave();

    return S_OK;
}

HRESULT AVQSVEncoder::encode_frame(Frame* frame) {
    // check input args
    if (frame == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": frame is nullptr\n");
        return E_INVALIDARG;
    }

    Surface* src_surface = frame->get_surface();
    if (src_surface == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": surface is nullptr\n");
        return E_INVALIDARG;
    }

    // check if encoder reset is needed
    bool av_context_initialized = (m_av_context != nullptr);
    auto src_surface_width = src_surface->get_width();
    auto src_surface_height = src_surface->get_height();
    auto src_surface_format = src_surface->get_format();
    bool frame_format_changed = src_surface_width != m_frame_width ||
        src_surface_height != m_frame_height ||
        src_surface_format != m_frame_format;
    bool reset_required = !av_context_initialized || frame_format_changed;
    if (reset_required) {
        HRESULT result = init_av_context(src_surface_width, src_surface_height, src_surface_format);
        if (FAILED(result)) {
            ga_logger(Severity::ERR, __FUNCTION__ ": init_av_context failed, result = 0x%08x\n", result);
            return E_FAIL;
        }
    }

    if (m_device_context == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }
    if (m_device_context_lock == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": device context lock is nullptr\n");
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    // wait source surface
    m_device_context_lock->Enter();
    HRESULT result = src_surface->wait_gpu_event_gpu(m_device_context);
    m_device_context_lock->Leave();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->wait_gpu_fence_cpu failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    CComPtr<ID3D11Texture2D> src_texture;
    result = src_surface->open_shared_texture(m_device, &src_texture);
    if (FAILED(result) || src_texture == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->open_shared_texture failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // alloc av frame
    auto av_frame = std::unique_ptr<AVFrame, utils::deleter::av_frame>(av_frame_alloc());
    if (av_frame == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_frame_alloc failed\n");
        return E_FAIL;
    }

    // fetch encoder surface
    int av_error = av_hwframe_get_buffer(m_av_context->hw_frames_ctx, av_frame.get(), 0);
    if (av_error < 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_hwframe_get_buffer failed, result = %d, what = %s\n",
            av_error, utils::av_error_to_string(av_error).c_str());
        return E_FAIL;
    }

    // fetch d3d11 texture from encoder surface
    mfxFrameSurface1* mfx_surface = reinterpret_cast<mfxFrameSurface1*>(av_frame->data[3]);
    if (mfx_surface == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": mfx_surface is nullptr\n");
        return E_FAIL;
    }

    mfxHDLPair* mfx_hdl_pair = reinterpret_cast<mfxHDLPair*>(mfx_surface->Data.MemId);
    if (mfx_hdl_pair == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": mfx_hdl_pair is nullptr\n");
        return E_FAIL;
    }

    ID3D11Texture2D* enc_texture = reinterpret_cast<ID3D11Texture2D*>(mfx_hdl_pair->first);
    if (enc_texture == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": enc_texture is nullptr\n");
        return E_FAIL;
    }

    // copy input surface to encode
    result = copy_src_surface_to_encode(enc_texture, src_texture);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": copy_src_surface_to_encode is nullptr\n");
        return E_FAIL;
    }

    // signal gpu fence
    m_device_context_lock->Enter();
    auto fence_value = InterlockedIncrement(&m_fence_value);
    result = m_device_context->Signal(m_fence, fence_value);
    m_device_context_lock->Leave();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->Signal failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    result = src_surface->signal_gpu_event(m_fence, m_fence_shared_handle, fence_value);
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": Surface->signal_gpu_event failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // signal context to wait on copy before submitting encode
    m_device_context_lock->Enter();
    result = m_device_context->Wait(m_fence, fence_value);
    m_device_context_lock->Leave();
    if (FAILED(result)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": ID3D11DeviceContext->Wait failed, result = 0x%08x\n", result);
        return E_FAIL;
    }

    // allocate new packet
    auto av_packet = std::unique_ptr<AVPacket, utils::deleter::av_packet>(av_packet_alloc());
    if (av_packet == nullptr) {
        ga_logger(Severity::ERR, __FUNCTION__ ": av_packet_alloc failed\n");
        return E_FAIL;
    }

    // insert keyframe if nessesary
    if (m_insert_key_frame) {
        av_frame->pict_type = AV_PICTURE_TYPE_I;
        m_insert_key_frame = 0;
    } else {
        av_frame->pict_type = AV_PICTURE_TYPE_P;
    }

    // encode frame
    av_error = avcodec_send_frame(m_av_context.get(), av_frame.get());
    if (av_error < 0) {
        ga_logger(Severity::ERR, __FUNCTION__ ": avcodec_send_frame failed, result = %d, what = %s\n",
            av_error, utils::av_error_to_string(av_error).c_str());
        return E_FAIL;
    }

    // receive packets if any, avcodec_receive_packet() will return EAGAIN if there are no outstanding packets
    std::unique_lock queue_lk(m_packet_queue_lock);
    for (av_error = 0; av_error >= 0; /* empty */) {
        av_error = avcodec_receive_packet(m_av_context.get(), av_packet.get());
        if (av_error >= 0) {
            // drop packets if queue is full
            if (m_packet_queue.size() > m_packet_queue_max_size) {
                ga_logger(Severity::WARNING, __FUNCTION__ ": encoded frame dropped, output queue is full\n");
                // insert keyframe to avoid missing refs
                m_insert_key_frame = 1;
                continue;
            }

            // push packet to queue
            Packet packet = {};
            packet.data.assign(av_packet->data, av_packet->data + av_packet->size);
            if (av_packet->flags & AV_PKT_FLAG_KEY)
                packet.flags = Packet::flag_keyframe;
            m_packet_queue.push_back(packet);
        }
    }
    queue_lk.unlock();
    m_packet_queue_cv.notify_one();

    // check avcodec_receive_packet for error
    if (av_error < 0 && av_error != AVERROR(EAGAIN)) {
        ga_logger(Severity::ERR, __FUNCTION__ ": avcodec_receive_packet failed, result = %d, what = %s\n",
            av_error, utils::av_error_to_string(av_error).c_str());
        return E_FAIL;
    }

    return S_OK;
}

HRESULT AVQSVEncoder::receive_packet(Packet& packet, UINT timeout_ms) {
    // wait for new packet in queue
    std::unique_lock queue_lk(m_packet_queue_lock);
    auto timeout = std::chrono::milliseconds(timeout_ms);
    auto signalled = m_packet_queue_cv.wait_for(queue_lk, timeout, [&]() -> bool { return !m_packet_queue.empty(); });
    if (!signalled)
        return DXGI_ERROR_WAIT_TIMEOUT;

    packet = std::move(m_packet_queue.front());
    m_packet_queue.pop_front();
    queue_lk.unlock();

    return S_OK;
}

void AVQSVEncoder::request_key_frame() {
    m_insert_key_frame.store(1);
}
