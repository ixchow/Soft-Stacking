#ifndef STROKE_LIST_HPP
#define STROKE_LIST_HPP

#include <QtGui>
#include <vector>

class Stroke;
class StrokeContainer;
class Canvas;

class StrokeList : public QScrollArea {
	Q_OBJECT
signals:
	void stroke_selected(unsigned int stroke);
	void save_stroke(unsigned int stroke);
	void delete_stroke(unsigned int stroke);

public slots:
	void stroke_brush_pressed(int stroke);
	void stroke_save_pressed(int stroke);
	void stroke_delete_pressed(int stroke);
	void op_edited(int stroke);
	void op_edit_finished(int stroke);
	
public:
	StrokeList(Canvas *canvas, QWidget *parent = NULL);

	void update_list(std::vector< Stroke * > const &strokes, std::vector< std::string > const &layer_names);
	void set_active_stroke(unsigned int stroke, bool pending);
	std::vector< StrokeContainer * > containers;

	QToolButton *add;
	QLineEdit *add_text;
	QToolButton *open;
	QToolButton *selector;
	QHBoxLayout *add_layout;
	QSignalMapper *brush_mapper;
	QSignalMapper *save_mapper;
	QSignalMapper *delete_mapper;
	QSignalMapper *op_edited_mapper;
	QSignalMapper *op_edit_finished_mapper;

	Canvas *canvas;
};


#endif //STROKE_LIST_HPP
