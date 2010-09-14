#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>
#include <stdint.h>

#include "drawtk.h"
#include "fonttex.h"
#include "texmanager.h"

#define SIZE		(64)
#define FONT_MXLVL	0
#define CHHEIGHT	SIZE
#define CHWIDTH		SIZE
#define MAPWIDTH	(16*CHHEIGHT)
#define MAPHEIGHT	(16*CHWIDTH)
#define MAX(arg1, arg2)	((arg1) > (arg2) ? (arg1) : (arg2))



static 
void get_max_size(FT_Face face, unsigned int *h, unsigned int *w)
{
	unsigned int xmax, ymax, i, x, y;
	FT_Glyph glyph;
	FT_BBox actbox;

	xmax = ymax = 0;

	for (i=32; i<256; i++) {
		FT_Load_Glyph(face, FT_Get_Char_Index(face, i),
				FT_LOAD_DEFAULT);
		FT_Get_Glyph(face->glyph, &glyph);
		FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_TRUNCATE, &actbox);
		FT_Done_Glyph(glyph);

		x = actbox.xMax - actbox.xMin;
		y = actbox.yMax - actbox.yMin;
		xmax = (xmax > x) ? xmax : x;
		ymax = (ymax > y) ? ymax : y;
	}

	*w = xmax;
	*h = ymax;
}


static
void render_character(FT_Face face, uint8_t* bits, unsigned ic,
                      struct dtk_font* font, unsigned int ppem, unsigned int lvl)
{
	int j, imin, w, jmax, h, cm_w, cm_h, ch_h, ch_w;
	FT_Bitmap* bitmap;
	FT_BitmapGlyph bmglyph;
	FT_Glyph glyph;
	FT_BBox bbox;
	uint8_t* bmbits;
	struct character* ch;
	unsigned int sc = 0x01 << lvl;
	unsigned int currppem = ppem / (float)sc ;
	cm_w = MAPWIDTH / sc;
	cm_h = MAPHEIGHT / sc;
	ch_w = CHWIDTH / sc;
	ch_h = CHHEIGHT / sc;

	// Convert the glyph to a bitmap.
	FT_Set_Pixel_Sizes(face, currppem, currppem);
	FT_Load_Glyph(face, FT_Get_Char_Index(face, ic), FT_LOAD_DEFAULT);
	FT_Get_Glyph(face->glyph, &glyph);
	FT_Glyph_To_Bitmap( &glyph, ft_render_mode_normal, 0, 1 );
	bmglyph = (FT_BitmapGlyph)glyph;
	bitmap=&(bmglyph->bitmap);

	
	FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_UNSCALED, &bbox);
	w = bitmap->width;
	h = bitmap->rows;
	imin = ((ic-32)%16)* ch_w + (ch_w-w)/2;
	jmax = h + ((ic-32)/16)*ch_h + (ch_h-h)/2;

	if (lvl == 0) {
		ch = &(font->ch[ic-32]);
		ch->txmin = ((float)(imin))/MAPWIDTH;
		ch->txmax = ((float)(imin+w))/MAPWIDTH;
		ch->tymin = ((float)(jmax-h))/MAPHEIGHT;
		ch->tymax = ((float)(jmax))/MAPHEIGHT;
		ch->xmin = bbox.xMin/(64.0f*CHWIDTH);
		ch->xmax = bbox.xMax/(64.0f*CHWIDTH);
		ch->ymin = bbox.yMin/(64.0f*CHHEIGHT);
		ch->ymax = bbox.yMax/(64.0f*CHHEIGHT);
		ch->advance = face->glyph->advance.x/((float)ppem*64.0f);
	}

	bmbits = bitmap->buffer;
	for (j=0; j<h; j++) 
		memcpy(bits + imin+(jmax-j)*cm_w, bmbits + w*j, w);

	FT_Done_Glyph(glyph);
}


