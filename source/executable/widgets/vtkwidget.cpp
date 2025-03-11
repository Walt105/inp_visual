#include "vtkwidget.h"
#include "inpreader.h"

#include "utils/logging.h"
#include "vtk/pickinteractorstyle.h"
#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QToolBar>
#include <QToolTip>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <QWidget>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkColorTransferFunction.h>
#include <vtkDataSetMapper.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkFloatArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkLookupTable.h>
#include <vtkOutputWindow.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkThreshold.h>
#include <vtkThresholdPoints.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridReader.h>
#include <vtkVertexGlyphFilter.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>

// 自定义VTK输出窗口类
class CustomVtkOutputWindow : public vtkOutputWindow
{
public:
    static CustomVtkOutputWindow *New();
    vtkTypeMacro(CustomVtkOutputWindow, vtkOutputWindow);

    void DisplayText(const char *txt) override { Logging::info("[VTK] {}", txt); }
    void DisplayErrorText(const char *txt) override { Logging::error("[VTK] {}", txt); }
    void DisplayWarningText(const char *txt) override { Logging::warn("[VTK] {}", txt); }
    void DisplayGenericWarningText(const char *txt) override { Logging::warn("[VTK] {}", txt); }
};
vtkStandardNewMacro(CustomVtkOutputWindow);

VtkWidget::VtkWidget(QWidget *parent, ProjectModel *project_model)
: QWidget(parent)
, project_model_(project_model)
, emitter_(new SignalEmitter(this))
, current_style_(InteractorStyle::PickCell) // 初始化为PickCell
{
    vtkOutputWindow::SetInstance(CustomVtkOutputWindow::New());
    initUi();
    setupConnections();
}

void VtkWidget::initUi()
{
    // 创建工具栏
    ui_.toolbar = new QToolBar(this);
    ui_.switch_style = new QAction("Switch Style", this);
    ui_.toolbar->addAction(ui_.switch_style);

    // 添加切换点显示的按钮
    ui_.toggle_points = new QAction("Toggle Points", this);
    ui_.toolbar->addAction(ui_.toggle_points);

    ui_.tooltip_widget = new QLabel(this);
    ui_.tooltip_widget->setWindowFlags(Qt::ToolTip);
    ui_.tooltip_widget->setWindowOpacity(0.8);
    ui_.tooltip_widget->adjustSize();
    ui_.tooltip_widget->setStyleSheet("background-color: white; color: black;");

    // 创建VTK OpenGL部件
    render_window_ = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    renderer_ = vtkSmartPointer<vtkRenderer>::New();
    render_window_->AddRenderer(renderer_);

    vtk_widget_ = new QVTKOpenGLNativeWidget(this);
    vtk_widget_->setRenderWindow(render_window_);

    renderer_->SetBackground(1.0, 1.0, 1.0);

    // 设置2D视角
    vtkCamera *camera = renderer_->GetActiveCamera();
    camera->ParallelProjectionOn(); // 使用平行投影
    camera->SetPosition(0, 0, 1);   // 设置相机位置
    camera->SetFocalPoint(0, 0, 0); // 设置焦点
    camera->SetViewUp(0, 1, 0);     // 设置向上方向

    // 使用自定义的2D拾取交互样式
    vtkRenderWindowInteractor *interactor = render_window_->GetInteractor();
    auto style = vtkSmartPointer<CellPickInteractorStyle>::New();
    style->setEmitter(emitter_);
    style->SetDefaultRenderer(renderer_);
    interactor->SetInteractorStyle(style);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(ui_.toolbar); // 将工具栏添加到布局中
    layout->addWidget(vtk_widget_);
    layout->setContentsMargins(0, 0, 0, 0);
}

