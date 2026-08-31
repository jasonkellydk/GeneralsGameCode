/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "FramGrab.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <limits>

namespace
{
	void Write_FourCC(std::fstream &stream, const char (&value)[5])
	{
		stream.write(value, 4);
	}

	void Write_U16(std::fstream &stream, std::uint16_t value)
	{
		stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
	}

	void Write_U32(std::fstream &stream, std::uint32_t value)
	{
		stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
	}

	void Write_I32(std::fstream &stream, std::int32_t value)
	{
		stream.write(reinterpret_cast<const char *>(&value), sizeof(value));
	}

	std::uint32_t Tell32(std::fstream &stream)
	{
		const std::streampos position = stream.tellp();
		if (position < 0 || static_cast<std::uintmax_t>(position) > std::numeric_limits<std::uint32_t>::max())
		{
			return 0;
		}

		return static_cast<std::uint32_t>(position);
	}

	void Patch_U32(std::fstream &stream, std::uint32_t offset, std::uint32_t value)
	{
		const std::streampos return_position = stream.tellp();
		stream.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
		Write_U32(stream, value);
		stream.seekp(return_position, std::ios::beg);
	}

	std::uint32_t Chunk_Size(std::uint32_t size_offset, std::uint32_t end_offset)
	{
		return end_offset >= size_offset + sizeof(std::uint32_t) ?
			end_offset - size_offset - sizeof(std::uint32_t) : 0;
	}
}

