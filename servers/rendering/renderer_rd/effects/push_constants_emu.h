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

namespace RendererRD {

template <typename T, uint32_t set_idx = 2u>
struct PushConstantsEmu {
	struct ParamsUniform {
		RID buffer;
		RID set;
	};

	RID shader;

	LocalVector<ParamsUniform> params_uniform;
	uint32_t curr_idx = 0u;

private:
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

		pu.set = rd->uniform_set_create(params_uniforms, shader, set_idx);

		params_uniform.push_back(pu);
	}

public:
	~PushConstantsEmu() {
		RenderingDevice *rd = RD::RenderingDevice::get_singleton();
		for (const ParamsUniform &pu : params_uniform) {
			if (pu.set.is_valid()) {
				rd->free(pu.set);
			}
			if (pu.buffer.is_valid()) {
				rd->free(pu.buffer);
			}
		}
	}

	ParamsUniform upload_and_advance(const T &p_src_data) {
		if (curr_idx >= params_uniform.size()) {
			push();
		}

		RD::RenderingDevice::get_singleton()->buffer_update(params_uniform[curr_idx].buffer, 0, sizeof(T), &p_src_data);

		return params_uniform[curr_idx++];
	}
};
} //namespace RendererRD

#endif // PUSH_CONSTANTS_EMU_RD_H