static 
void load_glyph(void** bits, struct dtk_font* font, const char* fname,
                unsigned int mxlvl)
{
	int i;
	FT_Face face;
	FT_Library library;
	unsigned int h, w, max_size, lvl, maxsc, ppem;
	maxsc= (0x01<<mxlvl);

	FT_Init_FreeType( &library );

	FT_New_Face(library, fname, 0, &face);
	FT_Set_Pixel_Sizes(face, CHWIDTH, CHHEIGHT);
	//max_size = MAX(face->max_advance_width, face->max_advance_height);
	get_max_size(face, &h, &w);
	max_size = MAX(h, w);
	ppem = maxsc * ((SIZE * SIZE) / (max_size*maxsc));
	//FT_Set_Pixel_Sizes(face, (unsigned int)ppem, (unsigned int)ppem);

	for (i=32; i<256; i++) {
		for (lvl=0; lvl<=mxlvl; lvl++) 
			render_character(face, bits[lvl], i, font, ppem, lvl);
	}
	
	FT_Done_Face(face);
	FT_Done_FreeType(library);
}


static
void font_destroy(struct dtk_texture* tex)
{
	free(tex->aux);
	free(tex);
}


LOCAL_FN
int dtk_char_pos(const struct dtk_font* restrict font, unsigned char c,
                 float* restrict vert, float* restrict texcoords,
		 unsigned int * restrict ind,
		 unsigned int currind, float * restrict org)
{
	const struct character* restrict ch;
	float pos = *org;
	if (c < 32) {
		memset(vert, 0, 4*sizeof(*vert));
		memset(texcoords, 0, 4*sizeof(*texcoords));
		memset(ind, 0, 6*sizeof(*ind));
		return 6;
	}

	ch = &(font->ch[c-32]);

	vert[0] = ch->xmin + pos;
	vert[1] = ch->ymin;
	texcoords[0] = ch->txmin;
	texcoords[1] = ch->tymin;

	vert[2] = ch->xmin + pos;
	vert[3] = ch->ymax;
	texcoords[2] = ch->txmin;
	texcoords[3] = ch->tymax;

	vert[4] = ch->xmax + pos;
	vert[5] = ch->ymin;
	texcoords[4] = ch->txmax;
	texcoords[5] = ch->tymin;

	vert[6] = ch->xmax + pos;
	vert[7] = ch->ymax;
	texcoords[6] = ch->txmax;
	texcoords[7] = ch->tymax;

	ind[0] = currind + 0;
	ind[1] = currind + 1;
	ind[2] = currind + 2;
	ind[3] = currind + 2;
	ind[4] = currind + 1;
	ind[5] = currind + 3;

	*org += ch->advance;
	return 6;
}


LOCAL_FN
struct dtk_texture* dtk_get_fonttex(const struct dtk_font* font)
{
	return font->tex;
}


API_EXPORTED
struct dtk_font* dtk_load_font(const char* fontname)
{
	int fail = 0;
	struct dtk_texture *tex = NULL;
	struct dtk_font* font = NULL;
	char stringid[256];
	
	// Get new/precreated texture
	snprintf(stringid, sizeof(stringid), "FONT:%s", fontname);
	if ((tex = get_texture(stringid)) == NULL)
		return NULL;

	// Load the font bitmap
	pthread_mutex_lock(&(tex->lock));
	if (!tex->isinit) {
		if ( ((font = malloc(sizeof(struct dtk_font))) == NULL)
		    || alloc_image_data(tex, MAPWIDTH, MAPHEIGHT,
		                             FONT_MXLVL, 8) )
			fail = 1;
		tex->aux = font;
		font->tex = tex;
		load_glyph(tex->data, tex->aux, fontname, FONT_MXLVL);
		tex->destroyfn = font_destroy;
		tex->isinit = 1;

		tex->fmt = GL_ALPHA;
		tex->type = GL_UNSIGNED_BYTE;
		tex->intfmt = GL_ALPHA;
	}
	pthread_mutex_unlock(&(tex->lock));

	if (fail) {
		free(font);
		rem_texture(tex);
		return NULL;
	}
	return font;
}


API_EXPORTED
void dtk_destroy_font(struct dtk_font* font)
{
	rem_texture(font->tex);
}