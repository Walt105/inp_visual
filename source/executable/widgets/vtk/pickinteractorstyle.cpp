#include "pickinteractorstyle.h"
#include <QCursor>
#include <QToolTip>
#include <vtkActor.h>
#include <vtkAreaPicker.h>
#include <vtkCellPicker.h>
#include <vtkDataSet.h>
#include <vtkDataSetMapper.h>
#include <vtkExtractGeometry.h>
#include <vtkMapper.h>
#include <vtkMath.h>
#include <vtkObjectFactory.h>
#include <vtkPointPicker.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkUnstructuredGrid.h>

#include "utils/logging.h"
vtkStandardNewMacro(CellPickInteractorStyle);
vtkStandardNewMacro(PointPickInteractorStyle);

void CellPickInteractorStyle::OnLeftButtonDown()
{
    vtkSmartPointer<vtkCellPicker> picker = vtkSmartPointer<vtkCellPicker>::New();
    picker->SetTolerance(0.0005);

    int *clickPos = this->GetInteractor()->GetEventPosition();
    vtkRenderer *renderer = this->GetDefaultRenderer();
    if (renderer)
    {
        picker->Pick(clickPos[0], clickPos[1], 0, renderer);

        vtkIdType cellId = picker->GetCellId();
        if (cellId != -1)
        {
            vtkSmartPointer<vtkUnstructuredGrid> grid = vtkUnstructuredGrid::SafeDownCast(picker->GetDataSet());
            if (grid)
            {
                vtkCell *cell = grid->GetCell(cellId);
                if (cell)
                {
                    if (emitter_)
                    {
                        emitter_->cellSelected(static_cast<size_t>(cellId), clickPos[0], clickPos[1]);
                    }
                    if (highlight_actor_)
                    {
                        renderer->RemoveActor(highlight_actor_);
                        highlight_actor_ = nullptr;
                    }
                    // 创建一个新的UnstructuredGrid，只包含选中的单元格
                    vtkSmartPointer<vtkUnstructuredGrid> singleCellGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
                    singleCellGrid->Allocate(1);
                    singleCellGrid->InsertNextCell(cell->GetCellType(), cell->GetPointIds());

                    // 设置点数据
                    singleCellGrid->SetPoints(grid->GetPoints());

                    // 高亮单元格
                    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
                    mapper->SetInputData(singleCellGrid);

                    highlight_actor_ = vtkSmartPointer<vtkActor>::New();
                    highlight_actor_->SetMapper(mapper);
                    highlight_actor_->GetProperty()->SetColor(0, 1, 0); // 设置高亮颜色为绿色
                    highlight_actor_->GetProperty()->SetEdgeVisibility(true);
                    highlight_actor_->GetProperty()->SetLineWidth(2);

                    renderer->AddActor(highlight_actor_);
                    renderer->GetRenderWindow()->Render();
                }
            }
        }
    }
    else
    {
        Logging::error("Error: No renderer available for picking.");
    }

    vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
}

