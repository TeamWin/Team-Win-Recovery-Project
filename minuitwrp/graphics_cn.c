/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include "../common.h"
#include <pixelflinger/pixelflinger.h>

#include "minui.h"

#ifdef BOARD_USE_CUSTOM_RECOVERY_FONT
#include BOARD_USE_CUSTOM_RECOVERY_FONT
#else
#include "font_10x18.h"
#endif

#include "font.h"
#include "font_16x16_cn.h"
#include "font_32x32_cn.h"
#include "unicode_map.h"

#ifdef RECOVERY_BGRA
#define PIXEL_FORMAT GGL_PIXEL_FORMAT_BGRA_8888
#define PIXEL_SIZE 4
#endif
#ifdef RECOVERY_RGBX
#define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGBX_8888
#define PIXEL_SIZE 4
#endif
#ifndef PIXEL_FORMAT
#define PIXEL_FORMAT GGL_PIXEL_FORMAT_RGB_565
#define PIXEL_SIZE 2
#endif

#define PRINT_SCREENINFO 1 // Enables printing of screen info to log

typedef struct {
	unsigned fontver;
    GGLSurface texture;
    unsigned offset[97];
    unsigned cheight;
    unsigned ascent;
} GRFont;

typedef struct {
	unsigned fontver;
    GGLSurface texture;
    unsigned ascent;
	unsigned en_num;
	unsigned cn_num;
    CHAR_LEN_TYPE ewidth;
    CHAR_LEN_TYPE eheight;
    CHAR_LEN_TYPE cwidth;
    CHAR_LEN_TYPE cheight;
    unsigned char *fontdata;
    unsigned char *fontindex_en;
    unsigned short *fontindex_cn;
    unsigned char *width_offset_en;
    unsigned char *width_offset_cn;
} GRFontCN;

static GRFont *gr_font = 0;
static GGLContext *gr_context = 0;
static GGLSurface gr_font_texture;
static GGLSurface gr_framebuffer[2];
static GGLSurface gr_mem_surface;
static unsigned gr_active_fb = 0;

static GRFontCN *gr_font_cn = 0;
static GRFontCN *gr_font_cn2 = 0;

static int gr_fb_fd = -1;
static int gr_vt_fd = -1;

static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;

#ifdef PRINT_SCREENINFO
static void print_fb_var_screeninfo()
{
	LOGI("vi.xres: %d\n", vi.xres);
	LOGI("vi.yres: %d\n", vi.yres);
	LOGI("vi.xres_virtual: %d\n", vi.xres_virtual);
	LOGI("vi.yres_virtual: %d\n", vi.yres_virtual);
	LOGI("vi.xoffset: %d\n", vi.xoffset);
	LOGI("vi.yoffset: %d\n", vi.yoffset);
	LOGI("vi.bits_per_pixel: %d\n", vi.bits_per_pixel);
	LOGI("vi.grayscale: %d\n", vi.grayscale);
}
#endif

static int get_framebuffer(GGLSurface *fb)
{
    int fd;
    void *bits;

    fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) 
    {
        perror("cannot open fb0");
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) 
    {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    fprintf(stderr, "Pixel format: %dx%d @ %dbpp\n", vi.xres, vi.yres, vi.bits_per_pixel);

    vi.bits_per_pixel = PIXEL_SIZE * 8;
    if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_BGRA_8888) 
    {
        fprintf(stderr, "Pixel format: BGRA_8888\n");
        if (PIXEL_SIZE != 4)    fprintf(stderr, "E: Pixel Size mismatch!\n");
        vi.red.offset     = 8;
        vi.red.length     = 8;
        vi.green.offset   = 16;
        vi.green.length   = 8;
        vi.blue.offset    = 24;
        vi.blue.length    = 8;
        vi.transp.offset  = 0;
        vi.transp.length  = 8;
    } 
    else if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_RGBX_8888) 
    {
        fprintf(stderr, "Pixel format: RGBX_8888\n");
        if (PIXEL_SIZE != 4)    fprintf(stderr, "E: Pixel Size mismatch!\n");
        vi.red.offset     = 24;
        vi.red.length     = 8;
        vi.green.offset   = 16;
        vi.green.length   = 8;
        vi.blue.offset    = 8;
        vi.blue.length    = 8;
        vi.transp.offset  = 0;
        vi.transp.length  = 8;
    } 
    else if (PIXEL_FORMAT == GGL_PIXEL_FORMAT_RGB_565) 
    {
#ifdef RECOVERY_RGB_565
		fprintf(stderr, "Pixel format: RGB_565\n");
		vi.blue.offset    = 0;
		vi.green.offset   = 5;
		vi.red.offset     = 11;
#else
        fprintf(stderr, "Pixel format: BGR_565\n");
		vi.blue.offset    = 11;
		vi.green.offset   = 5;
		vi.red.offset     = 0;
#endif
		if (PIXEL_SIZE != 2)    fprintf(stderr, "E: Pixel Size mismatch!\n");
		vi.blue.length    = 5;
		vi.green.length   = 6;
		vi.red.length     = 5;
        vi.blue.msb_right = 0;
        vi.green.msb_right = 0;
        vi.red.msb_right = 0;
        vi.transp.offset  = 0;
        vi.transp.length  = 0;
    }
    else
    {
        perror("unknown pixel format");
        close(fd);
        return -1;
    }

    vi.vmode = FB_VMODE_NONINTERLACED;
    vi.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

    if (ioctl(fd, FBIOPUT_VSCREENINFO, &vi) < 0) 
    {
        perror("failed to put fb0 info");
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) 
    {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bits == MAP_FAILED) 
    {
        perror("failed to mmap framebuffer");
        close(fd);
        return -1;
    }