void VtkWidget::setupConnections()
{
    connect(emitter_, &SignalEmitter::cellSelected, this, &VtkWidget::onCellSelected);
    connect(emitter_, &SignalEmitter::pointSelected, this, &VtkWidget::onPointSelected);
    connect(ui_.switch_style, &QAction::triggered, this, &VtkWidget::switchInteractorStyle);
    connect(ui_.toggle_points, &QAction::triggered, this, &VtkWidget::togglePointsVisibility);
    connect(project_model_, &ProjectModel::elementPropertyUpdated, this, &VtkWidget::updateElementProperty);
    connect(project_model_, &ProjectModel::boundaryConditionUpdated, this, &VtkWidget::updateNodeProperty);
}

void VtkWidget::switchInteractorStyle()
{
    vtkRenderWindowInteractor *interactor = render_window_->GetInteractor();
    if (current_style_ == InteractorStyle::PickCell)
    {
        // 切换到PickPoint模式
        auto style = vtkSmartPointer<PointPickInteractorStyle>::New();
        style->setEmitter(emitter_);
        style->SetDefaultRenderer(renderer_);
        interactor->SetInteractorStyle(style);
        current_style_ = InteractorStyle::PickPoint;

        // 确保点actor是可见的
        if (all_points_actor_)
        {
            all_points_actor_->SetVisibility(true);
            render_window_->Render();
        }

        Logging::info("Switched to PickPoint style.");
    }
    else
    {
        // 切换到PickCell模式
        auto style = vtkSmartPointer<CellPickInteractorStyle>::New();
        style->setEmitter(emitter_);
        style->SetDefaultRenderer(renderer_);
        interactor->SetInteractorStyle(style);
        current_style_ = InteractorStyle::PickCell;
        Logging::info("Switched to PickCell style.");
    }
}

void VtkWidget::togglePointsVisibility()
{
    if (all_points_actor_)
    {
        // 切换点的可见性
        bool visible = all_points_actor_->GetVisibility();
        all_points_actor_->SetVisibility(!visible);
        Logging::info("Points visibility: {}", !visible ? "on" : "off");
        Logging::info("Points actor exists and visibility toggled to: {}", !visible);
        render_window_->Render();
    }
    else
    {
        Logging::warn("Points actor does not exist, cannot toggle visibility");
    }
}

vtkRenderer *VtkWidget::renderer() const { return renderer_.Get(); }

void VtkWidget::resetCamera() { renderer_->ResetCamera(); }

vtkSmartPointer<vtkActor> VtkWidget::createCellActor(vtkSmartPointer<vtkUnstructuredGrid> grid,
                                                     vtkSmartPointer<vtkLookupTable> lut)
{
    // 创建边缘Actor
    auto edgeMapper = vtkSmartPointer<vtkDataSetMapper>::New();
    edgeMapper->SetInputData(grid);

    auto edgeActor = vtkSmartPointer<vtkActor>::New();
    edgeActor->SetMapper(edgeMapper);
    edgeActor->GetProperty()->EdgeVisibilityOn();
    // edgeActor->GetProperty()->SetEdgeColor(0.2, 0.2, 0.2);
    edgeActor->GetProperty()->SetOpacity(0.5);

    // 设置映射器使用查找表
    edgeMapper->SetLookupTable(lut);
    edgeMapper->SetScalarModeToUseCellData();
    edgeMapper->SetScalarRange(0, lut->GetNumberOfTableValues() - 1);
    edgeMapper->ScalarVisibilityOn();

    return edgeActor;
}

vtkSmartPointer<vtkActor> VtkWidget::createPointActor(vtkSmartPointer<vtkUnstructuredGrid> grid,
                                                      vtkSmartPointer<vtkLookupTable> lut)
{
    if (!grid || grid->GetNumberOfPoints() == 0)
    {
        return nullptr;
    }

    // 使用vtkVertexGlyphFilter将点转换为可视化对象
    auto vertexFilter = vtkSmartPointer<vtkVertexGlyphFilter>::New();
    vertexFilter->SetInputData(grid);
    vertexFilter->Update();

    // 创建mapper和actor
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(vertexFilter->GetOutputPort());
    mapper->SetLookupTable(lut);
    mapper->SetScalarRange(0, lut->GetNumberOfTableValues() - 1);
    mapper->ScalarVisibilityOn();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetPointSize(3); // 设置更大的点尺寸
    actor->GetProperty()->SetOpacity(1.0); // 确保点完全不透明

    return actor;
}

