#include "StackSelect.hpp"

#include "Canvas.hpp"
#include "StackOps.hpp"

StackSelect::StackSelect(Canvas *_canvas, QWidget *parent) : QWidget(parent), canvas(_canvas) {

	setWindowTitle(tr("Mapping Selector"));
	setMinimumSize(200,200);

	longhand = new QLabel(this);
	longhand->setWordWrap(true);
	shorthand = new QLineEdit(this);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addStretch(1);
	layout->addWidget(longhand, 0);
	layout->addWidget(shorthand, 0);

	setLayout(layout);

	connect(shorthand, SIGNAL(textEdited(const QString &)), this, SLOT(shorthand_edited(const QString &)));
}

void StackSelect::closeEvent(QCloseEvent *event) {
	event->ignore();
	hide();
	emit did_hide();
}

void StackSelect::shorthand_edited(const QString &qop) {
	std::string error = "";
	const StackOp *op = StackOp::from_shorthand(qPrintable(qop), canvas->layer_names(), &error);
	if (op) {
		longhand->setText(op->description(canvas->layer_names()).c_str());
	} else {
		longhand->setText(("Error: " + error).c_str());
	}

}

