#include "Misc.hpp"

#include <sstream>
#include <cassert>

const unsigned int Count = 5;

const unsigned int Samples[Count] = {
	2,
	5,
	10,
	20,
	900,
};
const unsigned int Blocks[Count] = {
	4,
	8,
	8,
	16,
	16,
};
const char *Desc[Count] = {
	":(",
	":|",
	":)",
	":D",
	":x",
};

Misc::Misc(QWidget *parent) : QWidget(parent) {
	quality = new QComboBox(this);
	for (unsigned int i = 0; i < Count; ++i) {
		std::ostringstream str;
		str << 's' << Samples[i] << 'b' << Blocks[i] << " " << Desc[i];
		quality->addItem(str.str().c_str());
	}

	save = new QToolButton(this);
	save->setIcon(QIcon("save.png"));

	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(save);
	layout->addWidget(quality);

	setLayout(layout);

	connect(quality, SIGNAL(currentIndexChanged(int)), this, SLOT(quality_changed(int)));
}

void Misc::quality_changed(int idx) {
	assert((unsigned int)(idx) < Count);
	emit set_blocks(Blocks[idx]);
	emit set_samples(Samples[idx]);
}
