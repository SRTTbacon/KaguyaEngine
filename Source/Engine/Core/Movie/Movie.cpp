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
#include "..\\DescriptorHeap\\DescriptorHeap.h"

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
        , m_bMustDecode(true)
        , m_pVideoAcceration(nullptr)
    {
        m_movieIndex = MovieMeshCount++;

        for (size_t i = 0; i < arraysize(m_dpbLayouts); i++) {
            m_dpbLayouts[i] = D3D12_RESOURCE_STATE_COMMON;
        }
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
        double fps = static_cast<double>(m_video.frame_infos.size()) / 1.0f;

        // インデックス計算
        int nextFrameIndex = static_cast<int>(m_time * fps);

        // 範囲制御
        if (nextFrameIndex < 0)
            nextFrameIndex = 0;
        if (nextFrameIndex >= m_video.frame_infos.size())
            nextFrameIndex = static_cast<uint32>(m_video.frame_infos.size() - 1);

        // デコード
        DecodeFrame();
        if (m_beforeFrameIndex != nextFrameIndex)
        {
            //m_video.frameIndex = nextFrameIndex;
            m_beforeFrameIndex = nextFrameIndex;

            const Video::FrameInfo& frame_info = m_video.frame_infos[m_video.frameIndex];
            //printf("CurrentSlot - %d\n", m_video.current_slot);
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
        if (m_video.frameIndex == 0)
        {
            // At video beginning, reset the state of picture display reordering: 
            for (DecodeResultReordered& used : m_reorderedResultsWorking)
            {
                used.display_order = -1;
                m_reorderedResultsFree.push_back(used);
            }
            m_reorderedResultsWorking.clear();
            m_video.target_display_order = 0;
        }

        m_bMustDecode = true;
        for (auto& x : m_reorderedResultsWorking)
        {
            if (x.display_order == m_video.target_display_order)
                m_bMustDecode = false;
        }

        if (m_bMustDecode)
        {
            const Video::FrameInfo& frame_info = m_video.frame_infos[m_video.frameIndex];
            const h264::SliceHeader& slice_header = m_video.slice_headers[m_video.frameIndex];
            const h264::PPS& pps = m_video.pps_array[slice_header.pic_parameter_set_id];
            const h264::SPS& sps = m_video.sps_array[pps.seq_parameter_set_id];

            m_video.BeginDecodePicture(m_video.frameIndex);

            ID3D12VideoDecodeCommandList* pVideoCmd = m_pVideoAcceration->GetVideoCommand();

            if (m_dpbLayouts[m_video.current_slot] != D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE)
            {
                // if current DPB slot is not in DPB layout, transition it now:
                D3D12_RESOURCE_BARRIER barriers[2] = {};
                for (auto& barrier : barriers)
                {
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = m_dpbTexture.Get();
                    barrier.Transition.StateBefore = m_dpbLayouts[m_video.current_slot];
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
                }
                barriers[0].Transition.Subresource = m_video.current_slot; // luma plane
                barriers[1].Transition.Subresource = m_video.num_dpb_slots + m_video.current_slot; // chroma plane
                pVideoCmd->ResourceBarrier(arraysize(barriers), barriers);
                m_dpbLayouts[m_video.current_slot] = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
            }
            if (m_bReferenceOnlyAllocation)
            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = m_dpbOutputTexture.Get();
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                pVideoCmd->ResourceBarrier(1, &barrier);
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

            ID3D12Resource* reference_frames[17] = {};
            UINT reference_subresources[17] = {};
            assert(m_video.num_dpb_slots <= arraysize(reference_frames));
            for (size_t i = 0; i < m_video.num_dpb_slots; ++i)
            {
                reference_frames[i] = m_dpbTexture.Get();
                reference_subresources[i] = (UINT)i;
            }
            input.ReferenceFrames.NumTexture2Ds = m_video.num_dpb_slots;
            input.ReferenceFrames.ppTexture2Ds = reference_frames;
            input.ReferenceFrames.pSubresources = reference_subresources;

            input.CompressedBitstream.pBuffer = m_bitStreamBuffer.Get();
            input.CompressedBitstream.Offset = frame_info.offset;
            input.CompressedBitstream.Size = frame_info.size;
            input.pHeap = m_pDecoderHeap.Get();

            DXVA_PicParams_H264 pic_params_h264 = {};
            DXVA_Qmatrix_H264 qmatrix_h264 = {};
            DXVA_Slice_H264_Short sliceinfo_h264 = {};

            // DirectX Video Acceleration for H.264/MPEG-4 AVC Decoding, Microsoft, Updated 2010, Page 21
            //	https://www.microsoft.com/en-us/download/details.aspx?id=11323
            //	Also: https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp
            //	Also: https://github.com/mofo7777/H264Dxva2Decoder
            pic_params_h264.wFrameWidthInMbsMinus1 = sps.pic_width_in_mbs_minus1;
            pic_params_h264.wFrameHeightInMbsMinus1 = sps.pic_height_in_map_units_minus1;
            pic_params_h264.IntraPicFlag = frame_info.is_intra ? 1 : 0;
            pic_params_h264.MbaffFrameFlag = 0 /*sps->mb_adaptive_frame_field_flag && !slice_header->field_pic_flag*/;
            pic_params_h264.field_pic_flag = 0 /*slice_header->field_pic_flag*/; // 0 = full frame (top and bottom field)
            //pic_params_h264.bottom_field_flag = 0; // missing??
            pic_params_h264.chroma_format_idc = 1; // sps->chroma_format_idc; // only 1 is supported (YUV420)
            pic_params_h264.bit_depth_chroma_minus8 = sps.bit_depth_chroma_minus8;
            assert(pic_params_h264.bit_depth_chroma_minus8 == 0);   // Only support for NV12 now
            pic_params_h264.bit_depth_luma_minus8 = sps.bit_depth_luma_minus8;
            assert(pic_params_h264.bit_depth_luma_minus8 == 0);   // Only support for NV12 now
            pic_params_h264.residual_colour_transform_flag = sps.separate_colour_plane_flag; // https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp#L328
            if (pic_params_h264.field_pic_flag)
            {
                pic_params_h264.CurrPic.AssociatedFlag = slice_header.bottom_field_flag ? 1 : 0; // if pic_params_h264.field_pic_flag == 1, then this is 0 = top field, 1 = bottom_field
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
            for (size_t i = 0; i < m_video.reference_usage.size() && i < 16; ++i)
            {
                uint32_t ref_slot = m_video.reference_usage[i];
                assert(ref_slot != m_video.current_slot);
                pic_params_h264.RefFrameList[i].AssociatedFlag = 0; // 0 = short term, 1 = long term reference
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
            pic_params_h264.MbsConsecutiveFlag = 1; // The value shall be 1 unless the restricted-mode profile in use explicitly supports the value 0.
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
            assert(pic_params_h264.num_slice_groups_minus1 == 0);   // FMO Not supported by VA
            pic_params_h264.slice_group_map_type = pps.slice_group_map_type;
            pic_params_h264.deblocking_filter_control_present_flag = pps.deblocking_filter_control_present_flag;
            pic_params_h264.redundant_pic_cnt_present_flag = pps.redundant_pic_cnt_present_flag;
            pic_params_h264.slice_group_change_rate_minus1 = pps.slice_group_change_rate_minus1;
            pic_params_h264.Reserved16Bits = 3; // DXVA spec
            pic_params_h264.StatusReportFeedbackNumber = (UINT)m_video.frameIndex + 1; // shall not be 0
            assert(pic_params_h264.StatusReportFeedbackNumber > 0);
            pic_params_h264.ContinuationFlag = 1;
            int ref_idx_l0 = 0;
            int ref_idx_l1 = 0;
            Video::RefIdxActive(slice_header, pps, ref_idx_l0, ref_idx_l1);
            pic_params_h264.num_ref_idx_l0_active_minus1 = ref_idx_l0;
            pic_params_h264.num_ref_idx_l1_active_minus1 = ref_idx_l1;
            input.FrameArguments[input.NumFrameArguments].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_PICTURE_PARAMETERS;
            input.FrameArguments[input.NumFrameArguments].Size = sizeof(pic_params_h264);
            input.FrameArguments[input.NumFrameArguments].pData = &pic_params_h264;
            input.NumFrameArguments++;

            // DirectX Video Acceleration for H.264/MPEG-4 AVC Decoding, Microsoft, Updated 2010, Page 29
            //	Also: https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/drivers/d3d12/d3d12_video_dec_h264.cpp#L548
            if (sps.seq_scaling_matrix_present_flag)
            {
                static constexpr int vl_zscan_normal_16[] =
                {
                    /* Zig-Zag scan pattern */
                    0, 1, 4, 8, 5, 2, 3, 6,
                    9, 12, 13, 10, 7, 11, 14, 15
                };
                static constexpr int vl_zscan_normal[] =
                {
                    /* Zig-Zag scan pattern */
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
                // I don't know why it needs to be filled with 16, but otherwise it gets corrupted output
                //	Source: https://github.com/mofo7777/H264Dxva2Decoder
                std::memset(&qmatrix_h264, 16, sizeof(DXVA_Qmatrix_H264));
            }
            input.FrameArguments[input.NumFrameArguments].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_INVERSE_QUANTIZATION_MATRIX;
            input.FrameArguments[input.NumFrameArguments].Size = sizeof(qmatrix_h264);
            input.FrameArguments[input.NumFrameArguments].pData = &qmatrix_h264;
            input.NumFrameArguments++;

            // DirectX Video Acceleration for H.264/MPEG-4 AVC Decoding, Microsoft, Updated 2010, Page 31
            sliceinfo_h264.BSNALunitDataLocation = 0;
            sliceinfo_h264.SliceBytesInBuffer = (UINT)frame_info.size;
            sliceinfo_h264.wBadSliceChopping = 0; // whole slice is in the buffer
            input.FrameArguments[input.NumFrameArguments].Type = D3D12_VIDEO_DECODE_ARGUMENT_TYPE_SLICE_CONTROL;
            input.FrameArguments[input.NumFrameArguments].Size = sizeof(sliceinfo_h264);
            input.FrameArguments[input.NumFrameArguments].pData = &sliceinfo_h264;
            input.NumFrameArguments++;

            pVideoCmd->DecodeFrame(m_pDecoder.Get(), &output, &input);

            if (m_bReferenceOnlyAllocation)
            {
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = m_dpbOutputTexture.Get();
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                pVideoCmd->ResourceBarrier(1, &barrier);
            }
            else
            {
                D3D12_RESOURCE_BARRIER barriers[2] = {};
                for (auto& barrier : barriers)
                {
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = m_dpbTexture.Get();
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
                }
                barriers[0].Transition.Subresource = m_video.current_slot; // luma plane
                barriers[1].Transition.Subresource = m_video.num_dpb_slots + m_video.current_slot; // chroma plane
                pVideoCmd->ResourceBarrier(arraysize(barriers), barriers);
                m_dpbLayouts[m_video.current_slot] = D3D12_RESOURCE_STATE_COMMON;
            }

            m_video.EndDecodePicture(m_video.frameIndex);
        }
    }

    void Movie::Release()
    {
        m_bLoaded = false;
    }

    void Movie::BeforeDrawUpdate(uint32 srvIndex)
    {
        if (m_bMustDecode)
        {
            if (m_reorderedResultsFree.empty())
            {
                // Request new image, because there is no more free ones that we can use:
                m_reorderedResultsFree.emplace_back();
                m_reorderedResultsFree.back().create(m_pDevice, m_video.padded_width, m_video.padded_height);
            }

            // Copy will be done from decoder output to the newly allocated reordered picture:
            DecodeResultReordered reordered_current = m_reorderedResultsFree.back();
            m_reorderedResultsFree.pop_back();
            reordered_current.display_order = m_video.frame_infos[m_video.frameIndex].display_order;
            reordered_current.frame_index = m_video.frameIndex;

            if (m_bReferenceOnlyAllocation)
            {
                D3D12_TEXTURE_COPY_LOCATION cpy_src = {};
                cpy_src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                cpy_src.pResource = m_dpbOutputTexture.Get();
                D3D12_TEXTURE_COPY_LOCATION cpy_dst = {};
                cpy_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                cpy_dst.pResource = reordered_current.texture.Get();

                cpy_src.SubresourceIndex = 0; // luma plane
                cpy_dst.SubresourceIndex = 0; // luma plane
                GetCommandList()->CopyTextureRegion(&cpy_dst, 0, 0, 0, &cpy_src, nullptr);

                cpy_src.SubresourceIndex = 1; // chroma plane
                cpy_dst.SubresourceIndex = 1; // chroma plane
                GetCommandList()->CopyTextureRegion(&cpy_dst, 0, 0, 0, &cpy_src, nullptr);
            }
            else
            {
                D3D12_TEXTURE_COPY_LOCATION cpy_src = {};
                cpy_src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                cpy_src.pResource = m_dpbTexture.Get();
                D3D12_TEXTURE_COPY_LOCATION cpy_dst = {};
                cpy_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                cpy_dst.pResource = reordered_current.texture.Get();

                cpy_src.SubresourceIndex = m_video.current_slot; // luma plane
                cpy_dst.SubresourceIndex = 0; // luma plane
                GetCommandList()->CopyTextureRegion(&cpy_dst, 0, 0, 0, &cpy_src, nullptr);

                cpy_src.SubresourceIndex = m_video.num_dpb_slots + m_video.current_slot; // chroma plane
                cpy_dst.SubresourceIndex = 1; // chroma plane
                GetCommandList()->CopyTextureRegion(&cpy_dst, 0, 0, 0, &cpy_src, nullptr);
            }

            // Current latest reordered is pushed to the reorder working queue:
            m_reorderedResultsWorking.push_back(reordered_current);

            m_video.frameIndex = (static_cast<uint64>(m_video.frameIndex) + 1) % m_video.frame_infos.size();
        }

        m_video.time_until_next_frame -= m_video.timer.elapsed_seconds();
        m_video.timer.record();

        if (m_video.time_until_next_frame <= 0)
        {
            // Search for the next displayable with lowest display order:
            int display_order_changed = -1;
            for (int i = 0; i < (int)m_reorderedResultsWorking.size(); ++i)
            {
                if (m_reorderedResultsWorking[i].display_order == m_video.target_display_order)
                {
                    display_order_changed = i;
                }
            }
            if (display_order_changed >= 0)
            {
                // Free current output texture:
                if (m_displayedImage.texture != nullptr)
                {
                    m_reorderedResultsFree.push_back(m_displayedImage);
                }
                // Take this used texture as current output:
                m_displayedImage = std::move(m_reorderedResultsWorking[display_order_changed]);
                // Remove this used texture:
                std::swap(m_reorderedResultsWorking[display_order_changed], m_reorderedResultsWorking.back());
                m_reorderedResultsWorking.pop_back();

                m_video.time_until_next_frame = m_video.frame_infos[m_displayedImage.frame_index].duration_seconds;

                assert(m_displayedImage.texture != nullptr);
                m_video.target_display_order++;
            }
        }

        // the displayed image's two planes are put into shader resource state, the swapchain_uav is put into unordered access state:
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = m_displayedImage.texture.Get();
        barriers[0].Transition.Subresource = 0; // luma plane
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = m_displayedImage.texture.Get();
        barriers[1].Transition.Subresource = 1; // chroma plane
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        GetCommandList()->ResourceBarrier(arraysize(barriers), barriers);

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Format = DXGI_FORMAT_R8_UNORM;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.PlaneSlice = 0; // luma plane
        m_pDevice->CreateShaderResourceView(m_displayedImage.texture.Get(), &srv_desc, m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(srvIndex));	// Texture2D<float> input_luminance : register(t0);
        srv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
        srv_desc.Texture2D.PlaneSlice = 1; // chroma plane
        m_pDevice->CreateShaderResourceView(m_displayedImage.texture.Get(), &srv_desc, m_pDescriptor->pMainHeap->GetCpuDescriptorHandle(srvIndex + 1)); // Texture2D<float2> input_chrominance : register(t1);

        m_bMustDecode = false;
    }

    void Movie::AfterDrawUpdate()
    {
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = m_displayedImage.texture.Get();
        barriers[0].Transition.Subresource = 0; // luma plane
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = m_displayedImage.texture.Get();
        barriers[1].Transition.Subresource = 1; // chroma plane
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        GetCommandList()->ResourceBarrier(arraysize(barriers), barriers);
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
            hr = m_pVideoAcceration->GetDeviceD3D12()->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dpbOutputTexture));
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
