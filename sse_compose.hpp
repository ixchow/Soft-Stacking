#include <xmmintrin.h>

inline void sse_compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count);
inline void generic_compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count);

inline void generic_multiply(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count) {
	uint32_t const *bg_at = bg_tile;
	uint32_t const *fg_at = fg_tile;
	uint32_t *into = into_tile;
	for (unsigned int i = 0; i < count; ++i) {
		const uint32_t bg = *bg_at;
		uint32_t fg = *fg_at;

		uint32_t alpha = fg >> 24;

		{ //multiply:
			uint32_t tb = (((bg >> 16) & 0xff) * ((fg >> 16) & 0xff)) / 255;
			uint32_t tg = (((bg >> 8) & 0xff) * ((fg >> 8) & 0xff)) / 255;
			uint32_t tr = ((bg & 0xff) * (fg & 0xff)) / 255;
			fg = (tb << 16) | (tg << 8) | (tr);
		}

		uint32_t r = ((bg & 0x000000ff) * (255 - alpha) + (fg & 0x000000ff) * alpha) / 255;
		uint32_t g = ((bg & 0x0000ff00) * (255 - alpha) + (fg & 0x0000ff00) * alpha) / 255;
		uint32_t b = ((bg & 0x00ff0000) * (255 - alpha) + (fg & 0x00ff0000) * alpha) / 255;
		*into = 0xff000000 | (b & 0x00ff0000) | (g & 0x0000ff00) | (r & 0x000000ff);

		++bg_at;
		++fg_at;
		++into;
	}
}

inline __m128i multiply_2px(__m128i fg, __m128i bg) {
	//replicate alpha:
	__m128i alpha = _mm_shufflelo_epi16(fg, _MM_SHUFFLE(3,3,3,3));
	alpha = _mm_shufflehi_epi16(alpha, _MM_SHUFFLE(3,3,3,3));

	//bg << 8:
	__m128i bg_shift_8 = _mm_slli_epi16(bg, 8);

	//Multiply:
	fg = _mm_mullo_epi16(fg, bg);
	fg = _mm_srli_epi16(fg, 8);

	//subtract bg from everything:
	fg = _mm_sub_epi16(fg, bg);
	bg_shift_8 = _mm_sub_epi16(bg_shift_8, bg);

	//multiply by alpha:
	__m128i out = _mm_add_epi16(bg_shift_8, _mm_mullo_epi16(fg, alpha));

	//hack-y /255 -> (r2i + 1 + (r2i >> 8)) >> 8;
	__m128i ones = _mm_set1_epi16(1);

	__m128i shifted = _mm_srli_epi16(out, 8);

	out = _mm_add_epi16(out, _mm_add_epi16(ones, shifted));

	out = _mm_srli_epi16(out, 8);

	return out;
}


inline void sse_multiply(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count) {

	//pre-roll -- count into_tile should be 16-byte aligned:
	unsigned int pre = 4 - ((into_tile - (uint32_t*)(NULL)) % 4);
	if (pre != 4) {
		generic_multiply(bg_tile, fg_tile, into_tile, pre);
		bg_tile += pre;
		fg_tile += pre;
		into_tile += pre;
		count -= pre;
	}

	assert((uint64_t(into_tile) & 0xf) == 0);

	unsigned int quads = count / 4;

	const __m128i *bg_at = reinterpret_cast< const __m128i * >(bg_tile);
	const __m128i *fg_at = reinterpret_cast< const __m128i * >(fg_tile);
	__m128i *into = reinterpret_cast< __m128i * >(into_tile);
	for (unsigned int i = 0; i < quads; ++i) {
		/* Scary yet true motivating example:
			uint16_t r1 = ((255 - alpha) * a + alpha * b) / 255;
			uint16_t r2i = ((a << 8) - a + (b - a) * alpha);
			uint16_t r2 = (r2i + 1 + (r2i >> 8)) >> 8;
			assert(r1 == r2);
		*/

		//Algorithm might go something like this:
		__m128i bg16 = _mm_loadu_si128(bg_at);
		__m128i fg16 = _mm_loadu_si128(fg_at);

		//do by halfs:
		
		__m128i zero = _mm_setzero_si128();
		__m128i bglo = _mm_unpacklo_epi8(bg16, zero);
		__m128i bghi = _mm_unpackhi_epi8(bg16, zero);
		__m128i fglo = _mm_unpacklo_epi8(fg16, zero);
		__m128i fghi = _mm_unpackhi_epi8(fg16, zero);
		

		__m128i outlo, outhi;
		outlo = multiply_2px(fglo, bglo);
		outhi = multiply_2px(fghi, bghi);

		__m128i into_temp = _mm_packus_epi16(outlo, outhi);

		__m128i alpha_mask = _mm_set1_epi32(0xff000000);
		into_temp = _mm_or_si128(alpha_mask, into_temp);

		//NOTE: stream is a bad idea here as the common case involves writing
		// to a tile we immediately read again for the next layer.
		//_mm_stream_si128(into, into_temp);
		_mm_store_si128(into, into_temp);

		++bg_at;
		++fg_at;
		++into;
	}

	//post-roll:
	//
	fg_tile += 4 * quads;
	bg_tile += 4 * quads;
	into_tile += 4 * quads;
	count -= 4 * quads;
	assert(count < 4);

	generic_multiply(bg_tile, fg_tile, into_tile, count);

}