#ifdef RECOVERY_GRAPHICS_USE_LINELENGTH
    vi.xres_virtual = fi.line_length / PIXEL_SIZE;
#endif

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
#ifdef BOARD_HAS_JANKY_BACKBUFFER
    LOGI("setting JANKY BACKBUFFER\n");
    fb->stride = fi.line_length/2;
#else
    fb->stride = vi.xres_virtual;
#endif
    fb->data = bits;
    fb->format = PIXEL_FORMAT;
    memset(fb->data, 0, vi.yres * fb->stride * PIXEL_SIZE);

    fb++;

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
#ifdef BOARD_HAS_JANKY_BACKBUFFER
    fb->stride = fi.line_length/2;
    fb->data = (void*) (((unsigned) bits) + vi.yres * fi.line_length);
#else
    fb->stride = vi.xres_virtual;
    fb->data = (void*) (((unsigned) bits) + vi.yres * fb->stride * PIXEL_SIZE);
#endif
    fb->format = PIXEL_FORMAT;
    memset(fb->data, 0, vi.yres * fb->stride * PIXEL_SIZE);

#ifdef PRINT_SCREENINFO
	print_fb_var_screeninfo();
#endif

    return fd;
}

static void get_memory_surface(GGLSurface* ms) 
{
  ms->version = sizeof(*ms);
  ms->width = vi.xres;
  ms->height = vi.yres;
  ms->stride = vi.xres_virtual;
  ms->data = malloc(vi.xres_virtual * vi.yres * PIXEL_SIZE);
  ms->format = PIXEL_FORMAT;
}

static void set_active_framebuffer(unsigned n)
{
    if (n > 1) return;
    vi.yres_virtual = vi.yres * 2;
    vi.yoffset = n * vi.yres;
//    vi.bits_per_pixel = PIXEL_SIZE * 8;
    if (ioctl(gr_fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) 
    {
        perror("active fb swap failed");
    }
}

void gr_flip(void)
{
    GGLContext *gl = gr_context;

    /* swap front and back buffers */
    gr_active_fb = (gr_active_fb + 1) & 1;

#ifdef BOARD_HAS_FLIPPED_SCREEN
    /* flip buffer 180 degrees for devices with physicaly inverted screens */
    unsigned int i;
    for (i = 1; i < (vi.xres * vi.yres); i++) 
    {
        unsigned short tmp = gr_mem_surface.data[i];
        gr_mem_surface.data[i] = gr_mem_surface.data[(vi.xres * vi.yres * 2) - i];
        gr_mem_surface.data[(vi.xres * vi.yres * 2) - i] = tmp;
    }
#endif

    /* copy data from the in-memory surface to the buffer we're about
     * to make active. */
    memcpy(gr_framebuffer[gr_active_fb].data, gr_mem_surface.data, vi.xres_virtual * vi.yres * PIXEL_SIZE);

    /* inform the display driver */
    set_active_framebuffer(gr_active_fb);
}

void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GGLContext *gl = gr_context;
    GGLint color[4];
    color[0] = ((r << 8) | r) + 1;
    color[1] = ((g << 8) | g) + 1;
    color[2] = ((b << 8) | b) + 1;
    color[3] = ((a << 8) | a) + 1;
    gl->color4xv(gl, color);
}

//====================================== OFFICIAL FONT EN ========================================================================//
int gr_measureEx_en(const char *s, void* pFont)
{
    GRFont* font = (GRFont*)pFont;
    int total = 0;
    unsigned pos;
    unsigned off;

    if (!font)   
		font = gr_font;

    while ((off = *s++))
    {
        off -= 32;
        if (off < 96)
            total += (font->offset[off+1] - font->offset[off]);
    }
    return total;
}

unsigned character_width_en(const char *s, void* pFont)
{
	GRFont *font = (GRFont*) pFont;
	unsigned off;

	/* Handle default font */
    if (!font)  
		font = gr_font;

	off = *s - 32;
	if (off == 0)
		return 0;

	return font->offset[off+1] - font->offset[off];
}

int gr_textEx_en(int x, int y, const char *s, void* pFont)
{
    GGLContext *gl = gr_context;
    GRFont *font = (GRFont*)pFont;
    unsigned off;
    unsigned cwidth;

    /* Handle default font */
    if (!font)  
		font = gr_font;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) 
    {
        off -= 32;
        cwidth = 0;
        if (off < 96) 
        {
            cwidth = font->offset[off+1] - font->offset[off];
			gl->texCoord2i(gl, (font->offset[off]) - x, 0 - y);
			gl->recti(gl, x, y, x + cwidth, y + font->cheight);
			x += cwidth;
        }
    }

    return x;
}

