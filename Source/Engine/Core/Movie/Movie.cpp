//*****************************************************************************
//
// 動画(mp4想定)ファイルをデコードし、指定したリソースに書き込むクラス
//
// Movie.cpp
//
// T_Takeuchi
//
// 2026/10/12
//
//*****************************************************************************

#include "Movie.h"
#include "VideoAcceration.h"

#include <filesystem>
#include <d3dx12.h>

namespace KaguyaEngine::Core
{
    uint32 Movie::MovieMeshCount = 0;

    Movie::Movie()
        : m_time(0.0f)
        , m_speed(1.0f)
        , m_width(0)
        , m_height(0)
        , m_beforeFrameIndex(0xffffffff)
        , m_bPaused(false)
        , m_bLoop(true)
        , m_bLoaded(false)
        , m_bReferenceOnlyAllocation(false)
        , m_pVideoAcceration(nullptr)
    {
        m_movieIndex = MovieMeshCount++;
    }

    Movie::~Movie()
    {
        Release();
    }

    void Movie::Initialize(VideoAcceration* pVideoAcceration)
    {
        m_pVideoAcceration = pVideoAcceration;
    }

    void Movie::LoadMovie(const std::string& movieFilePath)
    {
        if (!std::filesystem::exists(movieFilePath)) {
            return;
        }

        Release();

        if (!m_video.Load_mp4(movieFilePath.c_str()))
        {
            printf("動画を読み込めませんでした。\n");
            return;
        }

        if (m_video.frame_infos.empty())
        {
            printf("動画内にフレームが存在しません。\n");
            return;
        }

        CreateDecoder();
        CreateBitStream();
        CreateDPBTexture();
        CreateOutputTexture();

        m_bLoaded = true;
    }

    void Movie::UpdateVideo()
    {
        if (!m_bLoaded || m_bPaused || !m_pOutputTexture)
            return;

        // FPS計算
        double fps = static_cast<double>(m_video.frame_infos.size()) / m_video.timestamp_seconds;

        // インデックス計算
        int nextFrameIndex = static_cast<int>(m_time * fps);

        // 範囲制御
        if (nextFrameIndex < 0)
            nextFrameIndex = 0;
        if (nextFrameIndex >= m_video.frame_infos.size())
            nextFrameIndex = static_cast<uint32>(m_video.frame_infos.size() - 1);

        // デコード
        if (m_beforeFrameIndex != nextFrameIndex)
        {
            m_video.frameIndex = nextFrameIndex;
            DecodeFrame();
            m_beforeFrameIndex = nextFrameIndex;
        }

        // 時間を進める
        m_time += *m_pFrameTime * m_speed;

        if (m_beforeFrameIndex >= m_video.frame_infos.size() - 1)
        {
            m_time = 0.0f;
            if (!m_bLoop)
            {
                m_bPaused = true;
            }
        }
    }

    void Movie::DecodeFrame()
    {
        const Video::FrameInfo& frame_info = m_video.frame_infos[m_video.frameIndex];
        const h264::SliceHeader& slice_header = m_video.slice_headers[m_video.frameIndex];
        const h264::PPS& pps = m_video.pps_array[slice_header.pic_parameter_set_id];
        const h264::SPS& sps = m_video.sps_array[pps.seq_parameter_set_id];

        if (frame_info.is_intra)
        {
            m_video.reference_usage.clear();
            m_video.next_ref = 0;
            m_video.next_slot = 0;
        }
        m_video.current_slot = m_video.next_slot;
        m_video.poc_status[m_video.current_slot] = frame_info.poc;
        m_video.framenum_status[m_video.current_slot] = slice_header.frame_num;

        if (dpb_layouts[m_video.current_slot] == D3D12_RESOURCE_STATE_COMMON)
        {
            // バリアを遷移
            D3D12_RESOURCE_BARRIER barriers[2] = {};
            for (auto& barrier : barriers)
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = m_dpbTexture.Get();
                barrier.Transition.StateBefore = dpb_layouts[m_video.current_slot];
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
            }
            barriers[0].Transition.Subresource = m_video.current_slot; // luma plane
            barriers[1].Transition.Subresource = m_video.num_dpb_slots + m_video.current_slot; // chroma plane
            m_pVideoAcceration->GetVideoCommand()->ResourceBarrier(arraysize(barriers), barriers);

            if (m_bReferenceOnlyAllocation)
            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = m_dpbOutputTexture.Get();
                barrier.Transition.StateBefore = dpb_layouts[m_video.current_slot];
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                m_pVideoAcceration->GetVideoCommand()->ResourceBarrier(1, &barrier);
            }

