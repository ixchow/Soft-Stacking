#ifndef MISC_HPP
#define MISC_HPP

#include <QtGui>
#include <vector>

class Misc : public QWidget {
	Q_OBJECT
public:
	Misc(QWidget *parent = 0);

public slots:
	void quality_changed(int);

signals:
	void set_blocks(int);
	void set_samples(int);

public:

	QComboBox *quality;
	QToolButton *save;
};


#endif //LAYER_LIST_HPP