void CellPickInteractorStyle::OnLeftButtonUp()
{
    vtkInteractorStyleRubberBand2D::OnLeftButtonUp();
    // vtkRenderer *renderer = this->GetDefaultRenderer();

    // if (highlight_actor_)
    // {
    //     renderer->RemoveActor(highlight_actor_);
    //     highlight_actor_ = nullptr;
    // }

    // // 获取框选区域
    // vtkPlanes *frustum = static_cast<vtkAreaPicker *>(this->GetInteractor()->GetPicker())->GetFrustum();
    // if (!frustum)
    // {
    //     return;
    // }

    // // 获取数据集
    // vtkProp *prop = this->GetCurrentRenderer()->GetViewProps()->GetLastProp();
    // vtkActor *actor = vtkActor::SafeDownCast(prop);
    // if (!actor)
    // {
    //     return;
    // }
    // vtkDataSet *input = vtkDataSet::SafeDownCast(actor->GetMapper()->GetInput());
    // if (!input)
    // {
    //     return;
    // }

    // // 创建提取器
    // vtkSmartPointer<vtkExtractGeometry> extractGeometry = vtkSmartPointer<vtkExtractGeometry>::New();
    // extractGeometry->SetImplicitFunction(vtkImplicitFunction::SafeDownCast(frustum));
    // extractGeometry->SetInputData(input);
    // extractGeometry->Update();

    // // 获取框选区域内的单元格
    // vtkUnstructuredGrid *output = extractGeometry->GetOutput();
    // if (!output || output->GetNumberOfCells() == 0)
    // {
    //     return;
    // }

    // // 高亮显示选中的单元格
    // vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    // mapper->SetInputData(output);

    // highlight_actor_ = vtkSmartPointer<vtkActor>::New();
    // highlight_actor_->SetMapper(mapper);
    // highlight_actor_->GetProperty()->SetColor(0, 1, 0);
    // highlight_actor_->GetProperty()->SetEdgeVisibility(true);
    // highlight_actor_->GetProperty()->SetLineWidth(2);

    // renderer->AddActor(highlight_actor_);
    // renderer->GetRenderWindow()->Render();

    // // 发送选中的单元格ID
    // if (emitter_)
    // {
    //     int *pos = this->GetInteractor()->GetEventPosition();
    //     for (vtkIdType i = 0; i < output->GetNumberOfCells(); i++)
    //     {
    //         emitter_->cellSelected(static_cast<size_t>(i), pos[0], pos[1]);
    //     }
    // }
}

void CellPickInteractorStyle::setEmitter(SignalEmitter *emitter) { emitter_ = emitter; }

