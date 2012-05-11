#include "StrokeList.hpp"

#include "Canvas.hpp"
#include "StackOps.hpp"

#include <iostream>

using std::vector;
using std::string;

class StrokeContainer : public QFrame {
public:
	StrokeContainer(Stroke *from, vector< string > const &layer_names) : QFrame(NULL) {
		assert(from);
		setFrameStyle(QFrame::Box | QFrame::Plain);
		setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
		QHBoxLayout *layout = new QHBoxLayout;
		brush = new QToolButton;
		brush->setCheckable(true);
		brush->setIcon(QIcon("brush.png"));
		layout->addWidget(brush);
		//QLabel *image = new QLabel("(img)");
		//layout->addWidget(image, 0);

		last_good_op = from->op->shorthand(layer_names);
		op = new QLineEdit(from->op->shorthand(layer_names).c_str());
		layout->addWidget(op, 1);

		op_label = new QLabel();
		layout->addWidget(op_label);
		set_op_good();

		save = new QToolButton;
		save->setIcon(QIcon("save.png"));
		layout->addWidget(save, 0);

		del = new QToolButton;
		del->setIcon(QIcon("delete.png"));
		layout->addWidget(del, 0);

		setLayout(layout);
	}
	void set_op_good(bool good = true) {
		if (good) {
			op_label->setPixmap(QPixmap("okay.png"));
		} else {
			op_label->setPixmap(QPixmap("bad.png"));
		}
	}
	std::string last_good_op;
	QLineEdit *op;
	QLabel *op_label;

	QToolButton *brush;
	QToolButton *del;
	QToolButton *save;
};

void StrokeList::stroke_brush_pressed(int stroke) {
	assert((unsigned)stroke < containers.size());
	emit stroke_selected(static_cast< unsigned int >(stroke));
}

void StrokeList::stroke_save_pressed(int stroke) {
	assert((unsigned)stroke < containers.size());
	emit save_stroke(static_cast< unsigned int >(stroke));
}

void StrokeList::stroke_delete_pressed(int stroke) {
	assert((unsigned)stroke < containers.size());
	emit delete_stroke(static_cast< unsigned int >(stroke));
}

void StrokeList::op_edited(int stroke) {
	assert(unsigned(stroke) < containers.size());
	std::string new_op = qPrintable(containers[stroke]->op->text());
	if (canvas->update_stroke_op(stroke, new_op)) {
		containers[stroke]->last_good_op = new_op;
		containers[stroke]->set_op_good(true);
	} else {
		containers[stroke]->set_op_good(false);
	}
}

void StrokeList::op_edit_finished(int stroke) {
	assert(unsigned(stroke) < containers.size());
	containers[stroke]->op->setText(containers[stroke]->last_good_op.c_str());
	containers[stroke]->set_op_good(true);
}

StrokeList::StrokeList(Canvas *_canvas, QWidget *parent) : QScrollArea(parent), canvas(_canvas) {
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	QWidget *widget = new QWidget;
	add = new QToolButton;
	add->setIcon(QIcon("add.png"));
	add_text = new QLineEdit;
	open = new QToolButton;
	open->setIcon(QIcon("open.png"));

	selector = new QToolButton;
	selector->setIcon(QIcon("mapper.png"));
	selector->setCheckable(true);

	add_layout = new QHBoxLayout;
	add_layout->addWidget(add, 0);
	add_layout->addWidget(open, 0);
	add_layout->addWidget(add_text, 1);
	add_layout->addWidget(selector, 0);

	brush_mapper = NULL;
	save_mapper = NULL;
	delete_mapper = NULL;
	op_edited_mapper = NULL;
	op_edit_finished_mapper = NULL;

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addLayout(add_layout);
	layout->addStretch(1);
	widget->setLayout(layout);
	setWidgetResizable(true);
	setWidget(widget);
	setMinimumSize(QSize(200, 200));
}

void StrokeList::update_list(vector< Stroke * > const &strokes, vector< string > const &layer_names) {
	
	delete brush_mapper;
	brush_mapper = new QSignalMapper(this);

	delete save_mapper;
	save_mapper = new QSignalMapper(this);

	delete delete_mapper;
	delete_mapper = new QSignalMapper(this);

	delete op_edited_mapper;
	op_edited_mapper = new QSignalMapper(this);

	delete op_edit_finished_mapper;
	op_edit_finished_mapper = new QSignalMapper(this);

	delete add_layout;

	add_layout = new QHBoxLayout;
	add_layout->addWidget(add, 0);
	add_layout->addWidget(open, 0);
	add_layout->addWidget(add_text, 1);
	add_layout->addWidget(selector, 0);

	delete widget()->layout();
	while (!containers.empty()) {
		delete containers.back();
		containers.pop_back();
	}

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addLayout(add_layout);
	for (unsigned int i = 0; i < strokes.size(); ++i) {
		StrokeContainer *cont = new StrokeContainer(strokes[i], layer_names);
		containers.push_back(cont);
		brush_mapper->setMapping(cont->brush, i);
		connect(cont->brush, SIGNAL(clicked()), brush_mapper, SLOT(map()));
		save_mapper->setMapping(cont->save, i);
		connect(cont->save, SIGNAL(clicked()), save_mapper, SLOT(map()));
		delete_mapper->setMapping(cont->del, i);
		connect(cont->del, SIGNAL(clicked()), delete_mapper, SLOT(map()));
		op_edited_mapper->setMapping(cont->op, i);
		connect(cont->op, SIGNAL(textEdited(const QString &)), op_edited_mapper, SLOT(map()));
		op_edit_finished_mapper->setMapping(cont->op, i);
		connect(cont->op, SIGNAL(editingFinished()), op_edit_finished_mapper, SLOT(map()));
	}
	connect(brush_mapper, SIGNAL(mapped(int)), this, SLOT(stroke_brush_pressed(int)));
	connect(save_mapper, SIGNAL(mapped(int)), this, SLOT(stroke_save_pressed(int)));
	connect(delete_mapper, SIGNAL(mapped(int)), this, SLOT(stroke_delete_pressed(int)), Qt::QueuedConnection);
	connect(op_edited_mapper, SIGNAL(mapped(int)), this, SLOT(op_edited(int)));
	connect(op_edit_finished_mapper, SIGNAL(mapped(int)), this, SLOT(op_edit_finished(int)));

	//add strokes from last-to-first:
	for (vector< StrokeContainer * >::reverse_iterator c = containers.rbegin(); c != containers.rend(); ++c) {
		layout->addWidget(*c);
	}
	layout->addStretch(1);
	widget()->setLayout(layout);
}

void StrokeList::set_active_stroke(unsigned int stroke, bool pending) {
	for (unsigned int s = 0; s < containers.size(); ++s) {
		if (s == stroke) {
			containers[s]->brush->setChecked(true);
		} else {
			containers[s]->brush->setChecked(false);
		}
	}
	pending = false; //ignored
}
