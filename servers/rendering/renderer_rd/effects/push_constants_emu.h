/**************************************************************************/
/*  push_constants_emu.h                                                  */
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

#ifndef PUSH_CONSTANTS_EMU_RD_H
#define PUSH_CONSTANTS_EMU_RD_H

#include "servers/rendering_server.h"

#define ERR_PC_RENDER_THREAD_MSG String("This function (") + String(__func__) + String(") can only be called from the render thread. ")
#define ERR_PC_RENDER_THREAD_GUARD() ERR_FAIL_COND_MSG(render_thread_id != Thread::get_caller_id(), ERR_PC_RENDER_THREAD_MSG);

namespace RendererRD {

template <typename T, uint32_t SET_IDX = 2u, uint32_t MAX_EXTRA_BUFFERS = UINT32_MAX>
struct PushConstantsEmu {
	struct ParamsUniform {
		RID buffer;
		RID set;
	};

private:
	RID shader;

	LocalVector<ParamsUniform> params_uniform;
	uint32_t curr_idx = 0u;

	void push() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		ParamsUniform pu;
		pu.buffer = rd->uniform_buffer_create(sizeof(T), Vector<uint8_t>(), RD::BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT);

		Vector<RD::Uniform> params_uniforms;
		RD::Uniform u;
		u.binding = 0;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER_DYNAMIC;
		u.append_id(pu.buffer);
		params_uniforms.push_back(u);

		pu.set = rd->uniform_set_create(params_uniforms, shader, SET_IDX);

		params_uniform.push_back(pu);
	}

	void shrink_to(const uint32_t p_new_size) {
		DEV_ASSERT(curr_idx == 0u && "This function can only be called after reset and before being upload_and_advance again!");

		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		uint32_t elem_count = params_uniform.size();
		while (elem_count >= p_new_size) {
			--elem_count;
			if (params_uniform[elem_count].set.is_valid()) {
				rd->free(params_uniform[elem_count].set);
			}
			if (params_uniform[elem_count].buffer.is_valid()) {
				rd->free(params_uniform[elem_count].buffer);
			}
			params_uniform.remove_at(elem_count);
		}
	}

public:
#ifdef DEV_ENABLED
	~PushConstantsEmu() {
		DEV_ASSERT(shader.is_null());
	}
#endif

	void init(RID p_shader) {
		shader = p_shader;
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();
		rd->_register_push_constant_emu(&this->curr_idx);
	}

	void uninit() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		rd->_unregister_push_constant_emu(&this->curr_idx);

		for (const ParamsUniform &pu : params_uniform) {
			if (pu.set.is_valid()) {
				rd->free(pu.set);
			}
			if (pu.buffer.is_valid()) {
				rd->free(pu.buffer);
			}
		}

		shader = RID();
	}

	void _reset() {
		curr_idx = 0u;
		if (MAX_EXTRA_BUFFERS != UINT32_MAX) {
			shrink_to(MAX_EXTRA_BUFFERS);
		}
	}

	ParamsUniform upload_and_advance(const T &p_src_data) {
		if (curr_idx >= params_uniform.size()) {
			push();
		}

		RD::RenderingDevice::get_singleton()->buffer_update(params_uniform[curr_idx].buffer, 0, sizeof(T), &p_src_data, true);

		return params_uniform[curr_idx++];
	}
};

template <typename T, typename S, uint32_t MAX_EXTRA_BUFFERS = UINT32_MAX>
struct PushConstantsEmuEmbedded {
	struct ParamsUniform {
		RID buffer;
		RID set;
	};

private:
	LocalVector<ParamsUniform> params_uniform;
	uint32_t curr_idx = 0u;

	void push(S *p_embed_owner) {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		ParamsUniform pu;
		pu.buffer = rd->uniform_buffer_create(sizeof(T), Vector<uint8_t>(), RD::BUFFER_CREATION_DYNAMIC_PERSISTENT_BIT);
		pu.set = p_embed_owner->_create_push_constant_uniform_set(pu.buffer);
		params_uniform.push_back(pu);
	}

	void shrink_to(const uint32_t p_new_size) {
		DEV_ASSERT(curr_idx == 0u && "This function can only be called after reset and before being upload_and_advance again!");

		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		uint32_t elem_count = params_uniform.size();
		while (elem_count >= p_new_size) {
			--elem_count;
			if (params_uniform[elem_count].set.is_valid()) {
				rd->free(params_uniform[elem_count].set);
			}
			if (params_uniform[elem_count].buffer.is_valid()) {
				rd->free(params_uniform[elem_count].buffer);
			}
			params_uniform.remove_at(elem_count);
		}
	}

public:
#ifdef DEV_ENABLED
	~PushConstantsEmuEmbedded() {
		DEV_ASSERT(params_uniform.is_empty());
	}
#endif

	void init() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();
		rd->_register_push_constant_emu(&this->curr_idx);
	}

	void uninit() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();

		rd->_unregister_push_constant_emu(&this->curr_idx);

		for (const ParamsUniform &pu : params_uniform) {
			if (pu.set.is_valid()) {
				rd->free(pu.set);
			}
			if (pu.buffer.is_valid()) {
				rd->free(pu.buffer);
			}
		}
		params_uniform.clear();
	}

	void _reset() {
		curr_idx = 0u;
		if (MAX_EXTRA_BUFFERS != UINT32_MAX) {
			shrink_to(MAX_EXTRA_BUFFERS);
		}
	}

	ParamsUniform upload_and_advance(const T &p_src_data, S *p_embed_owner) {
		if (curr_idx >= params_uniform.size()) {
			push(p_embed_owner);
		}

		RD::RenderingDevice::get_singleton()->buffer_update(params_uniform[curr_idx].buffer, 0, sizeof(T), &p_src_data, true);

		return params_uniform[curr_idx++];
	}
};

} //namespace RendererRD

#endif // PUSH_CONSTANTS_EMU_RD_H