void PointPickInteractorStyle::OnLeftButtonDown()
{
    const double epsilon = 0.005;

    // 使用vtkPointPicker来拾取点
    vtkSmartPointer<vtkPointPicker> pointPicker = vtkSmartPointer<vtkPointPicker>::New();
    pointPicker->SetTolerance(epsilon);

    // 同时使用vtkPropPicker来拾取actor
    vtkSmartPointer<vtkPropPicker> propPicker = vtkSmartPointer<vtkPropPicker>::New();

    int *clickPos = this->GetInteractor()->GetEventPosition();
    vtkRenderer *renderer = this->GetDefaultRenderer();

    if (!renderer)
    {
        Logging::error("Error: No renderer available for picking.");
        vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
        return;
    }

    // 首先尝试使用pointPicker拾取点
    if (pointPicker->Pick(clickPos[0], clickPos[1], 0, renderer))
    {
        vtkIdType pointId = pointPicker->GetPointId();
        if (pointId != -1)
        {
            vtkDataSet *dataSet = pointPicker->GetDataSet();
            if (dataSet)
            {
                double *point = dataSet->GetPoint(pointId);

                // 检查是否与上次选择的点相同
                bool isSamePoint = highlight_actor_ &&
                                   std::abs(highlight_actor_->GetPosition()[0] - point[0]) < epsilon &&
                                   std::abs(highlight_actor_->GetPosition()[1] - point[1]) < epsilon &&
                                   std::abs(highlight_actor_->GetPosition()[2] - point[2]) < epsilon;

                if (isSamePoint)
                {
                    vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
                    return;
                }

                // 移除旧的高亮actor
                if (highlight_actor_)
                {
                    renderer->RemoveActor(highlight_actor_);
                    highlight_actor_ = nullptr;
                }

                // 创建新的高亮actor
                vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
                sphereSource->SetCenter(point);
                sphereSource->SetRadius(0.1);
                sphereSource->Update();

                vtkSmartPointer<vtkPolyDataMapper> highlightMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
                highlightMapper->SetInputConnection(sphereSource->GetOutputPort());

                highlight_actor_ = vtkSmartPointer<vtkActor>::New();
                highlight_actor_->SetMapper(highlightMapper);
                highlight_actor_->GetProperty()->SetColor(1, 0, 0); // 红色
                highlight_actor_->SetPosition(0, 0, 0.02);          // 确保高亮点在其他点的上面

                renderer->AddActor(highlight_actor_);
                renderer->GetRenderWindow()->Render();

                // 发送点选择信号
                if (emitter_)
                {
                    emitter_->pointSelected(static_cast<size_t>(pointId), clickPos[0], clickPos[1]);
                }

                vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
                return;
            }
        }
    }

    // 如果pointPicker没有找到点，尝试使用propPicker
    if (propPicker->Pick(clickPos[0], clickPos[1], 0, renderer))
    {
        // 获取拾取到的actor
        vtkActor *pickedActor = vtkActor::SafeDownCast(propPicker->GetActor());
        if (!pickedActor)
        {
            Logging::info("No actor found at pick position");
            vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
            return;
        }

        // 获取拾取位置的世界坐标
        double *worldPos = propPicker->GetPickPosition();
        Logging::info("Picked position: ({}, {}, {})", worldPos[0], worldPos[1], worldPos[2]);

        // 现在我们需要找到最接近拾取位置的点
        vtkMapper *mapper = pickedActor->GetMapper();
        if (!mapper)
        {
            Logging::error("Error: Picked actor has no mapper");
            vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
            return;
        }

        vtkDataSet *dataSet = mapper->GetInput();
        if (!dataSet)
        {
            Logging::error("Error: Mapper has no input dataset");
            vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
            return;
        }

        // 找到最接近拾取位置的点
        vtkIdType closestPointId = -1;
        double minDist = VTK_DOUBLE_MAX;

        for (vtkIdType i = 0; i < dataSet->GetNumberOfPoints(); i++)
        {
            double point[3];
            dataSet->GetPoint(i, point);

            double dist = sqrt(vtkMath::Distance2BetweenPoints(worldPos, point));
            if (dist < minDist)
            {
                minDist = dist;
                closestPointId = i;
            }
        }

        if (closestPointId == -1)
        {
            Logging::error("Error: Could not find closest point");
            vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
            return;
        }

        // 获取最接近的点坐标
        double point[3];
        dataSet->GetPoint(closestPointId, point);

        // 检查是否与上次选择的点相同
        bool isSamePoint = highlight_actor_ && std::abs(highlight_actor_->GetPosition()[0] - point[0]) < epsilon &&
                           std::abs(highlight_actor_->GetPosition()[1] - point[1]) < epsilon &&
                           std::abs(highlight_actor_->GetPosition()[2] - point[2]) < epsilon;

        if (isSamePoint)
        {
            vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
            return;
        }

        // 移除旧的高亮actor
        if (highlight_actor_)
        {
            renderer->RemoveActor(highlight_actor_);
            highlight_actor_ = nullptr;
        }

        // 创建新的高亮actor
        vtkSmartPointer<vtkSphereSource> sphereSource = vtkSmartPointer<vtkSphereSource>::New();
        sphereSource->SetCenter(point);
        sphereSource->SetRadius(0.1);
        sphereSource->Update();

        vtkSmartPointer<vtkPolyDataMapper> highlightMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        highlightMapper->SetInputConnection(sphereSource->GetOutputPort());

        highlight_actor_ = vtkSmartPointer<vtkActor>::New();
        highlight_actor_->SetMapper(highlightMapper);
        highlight_actor_->GetProperty()->SetColor(1, 0, 0); // 红色
        highlight_actor_->SetPosition(0, 0, 0.02);          // 确保高亮点在其他点的上面

        renderer->AddActor(highlight_actor_);
        renderer->GetRenderWindow()->Render();

        // 发送点选择信号
        if (emitter_)
        {
            emitter_->pointSelected(static_cast<size_t>(closestPointId), clickPos[0], clickPos[1]);
        }
    }
    else
    {
        Logging::info("No actor or point picked at position ({}, {})", clickPos[0], clickPos[1]);
    }

    vtkInteractorStyleRubberBand2D::OnLeftButtonDown();
}

void PointPickInteractorStyle::OnLeftButtonUp() { vtkInteractorStyleRubberBand2D::OnLeftButtonUp(); }
void PointPickInteractorStyle::setEmitter(SignalEmitter *emitter) { emitter_ = emitter; }
