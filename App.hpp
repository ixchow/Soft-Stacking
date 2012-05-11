#ifndef APP_HPP
#define APP_HPP

#include <QtGui>

class Canvas;
class LayerList;
class StrokeList;
class StackSelect;

class App : public QMainWindow {
	Q_OBJECT
public:
	App();

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void open();
	void reopen(unsigned int);
	void save();
	void open_stroke();
	void add_stroke();
	void save_stroke(unsigned int stroke);
	void selector_toggled(bool);
	void selector_hidden();

private:
	Canvas *canvas;
	LayerList *layer_list;
	StrokeList *stroke_list;
	StackSelect *stack_select;
};



#endif //APP_HPP