void VtkWidget::setupCellColors(const QMap<QString, std::vector<int>> &elset_sets)
{
    // 创建并初始化查找表
    cell_lut_ = vtkSmartPointer<vtkLookupTable>::New();
    cell_lut_->SetNumberOfTableValues(elset_sets.size());
    cell_lut_->Build();

    // 为每个单元集合设置不同的颜色并记录映射关系
    for (int i = 0; i < elset_sets.size(); ++i)
    {
        QString setName = elset_sets.keys()[i];
        elset_index_map_[setName] = i;
        cell_lut_->SetTableValue(i, 0.5, 0.5, 0.5, 1.0); // 使用默认灰色
    }
}

void VtkWidget::setupPointColors(const QMap<QString, std::vector<int>> &node_sets)
{
    // 创建并初始化查找表
    point_lut_ = vtkSmartPointer<vtkLookupTable>::New();
    point_lut_->SetNumberOfTableValues(node_sets.size() > 0 ? node_sets.size() : 1);
    point_lut_->Build();

    // 为每个节点集合设置不同的颜色
    for (int i = 0; i < node_sets.size(); ++i)
    {
        QString setName = node_sets.keys()[i];

        point_lut_->SetTableValue(i, 0, 0, 0, 1.0);
    }
}

void VtkWidget::loadInpFile(const QString &filename, const FileType &type)
{
    // 清空当前场景
    renderer_->RemoveAllViewProps();

    // 创建解析器并读取文件
    auto &reader = InpReader::getInstance();
    grid_ = vtkSmartPointer<vtkUnstructuredGrid>::New();
    if (type == FileType::Abaqus)
    {
        reader.parseAbaqus(filename);
        grid_ = reader.getMainGrid();
    }
    else if (type == FileType::Gmsh)
    {
        reader.parseGmsh(filename);
        grid_ = reader.getMainGrid();
    }
    auto node_sets = reader.getNodeSets();
    auto elset_sets = reader.getElementSets();
    project_model_->setElementSets(elset_sets);
    project_model_->setNodeSets(node_sets);
    // 清空cell_colors和point_colors
    auto cell_data = grid_->GetCellData();
    for (int i = 0; i < cell_data->GetNumberOfArrays(); i++)
    {
        cell_data->RemoveArray(cell_data->GetArrayName(i));
    }
    auto point_data = grid_->GetPointData();
    for (int i = 0; i < point_data->GetNumberOfArrays(); i++)
    {
        point_data->RemoveArray(point_data->GetArrayName(i));
    }

    if (grid_ && grid_->GetNumberOfPoints() > 0)
    {
        // 创建并填充颜色数组
        auto cell_colors = generateCellColors(elset_sets);
        auto point_colors = generatePointColors(node_sets);

        grid_->GetCellData()->SetScalars(cell_colors);
        grid_->GetPointData()->SetScalars(point_colors);

        // 设置颜色查找表
        setupCellColors(elset_sets);
        setupPointColors(node_sets);

        // 创建单元和点的Actor
        edge_actor_ = createCellActor(grid_, cell_lut_);
        all_points_actor_ = createPointActor(grid_, point_lut_);

        // 确保点Actor默认可见
        if (all_points_actor_)
        {
            all_points_actor_->SetVisibility(true);
            Logging::info("Points actor created successfully and set to visible");
        }
        else
        {
            Logging::error("Failed to create points actor!");
        }

        // 添加到渲染器
        if (edge_actor_)
        {
            renderer_->AddActor(edge_actor_);
            Logging::info("Edge actor added to renderer");
        }

        if (all_points_actor_)
        {
            // 设置点的Z值略高于边，确保点显示在边上面
            all_points_actor_->SetPosition(0, 0, 0.01);
            renderer_->AddActor(all_points_actor_);
            Logging::info("Points actor added to renderer");
            Logging::info("Points actor position: {}, {}, {}",
                          all_points_actor_->GetPosition()[0],
                          all_points_actor_->GetPosition()[1],
                          all_points_actor_->GetPosition()[2]);
        }

        // 输出点和单元的数量
        Logging::info("Loaded grid with {} points and {} cells", grid_->GetNumberOfPoints(), grid_->GetNumberOfCells());
        if (all_points_actor_)
        {
            Logging::info("Points actor visibility: {}", all_points_actor_->GetVisibility() ? "on" : "off");
            Logging::info("Points actor point size: {}", all_points_actor_->GetProperty()->GetPointSize());
        }

        renderer_->ResetCamera();
        render_window_->Render();
    }
    else
    {
        Logging::error("Failed to load or parse INP file: {}", filename);
    }
}

