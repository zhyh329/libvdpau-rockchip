/*
 * Copyright (c) 2013 Jens Kuske <jenskuske@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <string.h>
#include "vdpau_private.h"
#include "gles.h"

VdpStatus vdp_video_surface_create(VdpDevice device,
                                   VdpChromaType chroma_type,
                                   uint32_t width,
                                   uint32_t height,
                                   VdpVideoSurface *surface)
{
	if (!surface)
		return VDP_STATUS_INVALID_POINTER;

	if (!width || !height)
		return VDP_STATUS_INVALID_SIZE;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	video_surface_ctx_t *vs = calloc(1, sizeof(video_surface_ctx_t));
	if (!vs)
		return VDP_STATUS_RESOURCES;

	vs->device = dev;
	vs->width = width;
	vs->height = height;
	vs->chroma_type = chroma_type;

	switch (chroma_type)
	{
	case VDP_CHROMA_TYPE_420:
		break;
	default:
		free(vs);
		return VDP_STATUS_INVALID_CHROMA_TYPE;
	}

	VDPAU_DBG ("egl make context current");
    if (!eglMakeCurrent(dev->egl.display, dev->egl.surface,
                        dev->egl.surface, dev->egl.context)) {
        VDPAU_DBG ("Could not set EGL context to current %x", eglGetError());
        return VDP_STATUS_RESOURCES;
    }

	vs->y_tex = gl_create_texture(GL_NEAREST);
    vs->u_tex = gl_create_texture(GL_NEAREST);
    vs->v_tex = gl_create_texture(GL_NEAREST);
    vs->rgb_tex = gl_create_texture(GL_LINEAR);
    
    glGenFramebuffers (1, &vs->framebuffer);
	CHECKEGL

    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                  GL_UNSIGNED_BYTE, NULL);
	CHECKEGL

    glBindFramebuffer (GL_FRAMEBUFFER, vs->framebuffer);
 	CHECKEGL

    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, vs->rgb_tex, 0);
	CHECKEGL

    if (!eglMakeCurrent(dev->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
        VDPAU_DBG ("Could not set EGL context to none %x", eglGetError());
        return VDP_STATUS_RESOURCES;
    }

	int handle = handle_create(vs);
	if (handle == -1)
	{
		free(vs);
		return VDP_STATUS_RESOURCES;
	}

	*surface = handle;

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_destroy(VdpVideoSurface surface)
{
	video_surface_ctx_t *vs = handle_get(surface);
	if (!vs)
		return VDP_STATUS_INVALID_HANDLE;

    const GLuint framebuffers[] = {
        vs->framebuffer
    };

    const GLuint textures[] = {
        vs->y_tex,
        vs->u_tex,
        vs->v_tex,
        vs->rgb_tex
    };

	glDeleteFramebuffers (1, framebuffers);
	glDeleteTextures (4, textures);

	if (vs->decoder_private_free)
		vs->decoder_private_free(vs);

	handle_destroy(surface);
	free(vs);

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_get_parameters(VdpVideoSurface surface,
                                           VdpChromaType *chroma_type,
                                           uint32_t *width,
                                           uint32_t *height)
{
	video_surface_ctx_t *vid = handle_get(surface);
	if (!vid)
		return VDP_STATUS_INVALID_HANDLE;

	if (chroma_type)
		*chroma_type = vid->chroma_type;

	if (width)
		*width = vid->width;

	if (height)
		*height = vid->height;

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_get_bits_y_cb_cr(VdpVideoSurface surface,
                                             VdpYCbCrFormat destination_ycbcr_format,
                                             void *const *destination_data,
                                             uint32_t const *destination_pitches)
{
	video_surface_ctx_t *vs = handle_get(surface);
	if (!vs)
		return VDP_STATUS_INVALID_HANDLE;


	return VDP_STATUS_ERROR;
}

VdpStatus vdp_video_surface_put_bits_y_cb_cr(VdpVideoSurface surface,
                                             VdpYCbCrFormat source_ycbcr_format,
                                             void const *const *source_data,
                                             uint32_t const *source_pitches)
{
	video_surface_ctx_t *vs = handle_get(surface);
	if (!vs)
		return VDP_STATUS_INVALID_HANDLE;

	vs->source_format = source_ycbcr_format;

	switch (source_ycbcr_format)
	{
	case VDP_YCBCR_FORMAT_YUYV:
	case VDP_YCBCR_FORMAT_UYVY:
		if (vs->chroma_type != VDP_CHROMA_TYPE_422)
			return VDP_STATUS_INVALID_CHROMA_TYPE;
	/*
		src = source_data[0];
		dst = vs->data;
		for (i = 0; i < vs->height; i++) {
			memcpy(dst, src, 2*vs->width);
			src += source_pitches[0];
			dst += 2*vs->width;
		}
	*/
		break;
	case VDP_YCBCR_FORMAT_Y8U8V8A8:
	case VDP_YCBCR_FORMAT_V8U8Y8A8:

		break;

	case VDP_YCBCR_FORMAT_NV12:
		if (vs->chroma_type != VDP_CHROMA_TYPE_420)
			return VDP_STATUS_INVALID_CHROMA_TYPE;
		VDPAU_DBG("NV12");
		/*
		src = source_data[0];
		dst = vs->data;
		for (i = 0; i < vs->height; i++) {
			memcpy(dst, src, vs->width);
			src += source_pitches[0];
			dst += vs->width;
		}
		src = source_data[1];
		dst = vs->data + vs->plane_size;
		for (i = 0; i < vs->height / 2; i++) {
			memcpy(dst, src, vs->width);
			src += source_pitches[1];
			dst += vs->width;
		}
		*/
		break;

	case VDP_YCBCR_FORMAT_YV12:
		if (vs->chroma_type != VDP_CHROMA_TYPE_420)
			return VDP_STATUS_INVALID_CHROMA_TYPE;
		if (vs->width != source_pitches[0]) {
			VDPAU_DBG("YV12 %d, %d %d %d", vs->width, source_pitches[0], source_pitches[1], source_pitches[2]);
		}
		
		GLfloat vVertices[] =
		{
			-1.0f, -1.0f,
			0.0f, 1.0f,

			1.0f, -1.0f,
			1.0f, 1.0f,

			1.0f, 1.0f,
			1.0f, 0.0f,

			-1.0f, 1.0f,
			0.0f, 0.0f,
		};
		GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
		device_ctx_t *dev = vs->device;
		
		if (!eglMakeCurrent(dev->egl.display, dev->egl.surface,
							dev->egl.surface, dev->egl.context)) {
			VDPAU_DBG ("Could not set EGL context to current %x", eglGetError());
			return VDP_STATUS_RESOURCES;
		}
 
		glBindFramebuffer (GL_FRAMEBUFFER, vs->framebuffer);
		CHECKEGL
		
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER) ;
		if(status != GL_FRAMEBUFFER_COMPLETE) {
			VDPAU_DBG("failed to make complete framebuffer object %x", status);
		}

		glUseProgram (vs->device->egl.deinterlace.program);
		CHECKEGL

		glViewport(0, 0, vs->width, vs->height);
		CHECKEGL

		glClear (GL_COLOR_BUFFER_BIT);
		CHECKEGL

		glVertexAttribPointer (vs->device->egl.deinterlace.position_loc, 2,
							   GL_FLOAT, GL_FALSE, 4 * sizeof (GLfloat),
							   vVertices);
		CHECKEGL
		glEnableVertexAttribArray (vs->device->egl.deinterlace.position_loc);
		CHECKEGL

		glVertexAttribPointer (vs->device->egl.deinterlace.texcoord_loc, 2,
							   GL_FLOAT, GL_FALSE, 4 * sizeof (GLfloat),
							   &vVertices[2]);
		CHECKEGL
		glEnableVertexAttribArray (vs->device->egl.deinterlace.texcoord_loc);
		CHECKEGL

	    /* y component */
		glActiveTexture(GL_TEXTURE0);
		CHECKEGL
		glBindTexture (GL_TEXTURE_2D, vs->y_tex);
		CHECKEGL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, vs->width,
					 vs->height, 0, GL_LUMINANCE,
					 GL_UNSIGNED_BYTE, source_data[0]);
		CHECKEGL
		glUniform1i (vs->device->egl.y_tex_loc, 0);
		CHECKEGL

		/* u component */
		glActiveTexture(GL_TEXTURE1);
		CHECKEGL
		glBindTexture (GL_TEXTURE_2D, vs->u_tex);
		CHECKEGL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, vs->width/2,
					 vs->height/2, 0, GL_LUMINANCE,
					 GL_UNSIGNED_BYTE, source_data[2]);
		CHECKEGL
		glUniform1i (vs->device->egl.u_tex_loc, 1);
		CHECKEGL

		/* v component */
		glActiveTexture(GL_TEXTURE2);
		CHECKEGL
		glBindTexture (GL_TEXTURE_2D, vs->v_tex);
		CHECKEGL
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, vs->width/2,
					 vs->height/2, 0, GL_LUMINANCE,
					 GL_UNSIGNED_BYTE, source_data[1]);
		CHECKEGL
		glUniform1i (vs->device->egl.v_tex_loc, 2);
		CHECKEGL

		glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
		CHECKEGL
		
		glUseProgram(0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		if (!eglMakeCurrent(dev->egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
			VDPAU_DBG ("Could not set EGL context to none %x", eglGetError());
			return VDP_STATUS_RESOURCES;
		}

		break;
	}

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_query_capabilities(VdpDevice device,
                                               VdpChromaType surface_chroma_type,
                                               VdpBool *is_supported,
                                               uint32_t *max_width,
                                               uint32_t *max_height)
{
	if (!is_supported || !max_width || !max_height)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = surface_chroma_type == VDP_CHROMA_TYPE_420;
	*max_width = 8192;
	*max_height = 8192;

	return VDP_STATUS_OK;
}

VdpStatus vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(VdpDevice device,
                                                                    VdpChromaType surface_chroma_type,
                                                                    VdpYCbCrFormat bits_ycbcr_format,
                                                                    VdpBool *is_supported)
{
	if (!is_supported)
		return VDP_STATUS_INVALID_POINTER;

	device_ctx_t *dev = handle_get(device);
	if (!dev)
		return VDP_STATUS_INVALID_HANDLE;

	*is_supported = VDP_FALSE;

	return VDP_STATUS_OK;
}
