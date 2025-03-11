#include "elementpropertieswidget.h"
#include "inputdelegate.h"
#include "projectmodel.h"
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

ElementPropertiesWidget::ElementPropertiesWidget(QWidget *parent, ProjectModel *project_model)
: QWidget(parent)
, project_model_(project_model)
{
    initUi();
    setupConnections();
    setupDelegates();
}

void ElementPropertiesWidget::initUi()
{
    auto main_layout = new QVBoxLayout(this);

    // 创建表格
    ui_.properties_table = new QTableWidget(this);
    ui_.properties_table->setRowCount(10); // 10个属性

    // 设置滚动条策略
    ui_.properties_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui_.properties_table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 固定表格大小，启用滚动
    // ui_.properties_table->setFixedWidth(400); // 设置一个合适的固定宽度

    setupTableHeaders();

    // 按钮布局
    auto button_layout = new QHBoxLayout();
    ui_.add_button = new QPushButton(tr("Add Element"), this);
    ui_.remove_button = new QPushButton(tr("Remove Element"), this);
    button_layout->addWidget(ui_.add_button);
    button_layout->addWidget(ui_.remove_button);
    button_layout->addStretch();

    main_layout->addWidget(ui_.properties_table);
    main_layout->addLayout(button_layout);

    setLayout(main_layout);
}