void VtkWidget::openAbaqusFile(const QString &filename)
{
    if (!filename.isEmpty())
    {
        loadInpFile(filename, FileType::Abaqus);
    }
}

void VtkWidget::openGmshFile(const QString &filename)
{
    if (!filename.isEmpty())
    {
        loadInpFile(filename, FileType::Gmsh);
    }
}

void VtkWidget::onCellSelected(size_t cellId, int x, int y)
{
    if (cellId != -1)
    {
        auto cell = grid_->GetCell(cellId);
        auto pointIds = cell->GetPointIds();
        QString tooltipText = QString("Cell ID: %1\n").arg(cellId + 1);
        for (size_t i = 0; i < pointIds->GetNumberOfIds(); i++)
        {
            auto pointId = pointIds->GetId(i);
            auto point = grid_->GetPoint(pointId);
            tooltipText.append(QString("Point ID: %1, Point: %2, %3, %4\n")
                                   .arg(pointId + 1)
                                   .arg(point[0])
                                   .arg(point[1])
                                   .arg(point[2]));
        }
        auto cell_property = project_model_->getCellProperty(cellId + 1);
        tooltipText.append(QString("Cell Property: %1\n").arg(cell_property.toString()));

        ui_.tooltip_widget->setText(tooltipText);
        ui_.tooltip_widget->adjustSize();
        ui_.tooltip_widget->setGeometry(QRect(QCursor::pos(), ui_.tooltip_widget->size()));
        ui_.tooltip_widget->show();
        QTimer::singleShot(1000, this, [this]() { ui_.tooltip_widget->close(); });
    }
}

void VtkWidget::onPointSelected(size_t pointId, int x, int y)
{
    Logging::info("Point ID: {}, x: {}, y: {}", pointId, x, y);
    if (pointId != -1)
    {
        auto point = grid_->GetPoint(pointId);
        QString tooltipText = QString("Point ID: %1\n").arg(pointId + 1);
        tooltipText.append(QString("Point: %1, %2, %3\n").arg(point[0]).arg(point[1]).arg(point[2]));

        auto boundary_condition = project_model_->getBoundaryCondition(pointId + 1);
        tooltipText.append(QString("Boundary Condition: %1\n").arg(boundary_condition.toString()));

        ui_.tooltip_widget->setText(tooltipText);
        ui_.tooltip_widget->adjustSize();
        ui_.tooltip_widget->setGeometry(QRect(QCursor::pos(), ui_.tooltip_widget->size()));
        ui_.tooltip_widget->show();
        QTimer::singleShot(1000, this, [this]() { ui_.tooltip_widget->close(); });
    }
}

void VtkWidget::onChangeCellColor() { Logging::info("Change cell color"); }

void VtkWidget::onChangePointColor() { Logging::info("Change point color"); }

