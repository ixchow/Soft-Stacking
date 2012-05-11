#include "BrushParams.hpp"

BrushParams::BrushParams(QWidget *parent) : QWidget(parent) {
	setting_stuff_now = true;

	radius_slider = new QSlider(Qt::Horizontal, this);
	radius_slider->setMinimum(0);
	radius_slider->setMaximum(1000);
	radius_slider->setTracking(true);
	softness_slider = new QSlider(Qt::Horizontal, this);
	softness_slider->setMinimum(0);
	softness_slider->setMaximum(1000);
	softness_slider->setTracking(true);
	flow_slider = new QSlider(Qt::Horizontal, this);
	flow_slider->setMinimum(0);
	flow_slider->setMaximum(1000);
	flow_slider->setTracking(true);

	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(new QLabel("Rad:"));
	layout->addWidget(radius_slider, 1);
	layout->addWidget(new QLabel("Soft:"));
	layout->addWidget(softness_slider, 1);
	layout->addWidget(new QLabel("Flow:"));
	layout->addWidget(flow_slider, 1);

	setLayout(layout);

	setting_stuff_now = false;

	connect(radius_slider, SIGNAL(valueChanged(int)), this, SLOT(radius_changed(int)));
	connect(softness_slider, SIGNAL(valueChanged(int)), this, SLOT(softness_changed(int)));
	connect(flow_slider, SIGNAL(valueChanged(int)), this, SLOT(flow_changed(int)));
}

void BrushParams::set_radius(float radius) {
	int iradius = log(radius) / log(500.0f) * 1000.0f;
	setting_stuff_now = true;
	radius_slider->setValue(iradius);
	setting_stuff_now = false;
}

void BrushParams::radius_changed(int iradius) {
	if (setting_stuff_now) return;
	float radius = exp(iradius / 1000.0f * log(500.0f));
	emit edited_radius(radius);
}

void BrushParams::set_softness(float softness) {
	int isoftness = softness * 1000;
	setting_stuff_now = true;
	softness_slider->setValue(isoftness);
	setting_stuff_now = false;
}

void BrushParams::softness_changed(int isoftness) {
	if (setting_stuff_now) return;
	float softness = isoftness / 1000.0f;
	emit edited_softness(softness);
}

void BrushParams::set_flow(float flow) {
	int iflow = flow * 1000;
	setting_stuff_now = true;
	flow_slider->setValue(iflow);
	setting_stuff_now = false;
}

void BrushParams::flow_changed(int iflow) {
	if (setting_stuff_now) return;
	float flow = iflow / 1000.0f;
	emit edited_flow(flow);
}
