/**************************************************************************/
/*  multi_uma_buffer.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef MULTI_UMA_BUFFER_H
#define MULTI_UMA_BUFFER_H

#include "servers/rendering_server.h"

class MultiUmaBufferBase {
protected:
	LocalVector<RID> buffers;
	uint32_t curr_idx = UINT32_MAX;
	uint64_t last_frame_mapped = UINT64_MAX;
	const uint32_t max_extra_buffers;
#ifdef DEBUG_ENABLED
	const char *debug_name;
#endif

	MultiUmaBufferBase(uint32_t p_max_extra_buffers, const char *p_debug_name) :
			max_extra_buffers(p_max_extra_buffers)
#ifdef DEBUG_ENABLED
			,
			debug_name(p_debug_name)
#endif
	{
	}

#ifdef DEV_ENABLED
	~MultiUmaBufferBase() {
		DEV_ASSERT(buffers.is_empty() && "Forgot to call uninit()!");
	}
#endif

public:
	void uninit() {
		print_verbose("MultiUmaBuffer '"
#ifdef DEBUG_ENABLED
				+ String(debug_name) +
#else
					  "{DEBUG_ENABLED unavailable}"
#endif
				"' used a total of " + itos(buffers.size()) + " buffers. A large number may indicate a waste of VRAM and can be brought down by tweaking MAX_EXTRA_BUFFERS for this buffer.");

		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		for (RID buffer : buffers) {
			if (buffer.is_valid()) {
				rd->free(buffer);
			}
		}

		buffers.clear();
	}

	void shrink_to_max_extra_buffers() {
		DEV_ASSERT(curr_idx == 0u && "This function can only be called after reset and before being upload_and_advance again!");

		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		uint32_t elem_count = buffers.size();

		if (elem_count > max_extra_buffers) {
			print_verbose("MultiUmaBuffer '"
#ifdef DEBUG_ENABLED
					+ String(debug_name) +
#else
						  "{DEBUG_ENABLED unavailable}"
#endif
					"' peaked to " + itos(elem_count) + " elements and shrinking it to " + itos(max_extra_buffers) + ". If you see this message often, then something is wrong with rendering or MAX_EXTRA_BUFFERS needs to be increased.");
		}

		while (elem_count > max_extra_buffers) {
			--elem_count;
			if (buffers[elem_count].is_valid()) {
				rd->free(buffers[elem_count]);
			}
			buffers.remove_at(elem_count);
		}
	}
};

template <uint32_t NUM_BUFFERS, uint32_t MAX_EXTRA_BUFFERS = UINT32_MAX>
class MultiUmaBuffer : public MultiUmaBufferBase {
private:
	uint32_t buffer_sizes[NUM_BUFFERS] = {};
#ifdef DEV_ENABLED
	bool can_upload[NUM_BUFFERS] = {};
#endif

	void push() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();
		for (uint32_t i = 0u; i < NUM_BUFFERS; ++i) {
			const bool is_storage = buffer_sizes[i] & 0x80000000u;
			const uint32_t size_bytes = buffer_sizes[i] & ~0x80000000u;
			RID buffer;
			if (is_storage) {
				buffer = rd->storage_buffer_create(size_bytes, Vector<uint8_t>(), 0, RD::BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT);
			} else {
				buffer = rd->uniform_buffer_create(size_bytes, Vector<uint8_t>(), RD::BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT);
			}
			buffers.push_back(buffer);
		}
	}

public:
	MultiUmaBuffer(const char *p_debug_name) :
			MultiUmaBufferBase(MAX_EXTRA_BUFFERS, p_debug_name) {}

	uint32_t get_curr_idx() const { return curr_idx; }

	void set_size(uint32_t idx, uint32_t p_size_bytes, bool p_is_storage) {
		DEV_ASSERT(buffers.is_empty());
		buffer_sizes[idx] = p_size_bytes | (p_is_storage ? 0x80000000u : 0u);
		curr_idx = UINT32_MAX;
	}

	uint32_t get_size(uint32_t idx) const { return buffer_sizes[idx] & ~0x80000000u; }

	// Gets the raw buffer. Use with care.
	// If you call this function, make sure to have called prepare_for_upload() first.
	// Do not call _get() then prepare_for_upload().
	RID _get(uint32_t idx) {
		return buffers[curr_idx * NUM_BUFFERS + idx];
	}

	void prepare_for_upload() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();
		const uint64_t frames_drawn = rd->get_frames_drawn();

		if (last_frame_mapped == frames_drawn) {
			++curr_idx;
		} else {
			curr_idx = 0u;
			if (max_extra_buffers != UINT32_MAX) {
				shrink_to_max_extra_buffers();
			}
		}
		last_frame_mapped = frames_drawn;
		if (curr_idx * NUM_BUFFERS >= buffers.size()) {
			push();
		}

#ifdef DEV_ENABLED
		for (size_t i = 0u; i < NUM_BUFFERS; ++i) {
			can_upload[i] = true;
		}
#endif
	}

	RID get_for_upload(uint32_t idx) {
#ifdef DEV_ENABLED
		DEV_ASSERT(can_upload[idx] && "Forgot to prepare_for_upload first! Or called get_for_upload/upload() twice.");
		can_upload[idx] = false;
#endif
		return buffers[curr_idx * NUM_BUFFERS + idx];
	}

	void upload(uint32_t idx, const void *p_src_data, uint32_t size_bytes) {
#ifdef DEV_ENABLED
		DEV_ASSERT(can_upload[idx] && "Forgot to prepare_for_upload first! Or called get_for_upload/upload() twice.");
		can_upload[idx] = false;
#endif
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();
		rd->buffer_update(buffers[curr_idx * NUM_BUFFERS + idx], 0, size_bytes, p_src_data, true);
	}
};
#endif // MULTI_UMA_BUFFER_H
