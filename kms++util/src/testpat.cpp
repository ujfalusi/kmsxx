
//#define DRAW_PERF_PRINT

#include <cstring>
#include <cassert>

#include <kms++/kms++.h>
#include <kms++util/kms++util.h>

using namespace std;

namespace kms
{

static RGB get_test_pattern_pixel(IFramebuffer& fb, unsigned x, unsigned y)
{
	const unsigned w = fb.width();
	const unsigned h = fb.height();

	const unsigned mw = 20;

	const unsigned xm1 = mw;
	const unsigned xm2 = w - mw - 1;
	const unsigned ym1 = mw;
	const unsigned ym2 = h - mw - 1;

	// white margin lines
	if (x == xm1 || x == xm2 || y == ym1 || y == ym2)
		return RGB(255, 255, 255);
	// white box in top left corner
	else if (x < xm1 && y < ym1)
		return RGB(255, 255, 255);
	// white box outlines to corners
	else if ((x == 0 || x == w - 1) && (y < ym1 || y > ym2))
		return RGB(255, 255, 255);
	// white box outlines to corners
	else if ((y == 0 || y == h - 1) && (x < xm1 || x > xm2))
		return RGB(255, 255, 255);
	// blue bar on the left
	else if (x < xm1 && (y > ym1 && y < ym2))
		return RGB(0, 0, 255);
	// blue bar on the top
	else if (y < ym1 && (x > xm1 && x < xm2))
		return RGB(0, 0, 255);
	// red bar on the right
	else if (x > xm2 && (y > ym1 && y < ym2))
		return RGB(255, 0, 0);
	// red bar on the bottom
	else if (y > ym2 && (x > xm1 && x < xm2))
		return RGB(255, 0, 0);
	// inside the margins
	else if (x > xm1 && x < xm2 && y > ym1 && y < ym2) {
		// diagonal line
		if (x == y || w - x == h - y)
			return RGB(255, 255, 255);
		// diagonal line
		else if (w - x - 1 == y || x == h - y - 1)
			return RGB(255, 255, 255);
		else {
			int t = (x - xm1 - 1) * 8 / (xm2 - xm1 - 1);
			unsigned r = 0, g = 0, b = 0;

			unsigned c = (y - ym1 - 1) % 256;

			switch (t) {
			case 0:
				r = c;
				break;
			case 1:
				g = c;
				break;
			case 2:
				b = c;
				break;
			case 3:
				g = b = c;
				break;
			case 4:
				r = b = c;
				break;
			case 5:
				r = g = c;
				break;
			case 6:
				r = g = b = c;
				break;
			case 7:
				break;
			}

			return RGB(r, g, b);
		}
	} else {
		// black corners
		return RGB(0, 0, 0);
	}
}

static void draw_test_pattern_part(IFramebuffer& fb, unsigned start_y, unsigned end_y, YUVType yuvt)
{
	unsigned x, y;
	unsigned w = fb.width();

	switch (fb.format()) {
	case PixelFormat::XRGB8888:
	case PixelFormat::XBGR8888:
	case PixelFormat::ARGB8888:
	case PixelFormat::ABGR8888:
	case PixelFormat::RGB888:
	case PixelFormat::BGR888:
	case PixelFormat::RGB565:
	case PixelFormat::BGR565:
		for (y = start_y; y < end_y; y++) {
			for (x = 0; x < w; x++) {
				RGB pixel = get_test_pattern_pixel(fb, x, y);
				draw_rgb_pixel(fb, x, y, pixel);
			}
		}
		break;

	case PixelFormat::UYVY:
	case PixelFormat::YUYV:
	case PixelFormat::YVYU:
	case PixelFormat::VYUY:
		for (y = start_y; y < end_y; y++) {
			for (x = 0; x < w; x += 2) {
				RGB pixel1 = get_test_pattern_pixel(fb, x, y);
				RGB pixel2 = get_test_pattern_pixel(fb, x + 1, y);
				draw_yuv422_macropixel(fb, x, y, pixel1.yuv(yuvt), pixel2.yuv(yuvt));
			}
		}
		break;

	case PixelFormat::NV12:
	case PixelFormat::NV21:
		for (y = start_y; y < end_y; y += 2) {
			for (x = 0; x < w; x += 2) {
				RGB pixel00 = get_test_pattern_pixel(fb, x, y);
				RGB pixel10 = get_test_pattern_pixel(fb, x + 1, y);
				RGB pixel01 = get_test_pattern_pixel(fb, x, y + 1);
				RGB pixel11 = get_test_pattern_pixel(fb, x + 1, y + 1);
				draw_yuv420_macropixel(fb, x, y,
						       pixel00.yuv(yuvt), pixel10.yuv(yuvt),
						       pixel01.yuv(yuvt), pixel11.yuv(yuvt));
			}
		}
		break;
	default:
		throw std::invalid_argument("unknown pixelformat");
	}
}

static void draw_test_pattern_impl(IFramebuffer& fb, YUVType yuvt)
{
	draw_test_pattern_part(fb, 0, fb.height(), yuvt);
}

void draw_test_pattern(IFramebuffer &fb, YUVType yuvt)
{
#ifdef DRAW_PERF_PRINT
	Stopwatch sw;
	sw.start();
#endif

	draw_test_pattern_impl(fb, yuvt);

#ifdef DRAW_PERF_PRINT
	double us = sw.elapsed_us();
	printf("draw took %u us\n", (unsigned)us);
#endif
}

}