void ElementPropertiesWidget::setupTableHeaders()
{
    QStringList headers;
    headers << tr("Element 1");
    ui_.properties_table->setHorizontalHeaderLabels(headers);

    // 设置垂直表头（属性名称和单位分两行）
    QStringList row_headers;
    row_headers << tr("Element ID")                        // 无单位
                << tr("Element Type")                      // 无单位
                << tr("Young's Modulus\n(ugr/(mm·m·ms²))") // 换行显示单位
                << tr("Poisson's Ratio\n(-)")              // 无量纲，显示(-)
                << tr("Friction Coefficient\n(-)")         // 无量纲，显示(-)
                << tr("Density\n(ugr/mm³)") << tr("Viscous Damping\n(ugr/(mm·ms))")
                << tr("Normal Stiffness\n(ugr/(ms²))") << tr("Tangential Stiffness\n(ugr/(mm·m·ms²))")
                << tr("Color"); // 无单位

    ui_.properties_table->setVerticalHeaderLabels(row_headers);

    // 设置表头和单元格大小
    ui_.properties_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui_.properties_table->horizontalHeader()->setDefaultSectionSize(120);
    ui_.properties_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui_.properties_table->verticalHeader()->setDefaultSectionSize(45); // 增加行高以适应两行文本
    ui_.properties_table->verticalHeader()->setMinimumWidth(100);

    // 设置文本对齐和换行
    for (int i = 0; i < ui_.properties_table->rowCount(); ++i)
    {
        auto header = ui_.properties_table->verticalHeaderItem(i);
        if (header)
        {
            header->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
}

void ElementPropertiesWidget::setupConnections()
{
    connect(ui_.add_button, &QPushButton::clicked, this, &ElementPropertiesWidget::addNewElement);
    connect(ui_.remove_button, &QPushButton::clicked, this, &ElementPropertiesWidget::removeSelectedElement);
    connect(ui_.properties_table, &QTableWidget::cellChanged, this, &ElementPropertiesWidget::onCellChanged);
    connect(project_model_, &ProjectModel::elementSetsChanged, this, &ElementPropertiesWidget::onElementSetsChanged);
}

void ElementPropertiesWidget::setupDelegates()
{
    // 创建并设置验证delegate
    validate_delegate_ = new InputValidationDelegate(this);
    color_delegate_ = new ColorDelegate(this);
    combobox_delegate_ = new ComboBoxDelegate(this);

    // 更新元素集列表
    updateElementSetItems();

    // 为不同行设置delegate
    // 不再为ID行设置代理
    ui_.properties_table->setItemDelegateForRow(9, color_delegate_);

    // 为其他行设置数值验证delegate
    for (int i = 1; i < 9; ++i)
    {
        ui_.properties_table->setItemDelegateForRow(i, validate_delegate_);
    }
}

void ElementPropertiesWidget::updateElementSetItems()
{
    QStringList items;
    for (const auto &set : project_model_->elementSets().keys())
    {
        items << set;
    }
    combobox_delegate_->setItems(items);
}

void ElementPropertiesWidget::addNewElement()
{
    // 检查是否有可用的元素集
    if (project_model_->elementSets().isEmpty())
    {
        QMessageBox::warning(this, tr("Warning"), tr("No element sets available. Please create element sets first."));
        return;
    }

    // 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add New Element"));
    dialog.setMinimumWidth(300);

    // 创建布局
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // 添加标签
    QLabel *label = new QLabel(tr("Select Element Set:"), &dialog);
    layout->addWidget(label);

    // 创建下拉框
    QComboBox *comboBox = new QComboBox(&dialog);
    for (const auto &set : project_model_->elementSets().keys())
    {
        comboBox->addItem(set);
    }
    layout->addWidget(comboBox);

    // 添加按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    // 显示对话框
    if (dialog.exec() != QDialog::Accepted)
    {
        return; // 用户取消了操作
    }

    // 获取选择的元素集
    QString selectedElementSet = comboBox->currentText();
    if (selectedElementSet.isEmpty())
    {
        return;
    }

    const QSignalBlocker blocker(ui_.properties_table);
    const int col = ui_.properties_table->columnCount();

    if (col >= project_model_->elementSets().size())
    {
        return;
    }

    ui_.properties_table->setColumnCount(col + 1);
    ui_.properties_table->setHorizontalHeaderItem(col, new QTableWidgetItem(tr("Element %1").arg(col + 1)));

    // 创建新的元素属性
    ElementProperty newProperty;
    newProperty.id = selectedElementSet; // 设置为选择的元素集ID

    // 设置表格项
    // Element ID - 设置为只读
    auto id_item = new QTableWidgetItem();
    id_item->setData(Qt::EditRole, selectedElementSet);
    id_item->setFlags(id_item->flags() & ~Qt::ItemIsEditable); // 设置为只读
    ui_.properties_table->setItem(0, col, id_item);

    // 其他属性项
    for (int row = 1; row < 9; ++row)
    {
        auto item = new QTableWidgetItem();
        item->setData(Qt::EditRole, "");
        ui_.properties_table->setItem(row, col, item);
    }

    // 颜色
    auto color_item = new QTableWidgetItem();
    color_item->setData(Qt::BackgroundRole, QColor(255 * 0.5, 255 * 0.5, 255 * 0.5));
    ui_.properties_table->setItem(9, col, color_item);
    newProperty.color = QColor(255 * 0.5, 255 * 0.5, 255 * 0.5);

    // 添加到模型
    project_model_->addElementProperty(newProperty);
}

void ElementPropertiesWidget::removeSelectedElement()
{
    const auto selected_cols = ui_.properties_table->selectionModel()->selectedColumns();
    if (!selected_cols.isEmpty())
    {
        ui_.properties_table->removeColumn(selected_cols.first().column());

        // 更新剩余列的表头和ID
        for (int col = 0; col < ui_.properties_table->columnCount(); ++col)
        {
            ui_.properties_table->setHorizontalHeaderItem(col, new QTableWidgetItem(tr("Element %1").arg(col + 1)));

            // 更新ID
            auto id_item = new QTableWidgetItem(QString::number(col));
            id_item->setFlags(id_item->flags() & ~Qt::ItemIsEditable);
            ui_.properties_table->setItem(0, col, id_item);
        }
    }
}

void ElementPropertiesWidget::onCellChanged(int row, int column)
{
    if (row < 0 || column < 0 || column >= ui_.properties_table->columnCount())
        return;

    // 获取当前元素的ID
    QTableWidgetItem *idItem = ui_.properties_table->item(0, column);
    if (!idItem)
        return;

    QString element_id = idItem->text();

    // 确保索引有效
    if (column >= project_model_->elementProperties().size())
        return;

    // 获取当前元素的属性
    ElementProperty prop = project_model_->elementProperties()[column];

    QTableWidgetItem *item = ui_.properties_table->item(row, column);
    if (!item)
        return;

    QString value = item->text();

    // 使用switch更新属性
    switch (row)
    {
    case 0:
        prop.id = value;
        break;
    case 1:
        prop.type = value.toInt();
        break;
    case 2:
        prop.youngs_modulus = value.toDouble();
        break;
    case 3:
        prop.poisson_ratio = value.toDouble();
        break;
    case 4:
        prop.friction_coef = value.toDouble();
        break;
    case 5:
        prop.density = value.toDouble();
        break;
    case 6:
        prop.viscous_damping = value.toDouble();
        break;
    case 7:
        prop.normal_stiffness = value.toDouble();
        break;
    case 8:
        prop.tangential_stiffness = value.toDouble();
        break;
    case 9:
        {
            auto color = item->data(Qt::BackgroundRole).value<QColor>();
            prop.color = color;
            break;
        }
    default:
        return;
    }

    // 更新项目模型中的属性
    project_model_->updateElementProperty(element_id, prop);
}

void ElementPropertiesWidget::onElementSetsChanged()
{
    // 使用之前创建的辅助方法更新元素集列表
    updateElementSetItems();
}
