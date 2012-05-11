#include "Tiled.hpp"

#include "LayerOps.hpp"
#include "StackOps.hpp"

using std::vector;

Layer::Layer(std::string const &_name, QImage const &from, const LayerOp *_op) : name(_name), op(_op) {
	thumbnail = from.scaled(ThumbSize, ThumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	{ //checked background:
		thumbnail = thumbnail.convertToFormat(QImage::Format_ARGB32);
		Vector2ui pix_size = make_vector< unsigned int >(thumbnail.width(), thumbnail.height());
		for (unsigned int y = 0; y < pix_size.y; ++y) {
			QRgb * line = reinterpret_cast< QRgb * >(thumbnail.scanLine(y));
			for (unsigned int x = 0; x < pix_size.x; ++x) {
				int r = qRed(line[x]);
				int g = qGreen(line[x]);
				int b = qBlue(line[x]);
				int a = qAlpha(line[x]);
				int gr = 0xdd;
				if (((y % 20) < 10) ^ ((x % 20) < 10)) {
					gr = 0x88;
				}
				r = ((r - gr) * a + gr * 255) / 255;
				g = ((g - gr) * a + gr * 255) / 255;
				b = ((b - gr) * a + gr * 255) / 255;
				line[x] = qRgb(r,g,b);
			}
		}
	}

	const QImage argb = from.convertToFormat(QImage::Format_ARGB32);
	Vector2ui pix_size = make_vector< unsigned int >(from.width(), from.height());

	//hack-y way of converting to tiles, since I don't know for sure if
	//QImage will pad scanlines:
	vector< uint32_t > temp(pix_size.x * pix_size.y);
	for (unsigned int y = 0; y < pix_size.y; ++y) {
		const QRgb * line = reinterpret_cast< const QRgb * >(argb.scanLine(y));
		uint32_t *out = &temp[y * pix_size.x];
		for (unsigned int x = 0; x < pix_size.x; ++x) {
			reinterpret_cast< uint8_t * >(out)[4*x+0] = qRed(line[x]);
			reinterpret_cast< uint8_t * >(out)[4*x+1] = qGreen(line[x]);
			reinterpret_cast< uint8_t * >(out)[4*x+2] = qBlue(line[x]);
			reinterpret_cast< uint8_t * >(out)[4*x+3] = qAlpha(line[x]);
		}
	}


	set(pix_size, &temp[0]);
}

Layer::Layer(std::string const &_name, Vector2ui pix_size, const LayerOp *_op) : Tiled< uint32_t >(pix_size), name(_name), op(_op) {
}

Stroke::Stroke(QImage const &from, const StackOp *_op) : op(_op) {
	const QImage argb = from.convertToFormat(QImage::Format_ARGB32);
	Vector2ui pix_size = make_vector< unsigned int >(from.width(), from.height());

	//hack-y way of converting to tiles, since I don't know for sure if
	//QImage will pad scanlines:
	vector< uint8_t > temp(pix_size.x * pix_size.y);
	for (unsigned int y = 0; y < pix_size.y; ++y) {
		const QRgb * line = reinterpret_cast< const QRgb * >(argb.scanLine(y));
		uint8_t *out = &temp[y * pix_size.x];
		for (unsigned int x = 0; x < pix_size.x; ++x) {
			out[x] = qGray(line[x]);
		}
	}

	set(pix_size, &temp[0]);
}


Stroke::Stroke(Vector2ui pix_size, const StackOp *_op) : Tiled< uint8_t >(pix_size), op(_op) {
}