int gr_textExW_en(int x, int y, const char *s, void* pFont, int max_width)
{
    GGLContext *gl = gr_context;
    GRFont *font = (GRFont*)pFont;
    unsigned off;
    unsigned cwidth;

    /* Handle default font */
    if (!font)  
		font = gr_font;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) 
    {
        off -= 32;
        cwidth = 0;
        if (off < 96) 
        {
            cwidth = font->offset[off+1] - font->offset[off];
			if ((x + (int)cwidth) < max_width) {
				gl->texCoord2i(gl, (font->offset[off]) - x, 0 - y);
				gl->recti(gl, x, y, x + cwidth, y + font->cheight);
				x += cwidth;
			} 
			else 
			{
				gl->texCoord2i(gl, (font->offset[off]) - x, 0 - y);
				gl->recti(gl, x, y, max_width, y + font->cheight);
				x = max_width;
				return x;
			}
        }
    }

    return x;
}

int gr_textExWH_en(int x, int y, const char *s, void* pFont, int max_width, int max_height)
{
    GGLContext *gl = gr_context;
    GRFont *font = (GRFont*)pFont;
    unsigned off;
    unsigned cwidth;
	int rect_x, rect_y;

    /* Handle default font */
    if (!font)  
		font = gr_font;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) 
    {
        off -= 32;
        cwidth = 0;
        if (off < 96) 
        {
            cwidth = font->offset[off+1] - font->offset[off];
			if ((x + (int)cwidth) < max_width)
				rect_x = x + cwidth;
			else
				rect_x = max_width;
			if (y + font->cheight < (unsigned int)(max_height))
				rect_y = y + font->cheight;
			else
				rect_y = max_height;

			gl->texCoord2i(gl, (font->offset[off]) - x, 0 - y);
			gl->recti(gl, x, y, rect_x, rect_y);
			x += cwidth;
			if (x > max_width)
				return x;
        }
    }

    return x;
}

int twgr_text_en(int x, int y, const char *s)
{
    GGLContext *gl = gr_context;
    GRFont *font = gr_font;
    unsigned off;
    unsigned cwidth = 0;

    y -= font->ascent;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) 
    {
        off -= 32;
        if (off < 96) 
        {
            cwidth = font->offset[off+1] - font->offset[off];
            gl->texCoord2i(gl, (off * cwidth) - x, 0 - y);
            gl->recti(gl, x, y, x + cwidth, y + font->cheight);
        }
        x += cwidth;
    }

    return x;
}

int gr_getFontDetails_en(void* pFont, unsigned* cheight, unsigned* maxwidth)
{
    GRFont *font = (GRFont*) pFont;

    if (!font)   
		font = gr_font;
    if (!font)   
		return -1;

    if (cheight)    
		*cheight = font->cheight;
    if (maxwidth)
    {
        int pos;
        *maxwidth = 0;
        for (pos = 0; pos < 96; pos++)
        {
            unsigned int width = font->offset[pos+1] - font->offset[pos];
            if (width > *maxwidth)
            {
                *maxwidth = width;
            }
        }
    }
    return 0;
}

void* gr_loadFont_en(const char* fontName)
{
    int fd;
    GRFont *font = 0;
    GGLSurface *ftex;
    unsigned char *bits, *rle;
    unsigned char *in, data;
    unsigned width, height;
    unsigned element;

    fd = open(fontName, O_RDONLY);
    if (fd == -1)
    {
        char tmp[128];

        sprintf(tmp, "/res/fonts/%s.dat", fontName);
        fd = open(tmp, O_RDONLY);
        if (fd == -1)
            return NULL;
    }

    font = calloc(sizeof(*font), 1);
    ftex = &font->texture;

    read(fd, &width, sizeof(unsigned));
    read(fd, &height, sizeof(unsigned));
    read(fd, font->offset, sizeof(unsigned) * 96);
    font->offset[96] = width;

    bits = malloc(width * height);
    memset(bits, 0, width * height);

    unsigned pos = 0;
    while (pos < width * height)
    {
        int bit;

        read(fd, &data, 1);
        for (bit = 0; bit < 8; bit++)
        {
            if (data & (1 << (7-bit)))  bits[pos++] = 255;
            else                        bits[pos++] = 0;

            if (pos == width * height)  break;
        }
    }
    close(fd);

    ftex->version = sizeof(*ftex);
    ftex->width = width;
    ftex->height = height;
    ftex->stride = width;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;
    font->cheight = height;
    font->ascent = height - 2;
    return (void*) font;
}
//===========================================================================================================================//

int gr_measureEx(const char *s, void* pFont)
{
	if(pFont != NULL)
	{
		if(*(char*)pFont == FONT_VER_OFFICIAL)
		{
			return gr_measureEx_en(s, pFont);
		}
	}
	
    GRFontCN * font = (GRFontCN*)pFont;
    int total = 0;
    unsigned pos;
    unsigned off;

    if (!font)   
		font = gr_font_cn2;

    while ((off = *s++))
    {
        if(off < 0x80)
			total += font->ewidth;
		else
		{
			total += font->cwidth;
			s+=2;
		}
    }
    
    return total;  
}

