#define MINIMP4_IMPLEMENTATION
#include "common.h"

bool Video::Load_mp4(const char* filename)
{
	std::vector<uint8_t> filedata;
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (file.is_open())
	{
		size_t dataSize = (size_t)file.tellg();
		file.seekg((std::streampos)0);
		filedata.resize(dataSize);
		file.read((char*)filedata.data(), dataSize);
		file.close();
	}
	if (filedata.empty())
	{
		printf("File not found: %s\n", filename);
		return false;
	}

	bool success = false;
	const uint8_t* input_buf = filedata.data();
	struct INPUT_BUFFER
	{
		const uint8_t* buffer;
		size_t size;
	} input_buffer = { input_buf,filedata.size() };
	MP4D_demux_t mp4 = {};
	int token = 0;
	auto read_callback = [](int64_t offset, void* buffer, size_t size, void* token) -> int
		{
			INPUT_BUFFER* buf = (INPUT_BUFFER*)token;
			if (offset < 0 || (uint64_t)offset >= buf->size)
				return 1;
			size_t available = buf->size - (size_t)offset;
			size_t to_copy = MINIMP4_MIN(size, available);
			std::memcpy(buffer, buf->buffer + offset, to_copy);
			return to_copy != size;
		};
	int result = MP4D_open(&mp4, read_callback, &input_buffer, (int64_t)filedata.size());
	if (result != 1)
	{
		printf("MP4 parsing failure! MP4D_open result = %d", result);
		return false;
	}

	uint64_t slice_size = 0;

	for (uint32_t ntrack = 0; ntrack < mp4.track_count; ntrack++)
	{
		const MP4D_track_t& track = mp4.track[ntrack];

		if (track.handler_type != MP4D_HANDLER_TYPE_VIDE || track.object_type_indication != MP4_OBJECT_TYPE_AVC)
			continue; // Only AVC video (H264) is supported in this sample

		// SPS:
		{
			int size = 0;
			int index = 0;
			const void* data = nullptr;
			while (data = MP4D_read_sps(&mp4, ntrack, index, &size))
			{
				const uint8_t* sps_data = (const uint8_t*)data;

				h264::Bitstream bs = {};
				bs.init(sps_data, size);
				h264::NALHeader nal = {};
				h264::read_nal_header(&nal, &bs);
				assert(nal.type == h264::NAL_UNIT_TYPE_SPS);

				sps_array.emplace_back();
				h264::SPS& sps = sps_array.back();
				h264::read_sps(&sps, &bs);

				// Some validation checks that data parsing returned expected values:
				//	https://stackoverflow.com/questions/6394874/fetching-the-dimensions-of-a-h264video-stream
				width = ((sps.pic_width_in_mbs_minus1 + 1) * 16) - sps.frame_crop_left_offset * 2 - sps.frame_crop_right_offset * 2;
				height = ((2 - sps.frame_mbs_only_flag) * (sps.pic_height_in_map_units_minus1 + 1) * 16) - (sps.frame_crop_top_offset * 2) - (sps.frame_crop_bottom_offset * 2);
				assert(track.SampleDescription.video.width == width);
				assert(track.SampleDescription.video.height == height);
				padded_width = (sps.pic_width_in_mbs_minus1 + 1) * 16;
				padded_height = (sps.pic_height_in_map_units_minus1 + 1) * 16;
				num_dpb_slots = std::max(num_dpb_slots, uint32_t(sps.num_ref_frames + 1));

				index++;
			}
		}

		// PPS:
		{
			int size = 0;
			int index = 0;
			const void* data = nullptr;
			while (data = MP4D_read_pps(&mp4, ntrack, index, &size))
			{
				const uint8_t* pps_data = (const uint8_t*)data;

				h264::Bitstream bs = {};
				bs.init(pps_data, size);
				h264::NALHeader nal = {};
				h264::read_nal_header(&nal, &bs);
				assert(nal.type == h264::NAL_UNIT_TYPE_PPS);

				pps_array.emplace_back();
				h264::PPS& pps = pps_array.back();
				h264::read_pps(&pps, &bs);

				index++;
			}
		}

		double timescale_rcp = 1.0 / double(track.timescale);

		uint32_t track_duration = 0;
		for (uint32_t i = 0; i < track.sample_count; i++)
		{
			unsigned frame_bytes, timestamp, duration;
			MP4D_file_offset_t ofs = MP4D_frame_offset(&mp4, ntrack, i, &frame_bytes, &timestamp, &duration);
			track_duration += duration;

			frame_infos.emplace_back();
			Video::FrameInfo& frame_info = frame_infos.back();

			const uint8_t* src_buffer = input_buf + ofs;
			while (frame_bytes > 0)
			{
				uint32_t size = ((uint32_t)src_buffer[0] << 24) | ((uint32_t)src_buffer[1] << 16) | ((uint32_t)src_buffer[2] << 8) | src_buffer[3];
				size += 4;
				assert(frame_bytes >= size);

				h264::Bitstream bs = {};
				bs.init(&src_buffer[4], size - 4);
				h264::NALHeader nal = {};
				h264::read_nal_header(&nal, &bs);

				if (nal.type == h264::NAL_UNIT_TYPE_CODED_SLICE_IDR)
				{
					frame_info.is_intra = true;
				}
				else if (nal.type == h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR)
				{
					frame_info.is_intra = false;
				}
				else
				{
					// Continue search for frame beginning NAL unit:
					frame_bytes -= size;
					src_buffer += size;
					continue;
				}

				slice_headers.emplace_back();
				h264::SliceHeader& slice_header = slice_headers.back();
				h264::read_slice_header(&slice_header, &nal, pps_array.data(), sps_array.data(), &bs);

				// Accept frame beginning NAL unit:
				frame_info.reference_priority = nal.idc;

				frame_info.offset = h264_data.size();
				frame_info.size = sizeof(h264::nal_start_code) + size - 4;
				h264_data.resize(h264_data.size() + frame_info.size);
				std::memcpy(h264_data.data() + frame_info.offset, h264::nal_start_code, sizeof(h264::nal_start_code));
				std::memcpy(h264_data.data() + frame_info.offset + sizeof(h264::nal_start_code), src_buffer + 4, size - 4);

				break;
			}

			slice_size += frame_info.size;
			frame_info.timestamp_seconds = float(double(timestamp) * timescale_rcp);
			frame_info.duration_seconds = float(double(duration) * timescale_rcp);
		}
	}
	MP4D_close(&mp4);
	CalculateFrameDisplayOrder();
	return true;
}

