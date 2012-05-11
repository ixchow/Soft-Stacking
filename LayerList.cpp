#include "LayerList.hpp"

#include "Canvas.hpp"
#include "Constants.hpp"

#include <iostream>
#include <string>

using std::vector;
using std::string;

class LayerContainer : public QFrame {
public:
	LayerContainer(Layer *from) : QFrame(NULL) {
		assert(from);
		setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
		setFrameStyle(QFrame::Box | QFrame::Plain);
		QHBoxLayout *layout = new QHBoxLayout;
		layout->setContentsMargins(0,0,0,0);
		QLabel *image = new QLabel();
		image->setPixmap(QPixmap::fromImage(from->thumbnail));
		image->setMinimumSize(from->thumbnail.size());
		layout->addWidget(image, 0);

		name = new QLineEdit(from->name.c_str());
		last_good_name = from->name;
		layout->addWidget(name, 1);

		name_label = new QLabel();
		name_label->setPixmap(QPixmap("okay.png"));
		layout->addWidget(name_label);

		mode = new QComboBox();
		vector< string > names = LayerOp::op_shorthands();
		for (vector< string >::iterator n = names.begin(); n != names.end(); ++n) {
			mode->addItem(n->c_str());
			if (*n == from->op->shorthand()) {
				mode->setCurrentIndex(n - names.begin());
			}
		}
		layout->addWidget(mode);

		reload = new QToolButton;
		reload->setIcon(QIcon("open.png"));
		layout->addWidget(reload, 0);

		setLayout(layout);
	}
	void set_name_good(bool good = true) {
		if (good) {
			name_label->setPixmap(QPixmap("okay.png"));
		} else {
			name_label->setPixmap(QPixmap("bad.png"));
		}
	}
	QComboBox *mode;
	QLineEdit *name;
	QLabel *name_label;
	QToolButton *reload;
	std::string last_good_name;
};

LayerList::LayerList(Canvas *_canvas, QWidget *parent) : QScrollArea(parent), canvas(_canvas) {
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	QWidget *widget = new QWidget;
	add = new QToolButton;
	add->setIcon(QIcon("open.png"));

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(add);
	layout->addStretch(1);
	widget->setLayout(layout);
	setWidgetResizable(true);
	setWidget(widget);
	setMinimumSize(QSize(200, 200));

	name_edit_finished_mapper = NULL;
	name_edited_mapper = NULL;
	op_activated_mapper = NULL;
	reload_clicked_mapper = NULL;
}

void LayerList::update_list(vector< Layer * > const &layers) {

	delete name_edit_finished_mapper;
	delete name_edited_mapper;
	delete op_activated_mapper;
	delete reload_clicked_mapper;

	name_edit_finished_mapper = new QSignalMapper(this);
	name_edited_mapper = new QSignalMapper(this);
	op_activated_mapper = new QSignalMapper(this);
	reload_clicked_mapper = new QSignalMapper(this);

	connect(name_edit_finished_mapper, SIGNAL(mapped(int)), this, SLOT(name_edit_finished(int)));
	connect(name_edited_mapper, SIGNAL(mapped(int)), this, SLOT(name_edited(int)));
	connect(op_activated_mapper, SIGNAL(mapped(int)), this, SLOT(op_activated(int)));
	connect(reload_clicked_mapper, SIGNAL(mapped(int)), this, SLOT(reload_clicked(int)), Qt::QueuedConnection);
	
	delete widget()->layout();
	while (!containers.empty()) {
		delete containers.back();
		containers.pop_back();
	}

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0,0,0,0);
	layout->setSpacing(1);
	layout->addWidget(add);
	for (unsigned int i = 0; i < layers.size(); ++i) {
		LayerContainer *cont = new LayerContainer(layers[i]);
		containers.push_back(cont);
		name_edit_finished_mapper->setMapping(cont->name, containers.size()-1);
		connect(cont->name, SIGNAL(editingFinished()), name_edit_finished_mapper, SLOT(map()));
		name_edited_mapper->setMapping(cont->name, containers.size()-1);
		connect(cont->name, SIGNAL(textEdited(const QString &)), name_edited_mapper, SLOT(map()));
		op_activated_mapper->setMapping(cont->mode, containers.size()-1);
		connect(cont->mode, SIGNAL(activated(int)), op_activated_mapper, SLOT(map()));
		reload_clicked_mapper->setMapping(cont->reload, containers.size()-1);
		connect(cont->reload, SIGNAL(clicked()), reload_clicked_mapper, SLOT(map()));
	}
	for (vector< LayerContainer * >::reverse_iterator c = containers.rbegin(); c != containers.rend(); ++c) {
		layout->addWidget(*c);
	}
	layout->addStretch(1);
	widget()->setLayout(layout);
}

void LayerList::name_edit_finished(int layer) {
	assert((unsigned)layer < containers.size());
	containers[layer]->name->setText(containers[layer]->last_good_name.c_str());
	containers[layer]->set_name_good(true);
}

void LayerList::name_edited(int layer) {
	assert((unsigned)layer < containers.size());
	std::string new_name = qPrintable(containers[layer]->name->text());
	assert(canvas);
	if (canvas->rename_layer(layer, new_name)) {
		containers[layer]->last_good_name = new_name;
		containers[layer]->set_name_good(true);
	} else {
		containers[layer]->set_name_good(false);
	}
}

void LayerList::op_activated(int layer) {
	assert((unsigned)layer < containers.size());
	std::string new_mode = qPrintable(containers[layer]->mode->currentText());
	canvas->update_layer_op(layer, new_mode);
}

void LayerList::reload_clicked(int layer) {
	assert((unsigned)layer < containers.size());
	emit reload_layer((unsigned int)layer);
}