unsigned character_width(const char *s, void* pFont)
{
	if(pFont != NULL)
	{
		if(*(char*)pFont == FONT_VER_OFFICIAL)
		{
			return character_width_en(s, pFont);
		}
	}
	
	GRFontCN *font = (GRFontCN*) pFont;
	unsigned off;

	// Handle default font //
    if (!font)  
		font = gr_font;

	off = *s - 32;
	if (off == 0)
		return 0;

	return font->cwidth;
}

int getGBCharID(unsigned c1, unsigned c2)
{
	if (c1 >= 0xB0 && c1 <=0xF7 && c2>=0xA1 && c2<=0xFE)	
	{
		return (c1-0xB0)*94+c2-0xA1;
	}
	return -1;
}

int getUNICharID(unsigned short unicode, void* pFont)
{
	int i;
	GRFontCN *font = (GRFontCN *)pFont;
	
	for (i = 0; i < font->cn_num; i++) 
	{
		if (unicode == font->fontindex_cn[i])
		return i;
	}

	return -1;
}

int utf8_to_unicode(unsigned c1, unsigned c2, unsigned c3)
{
	unsigned short unicode;
	
	unicode = (c1 & 0x1F) << 12;
	unicode |= (c2 & 0x3F) << 6;
	unicode |= (c3 & 0x3F);
	
	return unicode;
}

static void get_char_en(unsigned int id, void* pFont)
{
    GGLSurface *ftex;
    GRFontCN *font = (GRFontCN *)pFont;
    unsigned char *bits;
    unsigned char *in, data;
    unsigned int bits_len;
    int i, j;

	// Handle default font
    if (!font)  
		font = gr_font_cn2;
		
    ftex = &font->texture;
    ftex->version = sizeof(*ftex);  
	
	if((font->fontver & FONT_VER_UNMONO_EN) == FONT_VER_UNMONO_EN)
		ftex->width = font->width_offset_en[id];
	else
		ftex->width = font->ewidth;
    ftex->height = font->eheight;
    ftex->stride = (font->ewidth+7) & 0xF8;
    
    bits = (unsigned char *)ftex->data;
    bits_len = ((font->ewidth+7) & 0xF8) * font->eheight;

//    in = font_cn2.fontdata.font_en[id].Mask;
	in = font->fontdata + id*bits_len/8;
    for (j=0;j<bits_len/8;j++)
    {
		data = *in++;
        for(i=0;i<8;i++)
        {
			*bits++ = (data & 0x80) ? 0xFF : 0;
			data <<= 1;
		}
    }
}

static void get_char_cn(unsigned int id, void* pFont)
{
    GGLSurface *ftex;
    GRFontCN *font = (GRFontCN *)pFont;
    unsigned char *bits;
    unsigned char *in, data;
    unsigned int bits_len_cn, bits_len_en;
    int i, j;

	// Handle default font
    if (!font)  
		font = gr_font_cn2;
    
    ftex = &font->texture;
    ftex->version = sizeof(*ftex);
    if((font->fontver & FONT_VER_UNMONO_CN) == FONT_VER_UNMONO_CN)
		ftex->width = font->width_offset_cn[id];
    else
		ftex->width = font->cwidth;
    ftex->height = font->cheight;
    ftex->stride = (font->cwidth+7) & 0xF8;
    
    bits = (unsigned char *)ftex->data;  
	bits_len_en = ((font->ewidth+7) & 0xF8) * font->eheight;
	bits_len_cn = ((font->cwidth+7) & 0xF8) * font->cheight;

//    in = font_cn2.fontdata.font_cn[id].Mask;
	in = font->fontdata + CHAR_EN_NUM*bits_len_en/8 + id*bits_len_cn/8;
    for (j=0;j<bits_len_cn/8;j++)
    {
        data = *in++;
        for(i=0;i<8;i++)
        {
			*bits++ = (data & 0x80) ? 0xFF : 0;
			data <<= 1;
		}
    }
}

int gr_textEx(int x, int y, const char *s, void* pFont)
{
	if(pFont != NULL)
	{
		if(*(char*)pFont == FONT_VER_OFFICIAL)
		{
			return gr_textEx_en(x, y, s, pFont);
		}
	}

    GGLContext *gl = gr_context;
    GRFontCN *font = (GRFontCN*)pFont;
    unsigned off;
	unsigned off2;
	unsigned off3;
	int id;
	unsigned short unicode;
	
	/* Handle default font */
    if (!font)  
		font = gr_font_cn2;
		
	y -= font->ascent;
	
	gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
	gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
	gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
	gl->enable(gl, GGL_TEXTURE_2D);	
		
	while((off = *s++)) 
	{
		if (off < 0x80)
		{		    		    
			off -= 32;
			if (off < CHAR_EN_NUM) 
			{
				if ((x + font->ewidth) >= gr_fb_width()) 
					return x;
				get_char_en(off, font);
				gl->bindTexture(gl, &font->texture);
				gl->texCoord2i(gl, 0 - x, 0 - y);
				gl->recti(gl, x, y, x + font->ewidth, y + font->eheight);
			}
			x += font->ewidth;
		}
		else
		{										
			
			if ((off & 0xF0) == 0xE0)
			{
				off2 = *s++;
				off3 = *s++;
				unicode = utf8_to_unicode(off, off2, off3);
				id = getUNICharID(unicode, font);
					
				if (id >= 0) 
				{
					if ((x + font->cwidth) >= gr_fb_width()) 
						return x;				
					get_char_cn(id, font);
					gl->bindTexture(gl, &font->texture);
					gl->texCoord2i(gl, 0 - x, 0 - y);
					gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
					
				} 
				x += font->cwidth;
			} 
			else 
			{
				x += font->ewidth;
			}

		}
    }

    return x;
}