bool Video::Load_h264_raw(const char* filename, float framerate)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (file.is_open())
	{
		size_t dataSize = (size_t)file.tellg();
		file.seekg((std::streampos)0);
		h264_data.resize(dataSize);
		file.read((char*)h264_data.data(), dataSize);
		file.close();
	}
	if (h264_data.empty())
	{
		printf("File not found: %s\n", filename);
		return false;
	}

	h264::Bitstream bs;
	bs.init(h264_data.data(), h264_data.size());
	while (h264::find_next_nal(&bs))
	{
		const uint64_t nal_offset = bs.byte_offset() - sizeof(h264::nal_start_code);
		if (!frame_infos.empty())
		{
			// patch size of previous frame:
			Video::FrameInfo& frame_info = frame_infos.back();
			if (frame_info.size == 0)
			{
				frame_info.size = nal_offset - frame_info.offset;
			}
		}

		h264::NALHeader nal;
		if (h264::read_nal_header(&nal, &bs))
		{
			switch (nal.type)
			{
			case h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR:
			{
				frame_infos.emplace_back();
				Video::FrameInfo& frame_info = frame_infos.back();
				frame_info.offset = nal_offset;
				frame_info.is_intra = false;
				frame_info.reference_priority = nal.idc;
				slice_headers.emplace_back();
				h264::SliceHeader& slice_header = slice_headers.back();
				h264::read_slice_header(&slice_header, &nal, pps_array.data(), sps_array.data(), &bs);
			}
			break;
			case h264::NAL_UNIT_TYPE_CODED_SLICE_IDR:
			{
				frame_infos.emplace_back();
				Video::FrameInfo& frame_info = frame_infos.back();
				frame_info.offset = nal_offset;
				frame_info.is_intra = true;
				frame_info.reference_priority = nal.idc;
				slice_headers.emplace_back();
				h264::SliceHeader& slice_header = slice_headers.back();
				h264::read_slice_header(&slice_header, &nal, pps_array.data(), sps_array.data(), &bs);
			}
			break;
			case h264::NAL_UNIT_TYPE_SPS:
			{
				if (sps_array.empty()) // TODO: multiple SPS fix
				{
					sps_array.emplace_back();
					h264::SPS& sps = sps_array.back();
					h264::read_sps(&sps, &bs);

					width = ((sps.pic_width_in_mbs_minus1 + 1) * 16) - sps.frame_crop_left_offset * 2 - sps.frame_crop_right_offset * 2;
					height = ((2 - sps.frame_mbs_only_flag) * (sps.pic_height_in_map_units_minus1 + 1) * 16) - (sps.frame_crop_top_offset * 2) - (sps.frame_crop_bottom_offset * 2);
					padded_width = (sps.pic_width_in_mbs_minus1 + 1) * 16;
					padded_height = (sps.pic_height_in_map_units_minus1 + 1) * 16;
					num_dpb_slots = std::max(num_dpb_slots, uint32_t(sps.num_ref_frames + 1));
					printf("Resolution = %d x %d (padded: %d x %d)\nDPB slots: %d\n", (int)width, (int)height, (int)padded_width, (int)padded_height, (int)num_dpb_slots);
				}
			}
			break;
			case h264::NAL_UNIT_TYPE_PPS:
			{
				if (pps_array.empty()) // TODO: multiple PPS fix
				{
					pps_array.emplace_back();
					h264::PPS& pps = pps_array.back();
					h264::read_pps(&pps, &bs);
				}
			}
			break;
			default:
				break;
			}
		}
		else
		{
			printf("found invalid NAL unit!\n");
			assert(0);
		}
	}
	if (!frame_infos.empty())
	{
		// patch size of last frame:
		Video::FrameInfo& frame_info = frame_infos.back();
		if (frame_info.size == 0)
		{
			frame_info.size = bs.byte_offset() - frame_info.offset;
		}
	}

	const float frame_duration = 1.0f / framerate;
	duration_seconds = 0;
	for (Video::FrameInfo& frame_info : frame_infos)
	{
		frame_info.duration_seconds = frame_duration;
		frame_info.timestamp_seconds = duration_seconds;
		duration_seconds += frame_info.duration_seconds;
	}

	CalculateFrameDisplayOrder();
	return true;
}

