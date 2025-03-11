#pragma once

#include <QColor>
#include <QMap>
#include <QObject>
#include <QVector>

enum class LengthUnit : int
{
    mm = 0,
    m = 1,
    km = 2,
};

enum class MassUnit : int
{
    g = 0,
    kg = 1,
    t = 2,
};

enum class TimeUnit : int
{
    us = 0,
    ms = 1,
    s = 2,
};

struct UnitSettings
{
    LengthUnit length_unit{LengthUnit::m};
    MassUnit mass_unit{MassUnit::kg};
    TimeUnit time_unit{TimeUnit::s};
};

// 控制参数设置
struct ControlSettings
{
    int time_steps{100000};     // 时间步数
    int output_frequency{1000}; // 输出频率
    int min_element_size{1};    // 最小单元尺寸
    double time_step{5e-6};     // 时间步长
    double gravity{1000.0};     // 重力加速度
};

// 单元属性设置
struct ElementProperty
{
    QString id{"element"};                 // 单元ID
    int type{5};                           // 单元类型
    double youngs_modulus{3.53e+10};       // 杨氏模量
    double poisson_ratio{0.24};            // 泊松比
    double friction_coef{1.27};            // 摩擦系数
    double density{2537.0};                // 密度
    double viscous_damping{2.37e+7};       // 粘滞阻尼
    double normal_stiffness{3.53e+11};     // 法向刚度
    double tangential_stiffness{3.53e+11}; // 切向刚度
    QColor color{Qt::blue};                // 颜色

    QString toString() const
    {
        return QString("ID: %1, Type: %2, E: %3, v: %4, mu: %5, rho: %6, eta: %7, kn: %8, kt: %9")
            .arg(id)
            .arg(type)
            .arg(youngs_modulus)
            .arg(poisson_ratio)
            .arg(friction_coef)
            .arg(density)
            .arg(viscous_damping)
            .arg(normal_stiffness)
            .arg(tangential_stiffness);
    }
};

// 边界条件设置
struct BoundaryCondition
{
    QString id{"boundary"};         // 边界ID
    int x_direction_type{3};        // X方向边界条件类型
    double x_direction_size{0.0};   // X方向边界条件大小
    int y_direction_type{3};        // Y方向边界条件类型
    double y_direction_size{0.0};   // Y方向边界条件大小
    double unit_surface_force{0.0}; // 单元面力
    double unit_shear_force{0.0};   // 单元剪引力
    QColor color{Qt::yellow};       // 颜色

    QString toString() const
    {
        return QString("ID: %1, Node Attribute: %2, X Direction Type: %3, X Direction Size: %4, Y Direction Type: %5, "
                       "Y Direction Size: %6, Unit Surface Force: %7, Unit Shear Force: %8")
            .arg(id)
            .arg(x_direction_type)
            .arg(x_direction_size)
            .arg(y_direction_type)
            .arg(y_direction_size)
            .arg(unit_surface_force)
            .arg(unit_shear_force);
    }
};

class ProjectModel : public QObject
{
    Q_OBJECT
public:
    ProjectModel(QObject *parent = nullptr);
    ~ProjectModel() override;

    // Unit settings getters/setters
    LengthUnit lengthUnit() const { return unit_settings_.length_unit; }
    MassUnit massUnit() const { return unit_settings_.mass_unit; }
    TimeUnit timeUnit() const { return unit_settings_.time_unit; }
    void setLengthUnit(LengthUnit unit);
    void setMassUnit(MassUnit unit);
    void setTimeUnit(TimeUnit unit);

    // Control settings getters/setters
    int timeSteps() const { return control_settings_.time_steps; }
    int outputFrequency() const { return control_settings_.output_frequency; }
    int minElementSize() const { return control_settings_.min_element_size; }
    double timeStep() const { return control_settings_.time_step; }
    double gravity() const { return control_settings_.gravity; }

    void setTimeSteps(int steps);
    void setOutputFrequency(int freq);
    void setMinElementSize(int size);
    void setTimeStep(double step);
    void setGravity(double gravity);

    // Element properties management
    const QVector<ElementProperty> &elementProperties() const { return element_properties_; }
    void addElementProperty(const ElementProperty &prop);
    void updateElementProperty(QString element_id, const ElementProperty &prop);
    void removeElementProperty(QString element_id);

    // Boundary conditions management
    const QVector<BoundaryCondition> &boundaryConditions() const { return boundary_conditions_; }
    void addBoundaryCondition(const BoundaryCondition &condition);
    void updateBoundaryCondition(QString boundary_id, const BoundaryCondition &condition);
    void removeBoundaryCondition(QString boundary_id);

    // Node sets management
    const QMap<QString, std::vector<int>> &nodeSets() const { return node_sets_; }
    void setNodeSets(const QMap<QString, std::vector<int>> &node_sets);
    BoundaryCondition getBoundaryCondition(int node_id) const;

    // Element sets management
    const QMap<QString, std::vector<int>> &elementSets() const { return element_sets_; }
    void setElementSets(const QMap<QString, std::vector<int>> &element_sets);
    ElementProperty getCellProperty(int cell_id) const;
signals:
    // Unit settings signals
    void lengthUnitChanged(LengthUnit unit);
    void massUnitChanged(MassUnit unit);
    void timeUnitChanged(TimeUnit unit);
    void unitSettingsChanged();

    // Control settings signals
    void timeStepsChanged(int steps);
    void outputFrequencyChanged(int freq);
    void minElementSizeChanged(int size);
    void timeStepChanged(double step);
    void gravityChanged(double gravity);
    void controlSettingsChanged();

    // Element properties signals
    void elementPropertyAdded(const ElementProperty &prop);
    void elementPropertyUpdated(QString element_id, const ElementProperty &prop);
    void elementPropertyRemoved(QString element_id);

    // Boundary conditions signals
    void boundaryConditionAdded(const BoundaryCondition &condition);
    void boundaryConditionUpdated(QString boundary_id, const BoundaryCondition &condition);
    void boundaryConditionRemoved(QString boundary_id);

    // Node sets signals
    void nodeSetsChanged();

    // Element sets signals
    void elementSetsChanged();

private:
    UnitSettings unit_settings_;
    ControlSettings control_settings_;
    QVector<ElementProperty> element_properties_;
    QVector<BoundaryCondition> boundary_conditions_;
    QMap<QString, std::vector<int>> node_sets_;
    QMap<QString, std::vector<int>> element_sets_;
};