FrameGrabClass::FrameGrabClass(const char *filename, MODE mode, int width, int height,
	int bitcount, float framerate) :
	Filename(filename == nullptr ? "" : filename),
	FrameRate(framerate > 0.0f ? framerate : 1.0f),
	Mode(mode),
	Counter(0),
	Width(width),
	Height(height),
	BitDepth(bitcount),
	FrameSize(0),
	RiffSizeOffset(0),
	AviHeaderFrameCountOffset(0),
	StreamHeaderFrameCountOffset(0),
	MovieListSizeOffset(0),
	MovieDataOffset(0)
{
	if (Mode != AVI || Width <= 0 || Height <= 0 || BitDepth != 24)
	{
		Mode = RAW;
		return;
	}

	const std::uint64_t row_size = (static_cast<std::uint64_t>(Width) * 3u + 3u) & ~3u;
	const std::uint64_t frame_size = row_size * static_cast<std::uint64_t>(Height);
	const std::uint64_t bitmap_size = static_cast<std::uint64_t>(Width) *
		static_cast<std::uint64_t>(Height) * 3u;
	if (row_size > std::numeric_limits<std::uint32_t>::max() ||
		frame_size > std::numeric_limits<std::uint32_t>::max() ||
		bitmap_size > std::numeric_limits<std::size_t>::max())
	{
		Mode = RAW;
		return;
	}

	FrameSize = static_cast<std::uint32_t>(frame_size);
	BitmapStorage.resize(static_cast<std::size_t>(bitmap_size));

	std::error_code error;
	std::filesystem::path output_path;
	for (unsigned int file_number = 0; ; ++file_number)
	{
		output_path = std::filesystem::path(Filename + std::to_string(file_number) + ".AVI");
		if (!std::filesystem::exists(output_path, error) || error)
		{
			break;
		}
	}

	Output.open(output_path.string(), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
	if (!Output.is_open())
	{
		Mode = RAW;
		return;
	}

	// Write a small uncompressed AVI container. This keeps movie capture
	// independent of platform multimedia APIs and accepts the bottom-up BGR
	// frames produced by WW3D::Update_Movie_Capture.
	Write_FourCC(Output, "RIFF");
	RiffSizeOffset = Tell32(Output);
	Write_U32(Output, 0);
	Write_FourCC(Output, "AVI ");

	Write_FourCC(Output, "LIST");
	const std::uint32_t header_list_size_offset = Tell32(Output);
	Write_U32(Output, 0);
	Write_FourCC(Output, "hdrl");

	Write_FourCC(Output, "avih");
	Write_U32(Output, 56);
	const std::uint32_t frames_per_second = std::max(1u,
		static_cast<unsigned int>(FrameRate + 0.5f));
	const std::uint32_t microseconds_per_frame =
		std::max(1u, 1000000u / frames_per_second);
	Write_U32(Output, microseconds_per_frame);
	Write_U32(Output, FrameSize * frames_per_second);
	Write_U32(Output, 0);
	Write_U32(Output, 0x00000010u);
	AviHeaderFrameCountOffset = Tell32(Output);
	Write_U32(Output, 0);
	Write_U32(Output, 0);
	Write_U32(Output, 1);
	Write_U32(Output, FrameSize);
	Write_U32(Output, static_cast<std::uint32_t>(Width));
	Write_U32(Output, static_cast<std::uint32_t>(Height));
	Write_U32(Output, 0);
	Write_U32(Output, 0);
	Write_U32(Output, 0);
	Write_U32(Output, 0);

	Write_FourCC(Output, "LIST");
	const std::uint32_t stream_list_size_offset = Tell32(Output);
	Write_U32(Output, 0);
	Write_FourCC(Output, "strl");

	Write_FourCC(Output, "strh");
	Write_U32(Output, 56);
	Write_FourCC(Output, "vids");
	Write_FourCC(Output, "\0\0\0\0");
	Write_U32(Output, 0);
	Write_U16(Output, 0);
	Write_U16(Output, 0);
	Write_U32(Output, 0);
	Write_U32(Output, 1);
	Write_U32(Output, frames_per_second);
	Write_U32(Output, 0);
	StreamHeaderFrameCountOffset = Tell32(Output);
	Write_U32(Output, 0);
	Write_U32(Output, FrameSize);
	Write_U32(Output, 0xffffffffu);
	Write_U32(Output, 0);
	Write_U16(Output, 0);
	Write_U16(Output, 0);
	Write_U16(Output, static_cast<std::uint16_t>(std::min(Width, 0x7fff)));
	Write_U16(Output, static_cast<std::uint16_t>(std::min(Height, 0x7fff)));

	Write_FourCC(Output, "strf");
	Write_U32(Output, 40);
	Write_U32(Output, 40);
	Write_I32(Output, Width);
	Write_I32(Output, Height);
	Write_U16(Output, 1);
	Write_U16(Output, static_cast<std::uint16_t>(BitDepth));
	Write_U32(Output, 0);
	Write_U32(Output, FrameSize);
	Write_I32(Output, 0);
	Write_I32(Output, 0);
	Write_U32(Output, 0);
	Write_U32(Output, 0);

	const std::uint32_t stream_list_end = Tell32(Output);
	Patch_U32(Output, stream_list_size_offset, Chunk_Size(stream_list_size_offset, stream_list_end));
	const std::uint32_t header_list_end = Tell32(Output);
	Patch_U32(Output, header_list_size_offset, Chunk_Size(header_list_size_offset, header_list_end));

	Write_FourCC(Output, "LIST");
	MovieListSizeOffset = Tell32(Output);
	Write_U32(Output, 0);
	Write_FourCC(Output, "movi");
	MovieDataOffset = Tell32(Output);
}

FrameGrabClass::~FrameGrabClass()
{
	if (Mode == AVI)
	{
		CleanupAVI();
	}
}

void FrameGrabClass::CleanupAVI()
{
	if (Mode != AVI)
	{
		return;
	}

	if (Output.is_open())
	{
		const std::uint32_t movie_end = Tell32(Output);
		Patch_U32(Output, MovieListSizeOffset, Chunk_Size(MovieListSizeOffset, movie_end));

		Write_FourCC(Output, "idx1");
		Write_U32(Output, static_cast<std::uint32_t>(FrameOffsets.size() * 16u));
		for (const std::uint32_t offset : FrameOffsets)
		{
			Write_FourCC(Output, "00db");
			Write_U32(Output, 0x00000010u);
			Write_U32(Output, offset);
			Write_U32(Output, FrameSize);
		}

		const std::uint32_t file_end = Tell32(Output);
		Patch_U32(Output, AviHeaderFrameCountOffset, Counter);
		Patch_U32(Output, StreamHeaderFrameCountOffset, Counter);
		Patch_U32(Output, RiffSizeOffset, file_end >= sizeof(std::uint32_t) ?
			file_end - sizeof(std::uint32_t) : 0);
		Output.flush();
		Output.close();
	}

	Mode = RAW;
}

void FrameGrabClass::GrabAVI(void *BitmapPointer)
{
	if (!Output.is_open() || BitmapPointer == nullptr)
	{
		return;
	}

	const std::uint32_t frame_offset = Tell32(Output) - MovieDataOffset;
	FrameOffsets.push_back(frame_offset);
	Write_FourCC(Output, "00db");
	Write_U32(Output, FrameSize);

	const auto *source = static_cast<const std::uint8_t *>(BitmapPointer);
	const std::uint32_t row_bytes = static_cast<std::uint32_t>(Width) * 3u;
	const std::uint32_t row_padding = FrameSize / static_cast<std::uint32_t>(Height) - row_bytes;
	const std::uint8_t zero_padding[3] = { 0, 0, 0 };
	for (int row = 0; row < Height; ++row)
	{
		Output.write(reinterpret_cast<const char *>(source + static_cast<std::size_t>(row) * row_bytes), row_bytes);
		Output.write(reinterpret_cast<const char *>(zero_padding), row_padding);
	}

	++Counter;
}

void FrameGrabClass::GrabRawFrame(void * /*BitmapPointer*/)
{
}

void FrameGrabClass::ConvertGrab(void *BitmapPointer)
{
	if (BitmapPointer == nullptr || BitmapStorage.empty())
	{
		return;
	}

	ConvertFrame(BitmapPointer);
	Grab(BitmapStorage.data());
}

void FrameGrabClass::Grab(void *BitmapPointer)
{
	if (Mode == AVI)
	{
		GrabAVI(BitmapPointer);
	}
	else
	{
		GrabRawFrame(BitmapPointer);
	}
}

void FrameGrabClass::ConvertFrame(void *BitmapPointer)
{
	const auto *source = static_cast<const std::uint8_t *>(BitmapPointer);
	const std::size_t source_row_bytes = static_cast<std::size_t>(Width) * 4u;
	const std::size_t destination_row_bytes = static_cast<std::size_t>(Width) * 3u;
	for (int row = 0; row < Height; ++row)
	{
		const std::uint8_t *source_row = source + static_cast<std::size_t>(Height - row - 1) * source_row_bytes;
		std::uint8_t *destination_row = BitmapStorage.data() + static_cast<std::size_t>(row) * destination_row_bytes;
		for (int column = 0; column < Width; ++column)
		{
			const std::uint8_t *source_pixel = source_row + static_cast<std::size_t>(column) * 4u;
			std::uint8_t *destination_pixel = destination_row + static_cast<std::size_t>(column) * 3u;
			destination_pixel[0] = source_pixel[0];
			destination_pixel[1] = source_pixel[1];
			destination_pixel[2] = source_pixel[2];
		}
	}
}