int gr_textExW(int x, int y, const char *s, void* pFont, int max_width)
{
	if(pFont != NULL)
	{
		if(*(char*)pFont == FONT_VER_OFFICIAL)
		{
			return gr_textExW_en( x, y, s, pFont, max_width);
		}
	}
	
    GGLContext *gl = gr_context;
    GRFontCN *font = (GRFontCN*)pFont;
    unsigned off;
	unsigned off2;
	unsigned off3;
	int id;
	unsigned short unicode;

    /* Handle default font */
    if (!font)  
		font = gr_font_cn2;
		
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) 
    {
        if(off<0x80)
        {
			off -= 32;
			if (off < CHAR_EN_NUM) 
			{
				get_char_en(off, font);
				gl->bindTexture(gl, &font->texture);
				gl->texCoord2i(gl, 0 - x, 0 - y);
				if ((x + font->ewidth) < max_width) 
				{	
					gl->recti(gl, x, y, x + font->ewidth, y + font->eheight);
					x += font->ewidth;
				} 
				else 
				{
					gl->recti(gl, x, y, max_width, y + font->cheight);
					x = max_width;
				}
			}
		}
		else
		{
			if ((off & 0xF0) == 0xE0)
			{
				off2 = *s++;
				off3 = *s++;
				unicode = utf8_to_unicode(off, off2, off3);
				id = getUNICharID(unicode, font);

				if (id >= 0) 
				{
					get_char_cn(id, font);
					gl->bindTexture(gl, &font->texture);
					gl->texCoord2i(gl, 0 - x, 0 - y);
					if ((x + font->cwidth) < max_width) 
					{							
						gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
						x += font->cwidth;
					}
					else
					{
						gl->recti(gl, x, y, max_width, y + font->cheight);
						x = max_width;
					}
				} 
				else 
				{
				    x += font->cwidth;
				}
			} 
			else 
			{
			    x += font->ewidth;
			}

		}
    }

    return x;
}

int gr_textExWH(int x, int y, const char *s, void* pFont, int max_width, int max_height)
{
	if(pFont != NULL)
	{
		if(*(char*)pFont == FONT_VER_OFFICIAL)
		{
			return gr_textExWH_en( x, y, s, pFont, max_width, max_height);
		}
	}

    GGLContext *gl = gr_context;
    GRFontCN *font = (GRFontCN*)pFont;
    unsigned off;
	unsigned off2;
	unsigned off3;
	int id;
	unsigned short unicode;
	
	int rect_x, rect_y;

    /* Handle default font */
    if (!font)  
		font = gr_font_cn2;

    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) 
    {
        if(off < 0x80)
        {
			off -= 32;
			if (off < CHAR_EN_NUM) 
			{
				if ((x + font->ewidth) < max_width)
					rect_x = x + font->ewidth;
				else
					rect_x = max_width;
				if ((y + font->eheight) < max_height)
					rect_y = y + font->eheight;
				else
					rect_y = max_height;

				get_char_en(off, font);
				gl->bindTexture(gl, &font->texture);
				gl->texCoord2i(gl, 0 - x, 0 - y);
				gl->recti(gl, x, y, rect_x, rect_y);
	
				x += font->ewidth;
				if (x > max_width)
					return x;
			}
		}
		else
		{
			if ((off & 0xF0) == 0xE0)
			{
				off2 = *s++;
				off3 = *s++;
				unicode = utf8_to_unicode(off, off2, off3);
				id = getUNICharID(unicode, font);
				
				if (id >= 0) 
				{
			
					if ((x + font->cwidth) < max_width)
						rect_x = x + font->cwidth;
					else
						rect_x = max_width;
					if ((y + font->cheight) < max_height)
						rect_y = y + font->cheight;
					else
						rect_y = max_height;
						
					get_char_cn(id, font);
					gl->bindTexture(gl, &font->texture);
					gl->texCoord2i(gl, 0 - x, 0 - y);
					gl->recti(gl, x, y, rect_x, rect_y);
					
					x += font->cwidth;
					if (x > max_width)
						return x;
				}
			}
		}
    }

    return x;
}