            dpb_layouts[m_video.current_slot] = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
        }

        D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS input = {};
        D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS output = {};
        if (m_bReferenceOnlyAllocation)
        {
            output.pOutputTexture2D = m_dpbOutputTexture.Get();
            output.OutputSubresource = 0;
            output.ConversionArguments.Enable = TRUE;
            output.ConversionArguments.pReferenceTexture2D = m_dpbTexture.Get();
            output.ConversionArguments.ReferenceSubresource = m_video.current_slot;
        }
        else
        {
            output.pOutputTexture2D = m_dpbTexture.Get();
            output.OutputSubresource = m_video.current_slot;
        }

        ID3D12Resource* reference_frames[16] = {};
        UINT reference_subresources[16] = {};
        for (size_t i = 0; i < m_video.num_dpb_slots; ++i)
        {
            reference_frames[i] = m_dpbTexture.Get();
            reference_subresources[i] = (UINT)i;
        }
        input.ReferenceFrames.NumTexture2Ds = arraysize(reference_frames);
        input.ReferenceFrames.ppTexture2Ds = reference_frames;
        input.ReferenceFrames.pSubresources = reference_subresources;

        input.CompressedBitstream.pBuffer = m_bitStreamBuffer.Get();
        input.CompressedBitstream.Offset = frame_info.offset;
        input.CompressedBitstream.Size = frame_info.size;
        input.pHeap = m_pDecoderHeap.Get();

        DXVA_PicParams_H264 pic_params_h264 = {};
        DXVA_Qmatrix_H264 qmatrix_h264 = {};
        DXVA_Slice_H264_Short sliceinfo_h264 = {};

        // H.264/MPEG-4 でデコード
        // https://www.microsoft.com/en-us/download/details.aspx?id=11323
        // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp
        // https://github.com/mofo7777/H264Dxva2Decoder
        pic_params_h264.wFrameWidthInMbsMinus1 = sps.pic_width_in_mbs_minus1;
        pic_params_h264.wFrameHeightInMbsMinus1 = sps.pic_height_in_map_units_minus1;
        pic_params_h264.IntraPicFlag = frame_info.is_intra ? 1 : 0;
        pic_params_h264.MbaffFrameFlag = 0;
        pic_params_h264.field_pic_flag = 0;
        //pic_params_h264.bottom_field_flag = 0; // missing??
        pic_params_h264.chroma_format_idc = 1; // 1のみサポート (YUV420)
        pic_params_h264.bit_depth_chroma_minus8 = sps.bit_depth_chroma_minus8;
        assert(pic_params_h264.bit_depth_chroma_minus8 == 0);   // NV12のみサポート
        pic_params_h264.bit_depth_luma_minus8 = sps.bit_depth_luma_minus8;
        assert(pic_params_h264.bit_depth_luma_minus8 == 0);     // NV12のみサポート
        pic_params_h264.residual_colour_transform_flag = sps.separate_colour_plane_flag; // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp#L328
        if (pic_params_h264.field_pic_flag)
        {
            pic_params_h264.CurrPic.AssociatedFlag = slice_header.bottom_field_flag ? 1 : 0; // 0 = トップフィールド, 1 = ボトムフィールド
        }
        else
        {
            pic_params_h264.CurrPic.AssociatedFlag = 0;
        }
        pic_params_h264.CurrPic.Index7Bits = (UCHAR)m_video.current_slot;
        pic_params_h264.CurrFieldOrderCnt[0] = frame_info.poc;
        pic_params_h264.CurrFieldOrderCnt[1] = frame_info.poc;
        for (uint32_t i = 0; i < 16; ++i)
        {
            pic_params_h264.RefFrameList[i].bPicEntry = 0xFF;
            pic_params_h264.FieldOrderCntList[i][0] = 0;
            pic_params_h264.FieldOrderCntList[i][1] = 0;
            pic_params_h264.FrameNumList[i] = 0;
        }
        for (size_t i = 0; i < m_video.reference_usage.size(); ++i)
        {
            uint32_t ref_slot = m_video.reference_usage[i];
            assert(ref_slot != m_video.current_slot);
            pic_params_h264.RefFrameList[i].AssociatedFlag = 0; // 0 = 短期的な参照, 1 = 長期的な参照
            pic_params_h264.RefFrameList[i].Index7Bits = (UCHAR)ref_slot;
            pic_params_h264.FieldOrderCntList[i][0] = m_video.poc_status[ref_slot];
            pic_params_h264.FieldOrderCntList[i][1] = m_video.poc_status[ref_slot];
            pic_params_h264.UsedForReferenceFlags |= 1 << (i * 2 + 0);
            pic_params_h264.UsedForReferenceFlags |= 1 << (i * 2 + 1);
            pic_params_h264.FrameNumList[i] = m_video.framenum_status[ref_slot];
        }
        pic_params_h264.weighted_pred_flag = pps.weighted_pred_flag;
        pic_params_h264.weighted_bipred_idc = pps.weighted_bipred_idc;
        pic_params_h264.sp_for_switch_flag = 0;

        pic_params_h264.transform_8x8_mode_flag = pps.transform_8x8_mode_flag;
        pic_params_h264.constrained_intra_pred_flag = pps.constrained_intra_pred_flag;
        pic_params_h264.num_ref_frames = sps.num_ref_frames;
        pic_params_h264.MbsConsecutiveFlag = 1;
        pic_params_h264.frame_mbs_only_flag = sps.frame_mbs_only_flag;
        pic_params_h264.MinLumaBipredSize8x8Flag = sps.level_idc >= 31;
        pic_params_h264.RefPicFlag = frame_info.reference_priority > 0 ? 1 : 0;
        pic_params_h264.frame_num = slice_header.frame_num;
        pic_params_h264.pic_init_qp_minus26 = pps.pic_init_qp_minus26;
        pic_params_h264.pic_init_qs_minus26 = pps.pic_init_qs_minus26;
        pic_params_h264.chroma_qp_index_offset = pps.chroma_qp_index_offset;
        pic_params_h264.second_chroma_qp_index_offset = pps.second_chroma_qp_index_offset;
        pic_params_h264.log2_max_frame_num_minus4 = sps.log2_max_frame_num_minus4;
        pic_params_h264.pic_order_cnt_type = sps.pic_order_cnt_type;
        pic_params_h264.log2_max_pic_order_cnt_lsb_minus4 = sps.log2_max_pic_order_cnt_lsb_minus4;
        pic_params_h264.delta_pic_order_always_zero_flag = sps.delta_pic_order_always_zero_flag;
        pic_params_h264.direct_8x8_inference_flag = sps.direct_8x8_inference_flag;
        pic_params_h264.entropy_coding_mode_flag = pps.entropy_coding_mode_flag;
        pic_params_h264.pic_order_present_flag = pps.pic_order_present_flag;
        pic_params_h264.num_slice_groups_minus1 = pps.num_slice_groups_minus1;
        assert(pic_params_h264.num_slice_groups_minus1 == 0);   // FMO は VA でサポートされていない
        pic_params_h264.slice_group_map_type = pps.slice_group_map_type;
        pic_params_h264.deblocking_filter_control_present_flag = pps.deblocking_filter_control_present_flag;
        pic_params_h264.redundant_pic_cnt_present_flag = pps.redundant_pic_cnt_present_flag;
        pic_params_h264.slice_group_change_rate_minus1 = pps.slice_group_change_rate_minus1;
        pic_params_h264.Reserved16Bits = 3;
        pic_params_h264.StatusReportFeedbackNumber = (UINT)m_video.frameIndex + 1; // 0は無効な値なので、1から始める
        assert(pic_params_h264.StatusReportFeedbackNumber > 0);
        pic_params_h264.ContinuationFlag = 1;
        pic_params_h264.num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_active_minus1;
        pic_params_h264.num_ref_idx_l1_active_minus1 = pps.num_ref_idx_l1_active_minus1;
        input.FrameArguments[input.NumFrameArguments].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS;
        input.FrameArguments[input.NumFrameArguments].Size = sizeof(pic_params_h264);
        input.FrameArguments[input.NumFrameArguments].pData = &pic_params_h264;
        input.NumFrameArguments++;

        // H.264/MPEG-4 AVC デコード
        // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp#L548
        if (sps.seq_scaling_matrix_present_flag)
        {
            static constexpr int vl_zscan_normal_16[] =
            {
                0, 1, 4, 8, 5, 2, 3, 6,
                9, 12, 13, 10, 7, 11, 14, 15
            };
            static constexpr int vl_zscan_normal[] =
            {
                 0, 1, 8,16, 9, 2, 3,10,
                17,24,32,25,18,11, 4, 5,
                12,19,26,33,40,48,41,34,
                27,20,13, 6, 7,14,21,28,
                35,42,49,56,57,50,43,36,
                29,22,15,23,30,37,44,51,
                58,59,52,45,38,31,39,46,
                53,60,61,54,47,55,62,63
            };
            for (int i = 0; i < 6; ++i)
            {
                for (int j = 0; j < 16; ++j)
                {
                    qmatrix_h264.bScalingLists4x4[i][j] = pps.ScalingList4x4[i][vl_zscan_normal_16[j]];
                }
            }
            for (int i = 0; i < 64; ++i)
            {
                qmatrix_h264.bScalingLists8x8[0][i] = pps.ScalingList8x8[0][vl_zscan_normal[i]];
                qmatrix_h264.bScalingLists8x8[1][i] = pps.ScalingList8x8[1][vl_zscan_normal[i]];
            }
        }
        else
        {
            // https://github.com/mofo7777/H264Dxva2Decoder
            std::memset(&qmatrix_h264, 16, sizeof(DXVA_Qmatrix_H264));
        }

        input.FrameArguments[input.NumFrameArguments].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_INVERSE_QUANTIZATION_MATRIX;
        input.FrameArguments[input.NumFrameArguments].Size = sizeof(qmatrix_h264);
        input.FrameArguments[input.NumFrameArguments].pData = &qmatrix_h264;
        input.NumFrameArguments++;

        // H.264/MPEG-4 AVC デコード
        // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp#L548
        sliceinfo_h264.BSNALunitDataLocation = 0;
        sliceinfo_h264.SliceBytesInBuffer = (UINT)frame_info.size;
        sliceinfo_h264.wBadSliceChopping = 0;
        input.FrameArguments[input.NumFrameArguments].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL;
        input.FrameArguments[input.NumFrameArguments].Size = sizeof(sliceinfo_h264);
        input.FrameArguments[input.NumFrameArguments].pData = &sliceinfo_h264;
        input.NumFrameArguments++;

        ID3D12VideoDecodeCommandList* pCommandList = m_pVideoAcceration->GetVideoCommand();
        pCommandList->DecodeFrame(m_pDecoder.Get(), &output, &input);

        D3D12_RESOURCE_BARRIER barriers2[2] = {};
        for (auto& barrier : barriers2)
        {
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_dpbTexture.Get();
            barrier.Transition.StateBefore = dpb_layouts[m_video.current_slot];
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        }
        barriers2[0].Transition.Subresource = m_video.current_slot;
        barriers2[1].Transition.Subresource = m_video.num_dpb_slots + m_video.current_slot;
        pCommandList->ResourceBarrier(arraysize(barriers2), barriers2);
        if (m_bReferenceOnlyAllocation)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_dpbOutputTexture.Get();
            barrier.Transition.StateBefore = dpb_layouts[m_video.current_slot];
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            pCommandList->ResourceBarrier(1, &barrier);
        }
        dpb_layouts[m_video.current_slot] = D3D12_RESOURCE_STATE_COMMON;
    }

    void Movie::Release()
    {
        m_bLoaded = false;
    }

    void Movie::BeforeDrawUpdate()
    {
        if (dpb_layouts[m_video.current_slot] == D3D12_RESOURCE_STATE_COMMON)
        {
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            if (m_bReferenceOnlyAllocation)
            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = m_dpbOutputTexture.Get();
                barrier.Transition.StateBefore = dpb_layouts[m_video.current_slot];
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                GetCommandList()->ResourceBarrier(1, &barrier);
            }
            else
            {
                for (int i = 0; i < 2; i++) {
                    barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_dpbTexture.Get(), dpb_layouts[m_video.current_slot], D3D12_RESOURCE_STATE_COPY_SOURCE));
                }
                barriers[0].Transition.Subresource = m_video.current_slot;
                barriers[1].Transition.Subresource = m_video.num_dpb_slots + m_video.current_slot;
                barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST));
                GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
                barriers.clear();
            }
            dpb_layouts[m_video.current_slot] = D3D12_RESOURCE_STATE_COPY_SOURCE;

            D3D12_TEXTURE_COPY_LOCATION cpy_src = {};
            cpy_src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            cpy_src.pResource = m_dpbTexture.Get();
            D3D12_TEXTURE_COPY_LOCATION cpy_dst = {};
            cpy_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            cpy_dst.pResource = m_pOutputTexture.Get();

            cpy_src.SubresourceIndex = m_video.current_slot;
            cpy_dst.SubresourceIndex = 0;
            GetCommandList()->CopyTextureRegion(&cpy_dst, 0, 0, 0, &cpy_src, nullptr);

            cpy_src.SubresourceIndex = m_video.num_dpb_slots + m_video.current_slot;
            cpy_dst.SubresourceIndex = 1;
            GetCommandList()->CopyTextureRegion(&cpy_dst, 0, 0, 0, &cpy_src, nullptr);

            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(m_pOutputTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
            GetCommandList()->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }
    }

    void Movie::AfterDrawUpdate()
    {
        if (dpb_layouts[m_video.current_slot] == D3D12_RESOURCE_STATE_COPY_SOURCE) {
            D3D12_RESOURCE_BARRIER barriers[2] = {};
            for (auto& barrier : barriers)
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = m_dpbTexture.Get();
                barrier.Transition.StateBefore = dpb_layouts[m_video.current_slot];
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
            }
            barriers[0].Transition.Subresource = m_video.current_slot; // luma plane
            barriers[1].Transition.Subresource = m_video.num_dpb_slots + m_video.current_slot; // chroma plane
            GetCommandList()->ResourceBarrier(2, barriers);
            dpb_layouts[m_video.current_slot] = D3D12_RESOURCE_STATE_COMMON;
        }
    }

    void Movie::CreateDecoder()
    {
        HRESULT hr;

        // デコーダーの設定

        D3D12_VIDEO_DECODER_DESC decoder_desc = {};
        decoder_desc.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
        decoder_desc.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;

        D3D12_FEATURE_DATA_VIDEO_DECODE_FORMAT_COUNT video_decode_format_count = {};
        video_decode_format_count.Configuration = decoder_desc.Configuration;
        hr = m_pVideoAcceration->GetVideoDevice()->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_FORMAT_COUNT, &video_decode_format_count, sizeof(video_decode_format_count));
        assert(SUCCEEDED(hr));
        std::vector<DXGI_FORMAT> formats(video_decode_format_count.FormatCount);
        D3D12_FEATURE_DATA_VIDEO_DECODE_FORMATS video_decode_formats = {};
        video_decode_formats.Configuration = decoder_desc.Configuration;
        video_decode_formats.FormatCount = video_decode_format_count.FormatCount;
        video_decode_formats.pOutputFormats = formats.data();
        hr = m_pVideoAcceration->GetVideoDevice()->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_FORMATS, &video_decode_formats, sizeof(video_decode_formats));
        assert(SUCCEEDED(hr));
        D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT video_decode_support = {};
        video_decode_support.Configuration = decoder_desc.Configuration;
        video_decode_support.DecodeFormat = DECODE_FORMAT;
        bool format_valid = false;
        for (auto& x : formats)
        {
            if (x == video_decode_support.DecodeFormat)
            {
                format_valid = true;
                break;
            }
        }
        if (!format_valid)
        {
            printf("NV12に適切なデコーダー設定が見つかりませんでした。");
            return;
        }

        video_decode_support.Width = m_video.padded_width;
        video_decode_support.Height = m_video.padded_height;
        video_decode_support.BitRate = 0;
        video_decode_support.FrameRate = { 0, 1 };
        hr = m_pVideoAcceration->GetVideoDevice()->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &video_decode_support, sizeof(video_decode_support));
        assert(SUCCEEDED(hr));

        if (video_decode_support.DecodeTier < D3D12_VIDEO_DECODE_TIER_1)
        {
            printf("D3D12_VIDEO_DECODE_TIER_1がGPUでサポートされていません。");
            return;
        }

        m_bReferenceOnlyAllocation = video_decode_support.ConfigurationFlags & D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_REFERENCE_ONLY_ALLOCATIONS_REQUIRED;

        D3D12_VIDEO_DECODER_HEAP_DESC heap_desc = {};
        heap_desc.Configuration = decoder_desc.Configuration;
        heap_desc.DecodeWidth = video_decode_support.Width;
        heap_desc.DecodeHeight = video_decode_support.Height;
        heap_desc.Format = video_decode_support.DecodeFormat;
        heap_desc.FrameRate = { 0,1 };
        heap_desc.BitRate = 0;
        heap_desc.MaxDecodePictureBufferCount = m_video.num_dpb_slots;

        if (video_decode_support.ConfigurationFlags & D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_HEIGHT_ALIGNMENT_MULTIPLE_32_REQUIRED)
        {
            heap_desc.DecodeWidth = align(video_decode_support.Width, 32U);
            heap_desc.DecodeHeight = align(video_decode_support.Height, 32U);
        }

        hr = m_pVideoAcceration->GetVideoDevice()->CreateVideoDecoderHeap(&heap_desc, IID_PPV_ARGS(&m_pDecoderHeap));
        assert(SUCCEEDED(hr));
        hr = m_pVideoAcceration->GetVideoDevice()->CreateVideoDecoder(&decoder_desc, IID_PPV_ARGS(&m_pDecoder));
        assert(SUCCEEDED(hr));
    }

    void Movie::CreateBitStream()
    {
        HRESULT hr;

        uint64_t aligned_size = 0;
        for (Video::FrameInfo& frame_info : m_video.frame_infos)
        {
            aligned_size += align(frame_info.size, (uint64_t)D3D12_VIDEO_DECODE_MIN_BITSTREAM_OFFSET_ALIGNMENT);
        }

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Width = aligned_size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.MipLevels = 1;
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
        hr = m_pVideoAcceration->GetDeviceD3D12()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_bitStreamBuffer));
        assert(SUCCEEDED(hr));
        hr = m_bitStreamBuffer->SetName(L"bitstream_buffer");
        assert(SUCCEEDED(hr));

        void* mapped_data = nullptr;
        hr = m_bitStreamBuffer->Map(0, nullptr, &mapped_data);
        assert(SUCCEEDED(hr));

        uint64_t aligned_offset = 0;
        for (Video::FrameInfo& frame_info : m_video.frame_infos)
        {
            std::memcpy((uint8_t*)mapped_data + aligned_offset, m_video.h264_data.data() + frame_info.offset, frame_info.size); // copy into GPU buffer through mapped_data ptr
            frame_info.offset = aligned_offset;
            aligned_offset += align(frame_info.size, (uint64_t)D3D12_VIDEO_DECODE_MIN_BITSTREAM_OFFSET_ALIGNMENT);
        }

        m_video.h264_data.clear();

        m_bitStreamBuffer->Unmap(0, nullptr);
    }

    void Movie::CreateDPBTexture()
    {
        HRESULT hr;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Format = DECODE_FORMAT;
        desc.Width = m_video.padded_width;
        desc.Height = m_video.padded_height;
        desc.DepthOrArraySize = m_video.num_dpb_slots;
        desc.SampleDesc.Count = 1;
        desc.MipLevels = 1;
        if (m_bReferenceOnlyAllocation)
        {
            desc.Flags = D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        else
        {
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        }
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        hr = m_pVideoAcceration->GetDeviceD3D12()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dpbTexture));
        assert(SUCCEEDED(hr));
        hr = m_dpbTexture->SetName(L"dpb_texture");
        assert(SUCCEEDED(hr));

        for (size_t i = 0; i < arraysize(dpb_layouts); i++) {
            dpb_layouts[i] = D3D12_RESOURCE_STATE_COMMON;
        }

        if (m_bReferenceOnlyAllocation)
        {
            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Format = DECODE_FORMAT;
            desc.Width = m_video.padded_width;
            desc.Height = m_video.padded_height;
            desc.DepthOrArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.MipLevels = 1;
            D3D12_HEAP_PROPERTIES heap_properties = {};
            heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            hr = m_pVideoAcceration->GetDeviceD3D12()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&m_dpbOutputTexture));
            assert(SUCCEEDED(hr));
            hr = m_dpbOutputTexture->SetName(L"dpb_output_texture");
            assert(SUCCEEDED(hr));
        }
    }

    void Movie::CreateOutputTexture()
    {
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Format = DECODE_FORMAT;
        desc.Width = m_video.padded_width;
        desc.Height = m_video.padded_height;
        desc.DepthOrArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.MipLevels = 1;
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        HRESULT hr = m_pDevice->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(m_pOutputTexture.ReleaseAndGetAddressOf()));
        assert(SUCCEEDED(hr));
        hr = m_pOutputTexture->SetName(L"DecodeResultReordered::Texture");
        assert(SUCCEEDED(hr));
    }
}