inline void generic_compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count) {
	uint32_t const *bg_at = bg_tile;
	uint32_t const *fg_at = fg_tile;
	uint32_t *into = into_tile;
	for (unsigned int i = 0; i < count; ++i) {
		uint32_t bg = *bg_at;
		uint32_t fg = *fg_at;

		uint32_t alpha = fg >> 24;
		uint32_t r = ((bg & 0x000000ff) * (255 - alpha) + (fg & 0x000000ff) * alpha) / 255;
		uint32_t g = ((bg & 0x0000ff00) * (255 - alpha) + (fg & 0x0000ff00) * alpha) / 255;
		uint32_t b = ((bg & 0x00ff0000) * (255 - alpha) + (fg & 0x00ff0000) * alpha) / 255;
		*into = 0xff000000 | (b & 0x00ff0000) | (g & 0x0000ff00) | (r & 0x000000ff);

		++bg_at;
		++fg_at;
		++into;
	}
}


inline __m128i over_2px(__m128i fg, __m128i bg) {
	//replicate alpha:
	__m128i alpha = _mm_shufflelo_epi16(fg, _MM_SHUFFLE(3,3,3,3));
	alpha = _mm_shufflehi_epi16(alpha, _MM_SHUFFLE(3,3,3,3));

	//bg << 8:
	__m128i bg_shift_8 = _mm_slli_epi16(bg, 8);

	//subtract bg from everything:
	fg = _mm_sub_epi16(fg, bg);
	bg_shift_8 = _mm_sub_epi16(bg_shift_8, bg);

	//multiply by alpha:
	__m128i out = _mm_add_epi16(bg_shift_8, _mm_mullo_epi16(fg, alpha));

	//hack-y /255 -> (r2i + 1 + (r2i >> 8)) >> 8;
	__m128i ones = _mm_set1_epi16(1);

	__m128i shifted = _mm_srli_epi16(out, 8);

	out = _mm_add_epi16(out, _mm_add_epi16(ones, shifted));

	out = _mm_srli_epi16(out, 8);

	return out;
}


inline void sse_compose(uint32_t const *bg_tile, uint32_t const *fg_tile, uint32_t *into_tile, uint32_t count) {

	//pre-roll -- count into_tile should be 16-byte aligned:
	unsigned int pre = 4 - ((into_tile - (uint32_t*)(NULL)) % 4);
	if (pre != 4) {
		generic_compose(bg_tile, fg_tile, into_tile, pre);
		bg_tile += pre;
		fg_tile += pre;
		into_tile += pre;
		count -= pre;
	}

	assert((uint64_t(into_tile) & 0xf) == 0);

	unsigned int quads = count / 4;

	const __m128i *bg_at = reinterpret_cast< const __m128i * >(bg_tile);
	const __m128i *fg_at = reinterpret_cast< const __m128i * >(fg_tile);
	__m128i *into = reinterpret_cast< __m128i * >(into_tile);
	for (unsigned int i = 0; i < quads; ++i) {
		/* Scary yet true motivating example:
			uint16_t r1 = ((255 - alpha) * a + alpha * b) / 255;
			uint16_t r2i = ((a << 8) - a + (b - a) * alpha);
			uint16_t r2 = (r2i + 1 + (r2i >> 8)) >> 8;
			assert(r1 == r2);
		*/

		//Algorithm might go something like this:
		__m128i bg16 = _mm_loadu_si128(bg_at);
		__m128i fg16 = _mm_loadu_si128(fg_at);

		//do by halfs:
		
		__m128i zero = _mm_setzero_si128();
		__m128i bglo = _mm_unpacklo_epi8(bg16, zero);
		__m128i bghi = _mm_unpackhi_epi8(bg16, zero);
		__m128i fglo = _mm_unpacklo_epi8(fg16, zero);
		__m128i fghi = _mm_unpackhi_epi8(fg16, zero);
		

		__m128i outlo, outhi;
		outlo = over_2px(fglo, bglo);
		outhi = over_2px(fghi, bghi);

		__m128i into_temp = _mm_packus_epi16(outlo, outhi);

		__m128i alpha_mask = _mm_set1_epi32(0xff000000);
		into_temp = _mm_or_si128(alpha_mask, into_temp);

		//NOTE: stream is a bad idea here as the common case involves writing
		// to a tile we immediately read again for the next layer.
		//_mm_stream_si128(into, into_temp);
		_mm_store_si128(into, into_temp);

		++bg_at;
		++fg_at;
		++into;
	}

	//post-roll:
	//
	fg_tile += 4 * quads;
	bg_tile += 4 * quads;
	into_tile += 4 * quads;
	count -= 4 * quads;
	assert(count < 4);

	generic_compose(bg_tile, fg_tile, into_tile, count);

}