int twgr_text(int x, int y, const char *s)
{
    GGLContext *gl = gr_context;
    GRFontCN *font = gr_font_cn;
    unsigned off;
	unsigned off2;	
	unsigned off3;	
	int id;	
	unsigned short unicode;

    y -= font->ascent;

    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++))
    {
		if(off<0x80)
		{
			off -= 32;
			if (off < CHAR_EN_NUM)
			{
				if ((x + font->ewidth) >= gr_fb_width())
					return x;
					
				get_char_en(off, font);
				gl->bindTexture(gl, &font->texture);
				gl->texCoord2i(gl, 0 - x, 0 - y);
				gl->recti(gl, x, y, x + font->ewidth, y + font->eheight);
			}
			x += font->ewidth;
		}
		else
		{
			if ((off & 0xF0) == 0xE0)
			{
				off2 = *s++;
				off3 = *s++;
				unicode = utf8_to_unicode(off, off2, off3);
				id = getUNICharID(unicode, font);

				if (id >= 0)
				{
					if ((x + font->cwidth) >= gr_fb_width())
						return x;
						
					get_char_cn(id, font);
					gl->bindTexture(gl, &font->texture);
					gl->texCoord2i(gl, 0 - x, 0 - y);
					gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
					x += font->cwidth;
				}
				else
				{
					x += font->cwidth;
				}
			}
			else
			{
				x += font->ewidth;
			}
		}
	}

    return x;
}

void gr_fill(int x, int y, int w, int h)
{
    GGLContext *gl = gr_context;
    gl->disable(gl, GGL_TEXTURE_2D);
    gl->recti(gl, x, y, x + w, y + h);
}

void gr_blit(gr_surface source, int sx, int sy, int w, int h, int dx, int dy) 
{
    if (gr_context == NULL) 
    {
        return;
    }

    GGLContext *gl = gr_context;
    gl->bindTexture(gl, (GGLSurface*) source);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);
    gl->texCoord2i(gl, sx - dx, sy - dy);
    gl->recti(gl, dx, dy, dx + w, dy + h);
}

unsigned int gr_get_width(gr_surface surface) 
{
    if (surface == NULL) 
    {
        return 0;
    }
    return ((GGLSurface*) surface)->width;
}

unsigned int gr_get_height(gr_surface surface) 
{
    if (surface == NULL) 
    {
        return 0;
    }
    return ((GGLSurface*) surface)->height;
}

void* gr_loadFont(const char* fontName)
{
    int fd;
    GRFontCN *font = 0;   
	GRFont *font_en = 0;
    GGLSurface *ftex;
    unsigned char *bits, *fontdata;
    CHAR_LEN_TYPE ewidth, eheight, cwidth, cheight;
    unsigned bits_len_cn, bits_len_en, total_len;
    unsigned en_num = 0, cn_num = 0, index_type = 0;
    unsigned char *fontindex_en = NULL;
    unsigned short *fontindex_cn = NULL;
    unsigned fontindex_tmp;
    int unicode;
    unsigned i;
    unsigned char *width_offset_en, *width_offset_cn;

    fd = open(fontName, O_RDONLY);
    if (fd == -1)
    {
        char tmp[256];

        sprintf(tmp, "/res/fonts/%s.dat", fontName);
        fd = open(tmp, O_RDONLY);
        if (fd == -1)
            return NULL;
    }
	
	font = calloc(sizeof(*font), 1);
	ftex = &font->texture;
		
    read(fd, &en_num, sizeof(unsigned));
    if((en_num > 0xFF) && (en_num < 0xFFFF))
    {	
		close(fd);
		font_en = (GRFont *)gr_loadFont_en(fontName);
		font_en->fontver = FONT_VER_OFFICIAL;
		
		return (void*) font_en;		
	}
    else 
    {	
		if(en_num > 0xFFFF)
		{
			font->fontver = FONT_VER_NO_INDEX;
			
			ewidth  = en_num & 0xFF;
			eheight = (en_num>>8) & 0xFF;
			cwidth  = (en_num>>16) & 0xFF;
			cheight = (en_num>>24) & 0xFF;
			
			en_num = CHAR_EN_NUM;
			cn_num = CHAR_CN_NUM;
		}
		else if(en_num < 0xFF)
		{			
			read(fd, &cn_num, sizeof(unsigned));
			read(fd, &index_type, sizeof(unsigned));
			read(fd, &ewidth, sizeof(CHAR_LEN_TYPE));
			read(fd, &eheight, sizeof(CHAR_LEN_TYPE));
			read(fd, &cwidth, sizeof(CHAR_LEN_TYPE));
			read(fd, &cheight, sizeof(CHAR_LEN_TYPE));
			
			font->fontver = index_type | FONT_VER_HAVE_INDEX;
		}
	}

	bits_len_en = ((ewidth+7) & 0xF8) * eheight;
	bits_len_cn = ((cwidth+7) & 0xF8) * cheight;
    bits = malloc(bits_len_cn);
    memset(bits, 0, bits_len_cn);
    total_len = (en_num*bits_len_en + cn_num*bits_len_cn)/8;
    fontdata = malloc(total_len);
    read(fd, fontdata, total_len);
    
    if(font->fontver & FONT_VER_HAVE_INDEX)
    {
		if(en_num == CHAR_EN_NUM)
		{
			fontindex_en = NULL;
		}
		else
		{
			fontindex_en = malloc(en_num);
			read(fd, fontindex_en, en_num);
		}
		
		if(cn_num == CHAR_CN_NUM)
		{
			fontindex_cn = unicodemap;
			index_type = FONT_INDEX_TYPE_UTF8;
		}
		else
		{
			fontindex_cn = (unsigned short*)malloc(cn_num*2);
			
			if(index_type & FONT_INDEX_TYPE_UTF8)
			{
				for(i=0; i<cn_num; i++)
				{
					read(fd, &fontindex_tmp, 3);
					unicode = utf8_to_unicode(fontindex_tmp&0xFF, (fontindex_tmp>>8)&0xFF, (fontindex_tmp>>16)&0xFF);
					if(unicode >= 0)
						fontindex_cn[i] = unicode;
					else
						fontindex_cn[i] = unicodemap[0];
					
				}
			}
			else if((index_type & FONT_INDEX_TYPE_UTF8) == FONT_INDEX_TYPE_GBK)
			{
				for(i=0; i<cn_num; i++)
				{
					read(fd, &fontindex_tmp, 2);
					unicode = getGBCharID(fontindex_tmp&0xFF, (fontindex_tmp>>8)&0xFF);
					if(unicode >= 0)
						fontindex_cn[i] = unicodemap[unicode];
					else
						fontindex_cn[i] = unicodemap[0];
				}
			}			
		}	
		
		if(font->fontver & FONT_VER_UNMONO_EN)
		{
			width_offset_en = malloc(en_num);
			read(fd, width_offset_en, en_num);
		}
		else
		{
			width_offset_en = NULL;
		}
		if(font->fontver & FONT_VER_UNMONO_CN)
		{
			width_offset_cn = malloc(cn_num);
			read(fd, width_offset_cn, cn_num);
		}
		else
		{
			width_offset_cn = NULL;
		}
	}
	else
	{
		index_type = FONT_INDEX_TYPE_UTF8;
		fontindex_en = NULL;
		fontindex_cn = unicodemap;
	}	
	
    close(fd);

    ftex->version = sizeof(*ftex);
    ftex->width = cwidth;
    ftex->height = cheight;
    ftex->stride = (cwidth+7) & 0xF8;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;
    
    font->ascent = cheight/4 - cheight/8;
    font->en_num = en_num;
    font->cn_num = cn_num;
    font->ewidth = ewidth;
    font->eheight = eheight;
    font->cwidth = cwidth;
    font->cheight = cheight;
    font->fontdata = fontdata;
    font->fontindex_en = fontindex_en;
    font->fontindex_cn = fontindex_cn;
    font->width_offset_en = width_offset_en;
    font->width_offset_cn = width_offset_cn;
	
    LOGI("font = %s\n", fontName);
    LOGI("index_type    = %s\n", (index_type & FONT_INDEX_TYPE_UTF8)?"UTF-8":"GBK");
    LOGI("font->en_num  = %d\n", font->en_num);
    LOGI("font->cn_num  = %d\n", font->cn_num);   
    LOGI("font->ewidth  = %d\n", font->ewidth);
    LOGI("font->eheight = %d\n", font->eheight);
    LOGI("font->cwidth  = %d\n", font->cwidth);
    LOGI("font->cheight = %d\n", font->cheight);
      
    return (void*) font;
}

