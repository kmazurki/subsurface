// SPDX-License-Identifier: GPL-2.0
#include "statswidget.h"
#include "mainwindow.h"
#include "stats/statsview.h"
#include <QCheckBox>
#include <QPainter>
#include <QStyledItemDelegate>

class ChartItemDelegate : public QStyledItemDelegate {
private:
	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		   const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

// Number of pixels that non-toplevel items are indented
static int indent(const QFontMetrics &fm)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
	return 4 * fm.width('-');
#else
	return 4 * fm.horizontalAdvance('-');
#endif
}

static const int iconSpace = 2; // Number of pixels between icon and text
static const int topSpace = 2; // Number of pixels above icon

void ChartItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
			      const QModelIndex &index) const
{
	QFont font = index.data(Qt::FontRole).value<QFont>();
	QFontMetrics fm(font);
	QString name = index.data(ChartListModel::ChartNameRole).value<QString>();
	painter->setFont(font);
	QRect rect = option.rect;
	if (option.state & QStyle::State_Selected) {
		painter->save();
		painter->setBrush(option.palette.highlight());
		painter->setPen(Qt::NoPen);
		painter->drawRect(rect);
		painter->restore();
	}
	bool isHeader = index.data(ChartListModel::IsHeaderRole).value<bool>();
	if (!isHeader)
		rect.translate(indent(fm), 0);
	QPixmap icon = index.data(ChartListModel::IconRole).value<QPixmap>();
	if (!icon.isNull()) {
		rect.translate(0, topSpace);
		painter->drawPixmap(rect.topLeft(), icon);
		rect.translate(icon.size().width() + iconSpace, (icon.size().height() - fm.height()) / 2);
	}

	painter->drawText(rect, name);
}

QSize ChartItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QFont font = index.data(Qt::FontRole).value<QFont>();
	QFontMetrics fm(font);
	QString name = index.data(ChartListModel::ChartNameRole).value<QString>();
	QSize iconSize = index.data(ChartListModel::IconSizeRole).value<QSize>();
	QSize size = fm.size(Qt::TextSingleLine, name);
	bool isHeader = index.data(ChartListModel::IsHeaderRole).value<bool>();
	if (!isHeader)
		size += QSize(indent(fm), 0);
	if (iconSize.isValid())
		size = QSize(size.width() + iconSize.width() + iconSpace,
			     std::max(size.height(), iconSize.height()) + 2 * topSpace);
	return size;
}

StatsWidget::StatsWidget(QWidget *parent) : QWidget(parent)
{
	ui.setupUi(this);

	connect(ui.close, &QToolButton::clicked, this, &StatsWidget::closeStats);

	ui.chartType->setModel(&charts);
	connect(ui.chartType, QOverload<int>::of(&QComboBox::activated), this, &StatsWidget::chartTypeChanged);
	connect(ui.var1, QOverload<int>::of(&QComboBox::activated), this, &StatsWidget::var1Changed);
	connect(ui.var2, QOverload<int>::of(&QComboBox::activated), this, &StatsWidget::var2Changed);
	connect(ui.var1Binner, QOverload<int>::of(&QComboBox::activated), this, &StatsWidget::var1BinnerChanged);
	connect(ui.var2Binner, QOverload<int>::of(&QComboBox::activated), this, &StatsWidget::var2BinnerChanged);
	connect(ui.var2Operation, QOverload<int>::of(&QComboBox::activated), this, &StatsWidget::var2OperationChanged);
}

// Initialize QComboBox with list of variables
static void setVariableList(QComboBox *combo, const StatsState::VariableList &list)
{
	combo->clear();
	for (const StatsState::Variable &v: list.variables)
		combo->addItem(v.name, QVariant(v.id));
	combo->setCurrentIndex(list.selected);
}

// Initialize QComboBox and QLabel of binners. Hide if there are no binners.
static void setBinList(QLabel *label, QComboBox *combo, const QString &varName, const StatsState::BinnerList &list)
{
	combo->clear();
	if (list.binners.empty()) {
		label->hide();
		combo->hide();
		return;
	}

	label->show();
	combo->show();

	for (const QString &s: list.binners)
		combo->addItem(s);
	combo->setCurrentIndex(list.selected);
}

// Initialize QComboBox and QLabel of operations. Hide if there are no operations.
static void setOperationList(QLabel *label, QComboBox *combo, const QString &varName, const StatsState::VariableList &list)
{
	combo->clear();
	if (list.variables.empty()) {
		label->hide();
		combo->hide();
		return;
	}

	label->show();
	combo->show();

	setVariableList(combo, list);
}

void StatsWidget::updateUi()
{
	StatsState::UIState uiState = state.getUIState();
	setVariableList(ui.var1, uiState.var1);
	setVariableList(ui.var2, uiState.var2);
	int pos = charts.update(uiState.charts);
	ui.chartType->setCurrentIndex(pos);
	ui.chartType->setItemDelegate(new ChartItemDelegate);
	setBinList(ui.var1BinnerLabel, ui.var1Binner, uiState.var1Name, uiState.binners1);
	setBinList(ui.var2BinnerLabel, ui.var2Binner, uiState.var2Name, uiState.binners2);
	setOperationList(ui.var2OperationLabel, ui.var2Operation, uiState.var2Name, uiState.operations2);

	// Add checkboxes for additional features
	features.clear();
	for (const StatsState::Feature &f: uiState.features) {
		features.emplace_back(new QCheckBox(f.name));
		QCheckBox *check = features.back().get();
		check->setChecked(f.selected);
		int id = f.id;
		connect(check, &QCheckBox::stateChanged, [this,id] (int state) { featureChanged(id, state); });
		ui.features->addWidget(check);
	}

	ui.stats->plot(state);
}

void StatsWidget::closeStats()
{
	MainWindow::instance()->setApplicationState(ApplicationState::Default);
}

void StatsWidget::chartTypeChanged(int idx)
{
	state.chartChanged(ui.chartType->itemData(idx).toInt());
	updateUi();
}

void StatsWidget::var1Changed(int idx)
{
	state.var1Changed(ui.var1->itemData(idx).toInt());
	updateUi();
}

void StatsWidget::var2Changed(int idx)
{
	state.var2Changed(ui.var2->itemData(idx).toInt());
	updateUi();
}

void StatsWidget::var1BinnerChanged(int idx)
{
	state.binner1Changed(idx);
	updateUi();
}

void StatsWidget::var2BinnerChanged(int idx)
{
	state.binner2Changed(idx);
	updateUi();
}

void StatsWidget::var2OperationChanged(int idx)
{
	state.var2OperationChanged(ui.var2Operation->itemData(idx).toInt());
	updateUi();
}

void StatsWidget::featureChanged(int idx, bool status)
{
	state.featureChanged(idx, status);
	updateUi();
}

void StatsWidget::showEvent(QShowEvent *e)
{
	updateUi();
	QWidget::showEvent(e);
}