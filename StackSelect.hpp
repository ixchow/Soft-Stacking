#ifndef STACK_SELECT_HPP
#define STACK_SELECT_HPP

#include <QtGui>
#include <vector>

class StackOp;
class Canvas;

class StackSelect : public QWidget {
	Q_OBJECT
public:
	StackSelect(Canvas *canvas, QWidget *parent = 0);

protected:
	virtual void closeEvent(QCloseEvent *event);


public slots:
	void shorthand_edited(const QString &);

signals:
	void op_changed(const StackOp *);
	void did_hide();

public:

	Canvas *canvas;

	QLabel *longhand;
	QLineEdit *shorthand;
};


#endif //LAYER_LIST_HPP