int gr_getFontDetails(void* pFont, unsigned* cheight, unsigned* maxwidth)
{
	if(pFont != NULL)
	{
		if(*(char*)pFont == FONT_VER_OFFICIAL)
		{
			return gr_getFontDetails_en(pFont, cheight, maxwidth);
		}
	}

	GRFontCN *font = (GRFontCN *) pFont;
    if (!font)   
		font = gr_font_cn2;

    if (cheight)    
		*cheight = font->cheight;
    if (maxwidth)
		*maxwidth = font->cwidth;
    
    return 0;
}

static void gr_init_font(void)
{
    int fontRes;
    GGLSurface *ftex;
    unsigned char *bits, *rle;
    unsigned char *in, data;
    unsigned width, height;
    unsigned element;

    gr_font = calloc(sizeof(*gr_font), 1);
    ftex = &gr_font->texture;

    width = font.width;
    height = font.height;

    bits = malloc(width * height);
    rle = bits;

    in = font.rundata;
    while((data = *in++))
    {
        memset(rle, (data & 0x80) ? 0xFF : 0, data & 0x7F);
        rle += (data & 0x7F);
    }
    for (element = 0; element < 97; element++)
    {
        gr_font->offset[element] = (element * font.cwidth);
    }

    ftex->version = sizeof(*ftex);
    ftex->width = width;
    ftex->height = height;
    ftex->stride = width;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;
    gr_font->cheight = height;
    gr_font->ascent = height - 2;
}

static void gr_init_font_cn(void)
{
    GGLSurface *ftex;
    unsigned char *bits;
    unsigned int bits_len;
	
    gr_font_cn = calloc(sizeof(*gr_font_cn), 1);
    ftex = &gr_font_cn->texture;

	bits_len = ((font_cn.cwidth+7) & 0xF8) * font_cn.cheight;
    bits = malloc(bits_len);

    ftex->version = sizeof(*ftex);
    ftex->width = font_cn.cwidth;
    ftex->height = font_cn.cheight;
    ftex->stride = (font_cn.cwidth+7) & 0xF8;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;
    
    gr_font_cn->ascent = font_cn.cheight/4 - font_cn.cheight/8;
    gr_font_cn->en_num = CHAR_EN_NUM;
    gr_font_cn->cn_num = CHAR_CN_NUM;
    gr_font_cn->ewidth = font_cn.ewidth;
    gr_font_cn->eheight = font_cn.eheight;
    gr_font_cn->cwidth = font_cn.cwidth;
    gr_font_cn->cheight = font_cn.cheight;
    gr_font_cn->fontdata = (unsigned char*)&font_cn.fontdata;
    gr_font_cn->fontindex_en = NULL;
    gr_font_cn->fontindex_cn = unicodemap;
}

