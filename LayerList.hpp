#ifndef LAYER_LIST_HPP
#define LAYER_LIST_HPP

#include <QtGui>
#include <vector>

class Canvas;
class Layer;
class LayerContainer;

class LayerList : public QScrollArea {
	Q_OBJECT
public:
	LayerList(Canvas *canvas, QWidget *parent = 0);

public slots:
	void name_edit_finished(int layer);
	void name_edited(int layer);
	void op_activated(int layer);
	void reload_clicked(int layer);

signals:
	void reload_layer(unsigned int);

public:

	void update_list(std::vector< Layer * > const &layers);
	std::vector< LayerContainer * > containers;

	QSignalMapper *name_edit_finished_mapper;
	QSignalMapper *name_edited_mapper;
	QSignalMapper *op_activated_mapper;
	QSignalMapper *reload_clicked_mapper;

	QToolButton *add;
	Canvas *canvas;
};


#endif //LAYER_LIST_HPP
