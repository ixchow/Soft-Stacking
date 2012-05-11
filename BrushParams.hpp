#ifndef BRUSH_PARAMS_HPP
#define BRUSH_PARAMS_HPP

#include <QtGui>
#include <vector>

class BrushParams : public QWidget {
	Q_OBJECT
public:
	BrushParams(QWidget *parent = 0);

public slots:
	void set_radius(float radius);
	void set_softness(float softness);
	void set_flow(float flow);

	void radius_changed(int);
	void softness_changed(int);
	void flow_changed(int);

signals:
	void edited_radius(float);
	void edited_softness(float);
	void edited_flow(float);

public:

	QSlider *radius_slider;
	QSlider *softness_slider;
	QSlider *flow_slider;

	bool setting_stuff_now;
};


#endif //LAYER_LIST_HPP