static void gr_init_font_cn2(void)
{
    GGLSurface *ftex;
    unsigned char *bits;
    unsigned int bits_len;
	
    gr_font_cn2 = calloc(sizeof(*gr_font_cn2), 1);
    ftex = &gr_font_cn2->texture;

	bits_len = ((font_cn2.cwidth+7) & 0xF8) * font_cn2.cheight;
    bits = malloc(bits_len);

    ftex->version = sizeof(*ftex);
    ftex->width = font_cn2.cwidth;
    ftex->height = font_cn2.cheight;
    ftex->stride = (font_cn2.cwidth+7) & 0xF8;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;
    
    gr_font_cn2->ascent = font_cn2.cheight/4 - font_cn2.cheight/8;
    gr_font_cn2->en_num = CHAR_EN_NUM;
    gr_font_cn2->cn_num = CHAR_CN_NUM;    
    gr_font_cn2->ewidth = font_cn2.ewidth;
    gr_font_cn2->eheight = font_cn2.eheight;
    gr_font_cn2->cwidth = font_cn2.cwidth;
    gr_font_cn2->cheight = font_cn2.cheight;
    gr_font_cn2->fontdata = (unsigned char*)&font_cn2.fontdata;
    gr_font_cn2->fontindex_en = NULL;
    gr_font_cn2->fontindex_cn = unicodemap;
}

int gr_init(void)
{
    gglInit(&gr_context);
    GGLContext *gl = gr_context;

    gr_init_font();
    gr_init_font_cn();
    gr_init_font_cn2();
    
    gr_vt_fd = open("/dev/tty0", O_RDWR | O_SYNC);
    if (gr_vt_fd < 0) 
    {
        // This is non-fatal; post-Cupcake kernels don't have tty0.
    } 
    else if (ioctl(gr_vt_fd, KDSETMODE, (void*) KD_GRAPHICS)) 
    {
        // However, if we do open tty0, we expect the ioctl to work.
        perror("failed KDSETMODE to KD_GRAPHICS on tty0");
        gr_exit();
        return -1;
    }

    gr_fb_fd = get_framebuffer(gr_framebuffer);
    if (gr_fb_fd < 0) 
    {
        perror("Unable to get framebuffer.\n");
        gr_exit();
        return -1;
    }
    
    get_memory_surface(&gr_mem_surface);

    fprintf(stderr, "framebuffer: fd %d (%d x %d)\n", gr_fb_fd, gr_framebuffer[0].width, gr_framebuffer[0].height);

    /* start with 0 as front (displayed) and 1 as back (drawing) */
    gr_active_fb = 0;
    set_active_framebuffer(0);
    gl->colorBuffer(gl, &gr_mem_surface);

    gl->activeTexture(gl, 0);
    gl->enable(gl, GGL_BLEND);
    gl->blendFunc(gl, GGL_SRC_ALPHA, GGL_ONE_MINUS_SRC_ALPHA);

//    gr_fb_blank(true);
//    gr_fb_blank(false);

    return 0;
}

void gr_exit(void)
{
    close(gr_fb_fd);
    gr_fb_fd = -1;

    free(gr_mem_surface.data);

    ioctl(gr_vt_fd, KDSETMODE, (void*) KD_TEXT);
    close(gr_vt_fd);
    gr_vt_fd = -1;
}

int gr_fb_width(void)
{
    return gr_framebuffer[0].width;
}

int gr_fb_height(void)
{
    return gr_framebuffer[0].height;
}

gr_pixel *gr_fb_data(void)
{
    return (unsigned short *) gr_mem_surface.data;
}

void gr_fb_blank(int blank)
{
    int ret;

    ret = ioctl(gr_fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
    if (ret < 0)
        perror("ioctl(): blank");
}

int gr_get_surface(gr_surface* surface)
{
    GGLSurface* ms = malloc(sizeof(GGLSurface));
    
    if (!ms)    
		return -1;

    // Allocate the data
    get_memory_surface(ms);
    // Now, copy the data
//    memcpy(ms->data, gr_mem_surface.data, vi.xres * vi.yres * vi.bits_per_pixel / 8);
    *surface = (gr_surface*) ms;
    
    return 0;
}

int gr_free_surface(gr_surface surface)
{
    if (!surface)
        return -1;

    GGLSurface* ms = (GGLSurface*) surface;
    free(ms->data);
    free(ms);
    
    return 0;
}

void gr_write_frame_to_file(int fd)
{
    write(fd, gr_mem_surface.data, vi.xres * vi.yres * vi.bits_per_pixel / 8);
}