vtkSmartPointer<vtkIntArray> VtkWidget::generateCellColors(const QMap<QString, std::vector<int>> &elset_sets)
{
    auto cell_colors = vtkSmartPointer<vtkIntArray>::New();
    cell_colors->SetName("CellColors");
    cell_colors->SetNumberOfComponents(1);
    cell_colors->SetNumberOfTuples(grid_->GetNumberOfCells());

    size_t size = elset_sets.size();
    for (size_t i = 0; i < size; i++)
    {
        auto elset = elset_sets.keys()[i];
        auto ids = elset_sets[elset];
        for (auto id : ids)
        {
            cell_colors->SetValue(id - 1, i);
        }
    }
    // cell_colors->PrintSelf(std::cout, vtkIndent());
    return cell_colors;
}

vtkSmartPointer<vtkIntArray> VtkWidget::generatePointColors(const QMap<QString, std::vector<int>> &nset_sets)
{
    auto point_colors = vtkSmartPointer<vtkIntArray>::New();
    point_colors->SetName("PointColors");
    point_colors->SetNumberOfComponents(1);
    point_colors->SetNumberOfTuples(grid_->GetNumberOfPoints());

    // 初始化所有点的颜色为默认值0
    for (vtkIdType i = 0; i < grid_->GetNumberOfPoints(); ++i)
    {
        point_colors->SetValue(i, 0);
    }

    size_t size = nset_sets.size();
    for (size_t i = 0; i < size; i++)
    {
        auto nset = nset_sets.keys()[i];
        auto ids = nset_sets[nset];
        nset_index_map_[nset] = i; // 更新节点集合的索引映射

        // 为每个节点集合中的点分配对应的颜色索引
        for (auto id : ids)
        {
            if (id > 0 && id <= grid_->GetNumberOfPoints())
            {
                point_colors->SetValue(id - 1, i);
            }
        }
    }

    return point_colors;
}

void VtkWidget::setCellColor(size_t cellId, const QColor &color)
{
    if (!grid_ || cellId >= grid_->GetNumberOfCells())
        return;

    auto cell_colors = vtkIntArray::SafeDownCast(grid_->GetCellData()->GetScalars());
    if (!cell_colors)
        return;

    int colorIndex = cell_colors->GetValue(cellId);
    cell_lut_->SetTableValue(colorIndex, color.redF(), color.greenF(), color.blueF(), color.alphaF());

    render_window_->Render();
}

void VtkWidget::setPointColor(size_t pointId, const QColor &color)
{
    if (!grid_ || pointId >= grid_->GetNumberOfPoints())
        return;

    auto point_colors = vtkIntArray::SafeDownCast(grid_->GetPointData()->GetScalars());
    if (!point_colors)
        return;

    int colorIndex = point_colors->GetValue(pointId);
    point_lut_->SetTableValue(colorIndex, color.redF(), color.greenF(), color.blueF(), color.alphaF());

    render_window_->Render();
}

void VtkWidget::setCellSetColor(const QString &setName, const QColor &color)
{
    if (!elset_index_map_.contains(setName))
        return;

    size_t colorIndex = elset_index_map_[setName];
    cell_lut_->SetTableValue(colorIndex, color.redF(), color.greenF(), color.blueF(), color.alphaF());

    render_window_->Render();
}

void VtkWidget::setNodeSetColor(const QString &setName, const QColor &color)
{
    if (!nset_index_map_.contains(setName))
        return;

    size_t colorIndex = nset_index_map_[setName];
    point_lut_->SetTableValue(colorIndex, color.redF(), color.greenF(), color.blueF(), color.alphaF());

    render_window_->Render();
}

void VtkWidget::updateElementProperty(QString element_id, const ElementProperty &prop)
{
    Logging::info("Update element property: {}", element_id);
    auto color = prop.color;
    setCellSetColor(element_id, color);
}

void VtkWidget::updateNodeProperty(QString node_id, const BoundaryCondition &prop)
{
    auto color = prop.color;
    setNodeSetColor(node_id, color);
    Logging::info("Update node property: {}", node_id);
}