void Video::CalculateFrameDisplayOrder()
{
	int prev_poc_msb = 0;
	int prev_poc_lsb = 0;
	int gop = -1;
	for (size_t i = 0; i < frame_infos.size(); ++i)
	{
		const h264::SliceHeader& sh = slice_headers[i];
		const h264::PPS& pps = pps_array[sh.pic_parameter_set_id];
		const h264::SPS& sps = sps_array[pps.seq_parameter_set_id];
		const bool idr = frame_infos[i].is_intra;
		const bool is_ref = frame_infos[i].reference_priority != 0;
		const int max_poc_lsb = 1 << (sps.log2_max_pic_order_cnt_lsb_minus4 + 4);

		if (idr)
		{
			prev_poc_msb = 0;
			prev_poc_lsb = 0;
			gop++;
		}

		int poc_msb = 0;
		const int poc_lsb = sh.pic_order_cnt_lsb;
		if (poc_lsb < prev_poc_lsb && (prev_poc_lsb - poc_lsb) >= max_poc_lsb / 2)
			poc_msb = prev_poc_msb + max_poc_lsb;
		else if (poc_lsb > prev_poc_lsb && (poc_lsb - prev_poc_lsb) > max_poc_lsb / 2)
			poc_msb = prev_poc_msb - max_poc_lsb;
		else
			poc_msb = prev_poc_msb;

		frame_infos[i].poc = poc_msb + poc_lsb;
		frame_infos[i].gop = gop;

		if (is_ref)
		{
			prev_poc_msb = poc_msb;
			prev_poc_lsb = poc_lsb;
		}
	}

	std::vector<size_t> frame_display_order(frame_infos.size());
	for (size_t i = 0; i < frame_infos.size(); ++i)
		frame_display_order[i] = i;
	std::stable_sort(frame_display_order.begin(), frame_display_order.end(), [&](size_t a, size_t b) {
		const FrameInfo& frameA = frame_infos[a];
		const FrameInfo& frameB = frame_infos[b];
		if (frameA.gop != frameB.gop)
			return frameA.gop < frameB.gop;
		if (frameA.poc != frameB.poc)
			return frameA.poc < frameB.poc;
		return a < b;
		});
	for (size_t i = 0; i < frame_display_order.size(); ++i)
	{
		frame_infos[frame_display_order[i]].display_order = (int)i;
	}
}

