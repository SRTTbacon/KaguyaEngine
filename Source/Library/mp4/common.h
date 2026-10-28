#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cassert>
#include <chrono>
#include <algorithm>
#include <cstring>

static_assert(sizeof(void*) == sizeof(uint64_t), "Error: only 64-bit build is supported!");

#include "minimp4.h"	// mp4 -> h264 extraction
#include "h264.h"		// h264 parsing

// Video management utility:
struct Video
{
	// Video data that doesn't change during decoding:
	std::vector<uint8_t> h264_data; // raw h264 data with NAL start codes 0,0,0,1 or 0,0,1 before every NAL unit section
	uint32_t width = 0; // display width
	uint32_t height = 0; // display height
	uint32_t padded_width = 0; // decoder's working width
	uint32_t padded_height = 0; // decoder's working height
	uint32_t num_dpb_slots = 0; // maximum number of texture array slices in the Decoded Picture Buffer (DPB)
	std::vector<h264::SPS> sps_array; // array of h264 Picture Parameter Sets, usually there is only one
	std::vector<h264::PPS> pps_array; // array of h264 Sequence Parameter Sets, usually there is only one
	std::vector<h264::SliceHeader> slice_headers; // array of h264 Slice Headers, there are as many of them as video frames
	float duration_seconds = 0; // whole video duration in seconds
	struct FrameInfo
	{
		uint64_t offset = 0;
		uint64_t size = 0;
		float timestamp_seconds = 0;
		float duration_seconds = 0;
		uint32_t reference_priority = 0;
		int poc = 0;
		int gop = 0;
		int display_order = 0;
		bool is_intra = false;
	};
	std::vector<FrameInfo> frame_infos; // information about every frame in the order they were encontered inside the h264 stream (so they are in decode order, not display order)

	// Decoder timing state:
	int frameIndex = 0; // currently decoding h264 slice
	float time_until_next_frame = 0; // this is used to track when we need to swap displayed images, if it reaches 0 then we swap to the next one that can be displayed
	struct Timer
	{
		std::chrono::high_resolution_clock::time_point timestamp = std::chrono::high_resolution_clock::now();
		inline void record() { timestamp = std::chrono::high_resolution_clock::now(); }
		inline float elapsed_seconds() { return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - timestamp).count(); }
	} timer; // the timer is used to time the swapping of displayed pictures
	int target_display_order = 0; // the next FrameInfo::display_order that must be displayed

	// Decoded Picture Buffer (DPB) state.
	// reference_usage / *_status describe the references used to decode the current picture (before marking).
	// dpb[] is the marked reference set left behind for the next picture.
	struct DpbEntry
	{
		bool used = false;
		int frame_num = 0;
		int poc = 0;
	};
	DpbEntry dpb[17] = {};
	int poc_status[17] = {};
	int framenum_status[17] = {};
	std::vector<uint8_t> reference_usage;
	uint8_t current_slot = 0;

	// File loading helpers:

	// Extract the H264 data from an MP4 video file using the minimp4 library:
	bool Load_mp4(const char* filename);

	// Load from raw H264 data file (prefixed with 0,0,0,1 or 0,0,1 NAL unit start codes)
	bool Load_h264_raw(const char* filename, float framerate = 60.0f);

	static void RefIdxActive(const h264::SliceHeader& sh, const h264::PPS& pps, int& l0, int& l1)
	{
		l0 = pps.num_ref_idx_l0_active_minus1;
		l1 = pps.num_ref_idx_l1_active_minus1;
		if (sh.num_ref_idx_active_override_flag)
		{
			l0 = sh.num_ref_idx_l0_active_minus1;
			if (h264::is_slice_type(sh.slice_type, h264::SH_SLICE_TYPE_B))
				l1 = sh.num_ref_idx_l1_active_minus1;
		}
	}

	void CalculateFrameDisplayOrder();

	static int PicNum(int frame_num, int curr, int max_frame_num)
	{
		return frame_num > curr ? frame_num - max_frame_num : frame_num;
	}

	int CountUsed() const;

	void ClearDpb();

	void BeginDecodePicture(int frame_index);

	void EndDecodePicture(int frame_index);

};

// Other helpers:
#define arraysize(a) (sizeof(a) / sizeof(a[0]))

template<typename T>
constexpr T align(T value, T alignment)
{
	return ((value + alignment - T(1)) / alignment) * alignment;
}

#ifdef _WIN32
inline unsigned int firstbitlow(unsigned int value)
{
	unsigned long bit_index;
	if (_BitScanForward(&bit_index, value))
	{
		return (unsigned int)bit_index;
	}
	return 0;
}
#else
inline unsigned long firstbitlow(unsigned int value)
{
	if (value == 0)
	{
		return 0;
	}
	return __builtin_ctz(value);
}
#endif // _WIN32

// Windows specific things:
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wrl/client.h>
#endif // _WIN32

// Linux specific things:
#ifdef __linux__
#include<X11/Xlib.h>
#include <dlfcn.h>
#define GetProcAddress(handle,name) dlsym(handle, name)
#define ComPtr CComPtr
#define __RPC_FAR
#endif // __linux__

// DirectX Shader Compiler library is used for Vulkan and DirectX shader compilation:
#include "dxcapi.h"