int Video::CountUsed() const
{
	int count = 0;
	for (uint32_t i = 0; i < num_dpb_slots; ++i)
	{
		if (dpb[i].used)
			count++;
	}
	return count;
}

void Video::ClearDpb()
{
	for (DpbEntry& entry : dpb)
		entry = {};
	reference_usage.clear();
}

void Video::BeginDecodePicture(int frame_index)
{
	const FrameInfo& info = frame_infos[frame_index];
	const h264::SliceHeader& sh = slice_headers[frame_index];
	const h264::PPS& pps = pps_array[sh.pic_parameter_set_id];
	const h264::SPS& sps = sps_array[pps.seq_parameter_set_id];

	if (frame_index == 0 || info.is_intra)
		ClearDpb();

	int slot = -1;
	for (uint32_t i = 0; i < num_dpb_slots; ++i)
	{
		if (!dpb[i].used)
		{
			slot = (int)i;
			break;
		}
	}
	assert(slot >= 0);
	current_slot = (uint8_t)slot;

	for (uint32_t i = 0; i < 17; ++i)
	{
		poc_status[i] = 0;
		framenum_status[i] = 0;
	}
	poc_status[current_slot] = info.poc;
	framenum_status[current_slot] = sh.frame_num;

	const int max_frame_num = 1 << (sps.log2_max_frame_num_minus4 + 4);
	const int curr = sh.frame_num;
	reference_usage.clear();
	for (uint32_t i = 0; i < num_dpb_slots; ++i)
	{
		if (!dpb[i].used)
			continue;
		reference_usage.push_back((uint8_t)i);
		poc_status[i] = dpb[i].poc;
		framenum_status[i] = dpb[i].frame_num;
	}
	std::sort(reference_usage.begin(), reference_usage.end(), [&](uint8_t a, uint8_t b) {
		return PicNum(dpb[a].frame_num, curr, max_frame_num) > PicNum(dpb[b].frame_num, curr, max_frame_num);
		});
	assert(reference_usage.size() <= 16);
	assert(reference_usage.size() <= (size_t)std::max(sps.num_ref_frames, 1));
	for (uint8_t ref_slot : reference_usage)
		assert(ref_slot != current_slot);
}

void Video::EndDecodePicture(int frame_index)
{
	const FrameInfo& info = frame_infos[frame_index];
		const h264::SliceHeader& sh = slice_headers[frame_index];
		const h264::PPS& pps = pps_array[sh.pic_parameter_set_id];
		const h264::SPS& sps = sps_array[pps.seq_parameter_set_id];
		const int max_frame_num = 1 << (sps.log2_max_frame_num_minus4 + 4);
		const int curr = sh.frame_num;
		const int ref_limit = std::max(sps.num_ref_frames, 1);
		const bool idr = info.is_intra;

		if (info.reference_priority == 0)
		{
			assert(!dpb[current_slot].used);
			return;
		}

		if (!idr && sh.drpm.adaptive_ref_pic_marking_mode_flag)
		{
			for (int n = 0; n < 64; ++n)
			{
				const int op = sh.drpm.memory_management_control_operation[n];
				if (op == 0)
					break;
				if (op != 1)
					continue;
				const int pic_num = curr - (sh.drpm.difference_of_pic_nums_minus1[n] + 1);
				for (uint32_t i = 0; i < num_dpb_slots; ++i)
				{
					if (dpb[i].used && PicNum(dpb[i].frame_num, curr, max_frame_num) == pic_num)
					{
						dpb[i] = {};
						break;
					}
				}
			}
		}
		else if (!idr)
		{
			while (CountUsed() >= ref_limit)
			{
				int oldest = -1;
				int oldest_pic = 0;
				for (uint32_t i = 0; i < num_dpb_slots; ++i)
				{
					if (!dpb[i].used)
						continue;
					const int pic_num = PicNum(dpb[i].frame_num, curr, max_frame_num);
					if (oldest < 0 || pic_num < oldest_pic)
					{
						oldest = (int)i;
						oldest_pic = pic_num;
					}
				}
				if (oldest < 0)
					break;
				dpb[oldest] = {};
			}
		}

		dpb[current_slot].used = true;
		dpb[current_slot].frame_num = sh.frame_num;
		dpb[current_slot].poc = info.poc;
		assert(CountUsed() <= ref_limit);
}
