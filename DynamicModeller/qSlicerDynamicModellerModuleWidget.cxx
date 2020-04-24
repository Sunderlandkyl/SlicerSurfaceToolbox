/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, Cancer
  Care Ontario, OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.

==============================================================================*/

// Qt includes
#include <QCheckBox>
#include <QDebug>
#include <QLineEdit>
#include <QSpinBox>

// ctk includes
#include <ctkDoubleSpinBox.h>

// SlicerQt includes
#include "qSlicerDynamicModellerModuleWidget.h"
#include "ui_qSlicerDynamicModellerModuleWidget.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLNode.h>

// VTK includes
#include <vtkStringArray.h>

// DynamicModeller Logic includes
#include <vtkSlicerDynamicModellerLogic.h>
#include <vtkSlicerDynamicModellerRuleFactory.h>

// DynamicModeller MRML includes
#include <vtkMRMLDynamicModellerNode.h>

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerDynamicModellerModuleWidgetPrivate: public Ui_qSlicerDynamicModellerModuleWidget
{
public:
  qSlicerDynamicModellerModuleWidgetPrivate();
  vtkWeakPointer<vtkMRMLDynamicModellerNode> DynamicModellerNode{ nullptr };

  std::string CurrentRuleName;
};

//-----------------------------------------------------------------------------
// qSlicerDynamicModellerModuleWidgetPrivate methods

//-----------------------------------------------------------------------------
qSlicerDynamicModellerModuleWidgetPrivate::qSlicerDynamicModellerModuleWidgetPrivate()
= default;

//-----------------------------------------------------------------------------
// qSlicerDynamicModellerModuleWidget methods

//-----------------------------------------------------------------------------
qSlicerDynamicModellerModuleWidget::qSlicerDynamicModellerModuleWidget(QWidget* _parent)
  : Superclass( _parent )
  , d_ptr( new qSlicerDynamicModellerModuleWidgetPrivate )
{
}

//-----------------------------------------------------------------------------
qSlicerDynamicModellerModuleWidget::~qSlicerDynamicModellerModuleWidget()
= default;

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::setup()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  d->setupUi(this);
  this->Superclass::setup();

  connect(d->ParameterNodeComboBox, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
    this, SLOT(onParameterNodeChanged(vtkMRMLNode*)));

  d->RuleComboBox->clear();
  vtkSlicerDynamicModellerRuleFactory* ruleFactory = vtkSlicerDynamicModellerRuleFactory::GetInstance();
  if (ruleFactory)
    {
    std::vector<std::string> ruleNames = ruleFactory->GetDynamicModellerRuleNames();
    for (std::string ruleName : ruleNames)
      {
      d->RuleComboBox->addItem(ruleName.c_str(), ruleName.c_str());
      }
    }

  connect(d->RuleComboBox, SIGNAL(currentIndexChanged(int)),
    this, SLOT(updateMRMLFromWidget()));
  connect(d->ApplyButton, SIGNAL(checkStateChanged(Qt::CheckState)),
    this, SLOT(onApplyButtonClicked()));
  connect(d->ApplyButton, SIGNAL(clicked()),
    this, SLOT(onApplyButtonClicked()));
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::onParameterNodeChanged(vtkMRMLNode* node)
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  if (!this->mrmlScene() || this->mrmlScene()->IsBatchProcessing())
    {
    return;
    }

  vtkMRMLDynamicModellerNode* meshModifyNode = vtkMRMLDynamicModellerNode::SafeDownCast(node);
  qvtkReconnect(d->DynamicModellerNode, meshModifyNode, vtkCommand::ModifiedEvent, this, SLOT(updateWidgetFromMRML()));

  d->DynamicModellerNode = meshModifyNode;
  this->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::resetInputWidgets()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  vtkSlicerDynamicModellerRule* rule = nullptr;
  vtkSlicerDynamicModellerLogic* meshModifyLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  if (meshModifyLogic && d->DynamicModellerNode)
    {
    rule = meshModifyLogic->GetDynamicModellerRule(d->DynamicModellerNode);
    }

  QList<QWidget*> widgets = d->InputNodesCollapsibleButton->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
  for (QWidget* widget : widgets)
    {
    widget->deleteLater();
    }

  if (rule == nullptr || rule->GetNumberOfInputNodes() == 0)
    {
    d->InputNodesCollapsibleButton->setEnabled(false);
    return;
    }
  d->InputNodesCollapsibleButton->setEnabled(true);

  QWidget* inputNodesWidget = new QWidget();
  QFormLayout* inputNodesLayout = new QFormLayout();
  inputNodesWidget->setLayout(inputNodesLayout);
  d->InputNodesCollapsibleButton->layout()->addWidget(inputNodesWidget);
  for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
    {
    std::string name = rule->GetNthInputNodeName(i);
    std::string description = rule->GetNthInputNodeDescription(i);
    std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
    vtkStringArray* classNameArray = rule->GetNthInputNodeClassNames(i);
    QStringList classNames;
    for (int i = 0; i < classNameArray->GetNumberOfValues(); ++i)
      {
      vtkStdString className = classNameArray->GetValue(i);
      classNames << className.c_str();
      }

    QLabel* nodeLabel = new QLabel();
    std::stringstream labelTextSS;
    labelTextSS << name << ":";
    std::string labelText = labelTextSS.str();
    nodeLabel->setText(labelText.c_str());
    nodeLabel->setToolTip(description.c_str());

    qMRMLNodeComboBox* nodeSelector = new qMRMLNodeComboBox();
    nodeSelector->setNodeTypes(classNames);
    nodeSelector->setToolTip(description.c_str());
    nodeSelector->setNoneEnabled(true);
    nodeSelector->setMRMLScene(this->mrmlScene());
    nodeSelector->setProperty("ReferenceRole", referenceRole.c_str());
    nodeSelector->setAddEnabled(false);
    nodeSelector->setRemoveEnabled(false);
    nodeSelector->setRenameEnabled(false);

    inputNodesLayout->addRow(nodeLabel, nodeSelector);

    connect(nodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
      this, SLOT(updateMRMLFromWidget()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::resetParameterWidgets()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  vtkSlicerDynamicModellerLogic* meshModifyLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  vtkSlicerDynamicModellerRule* rule = nullptr;
  if (meshModifyLogic && d->DynamicModellerNode)
    {
    rule = meshModifyLogic->GetDynamicModellerRule(d->DynamicModellerNode);
    }

  QList<QWidget*> widgets = d->ParametersCollapsibleButton->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
  for (QWidget* widget : widgets)
    {
    widget->deleteLater();
    }

  if (rule == nullptr || rule->GetNumberOfInputParameters() == 0)
    {
    d->ParametersCollapsibleButton->setEnabled(false);
    d->ParametersCollapsibleButton->setVisible(false);
    return;
    }
  d->ParametersCollapsibleButton->setEnabled(true);
  d->ParametersCollapsibleButton->setVisible(true);

  QWidget* inputParametersWidget = new QWidget();
  QFormLayout* inputParametersLayout = new QFormLayout();
  inputParametersWidget->setLayout(inputParametersLayout);
  d->ParametersCollapsibleButton->layout()->addWidget(inputParametersWidget);
  for (int i = 0; i < rule->GetNumberOfInputParameters(); ++i)
    {
    std::string name = rule->GetNthInputParameterName(i);
    std::string description = rule->GetNthInputParameterDescription(i);
    std::string attributeName = rule->GetNthInputParameterAttributeName(i);
    int type = rule->GetNthInputParameterType(i);

    QLabel* parameterLabel = new QLabel();
    std::stringstream labelTextSS;
    labelTextSS << name << ":";
    std::string labelText = labelTextSS.str();
    parameterLabel->setText(labelText.c_str());
    parameterLabel->setToolTip(description.c_str());

    QWidget* parameterSelector = nullptr;
    if (type == vtkSlicerDynamicModellerRule::PARAMETER_BOOL)
      {
      QCheckBox* checkBox = new QCheckBox();
      parameterSelector = checkBox;
      connect(checkBox, SIGNAL(stateChanged(int)),
        this, SLOT(updateMRMLFromWidget()));
      }
    else if (type == vtkSlicerDynamicModellerRule::PARAMETER_INT)
      {
      QSpinBox* spinBox = new QSpinBox();
      connect(spinBox, SIGNAL(valueChanged(int)),
        this, SLOT(updateMRMLFromWidget()));
      parameterSelector = spinBox;
      }
    else if (type == vtkSlicerDynamicModellerRule::PARAMETER_DOUBLE)
      {
      ctkDoubleSpinBox* doubleSpinBox = new ctkDoubleSpinBox();
      connect(doubleSpinBox, SIGNAL(valueChanged(double)),
        this, SLOT(updateMRMLFromWidget()));
      parameterSelector = doubleSpinBox;
      }
    else
      {
      QLineEdit* lineEdit = new QLineEdit();
      connect(lineEdit, SIGNAL(textChanged(QString)),
          this, SLOT(updateMRMLFromWidget()));
      parameterSelector = lineEdit;
      }

    parameterSelector->setObjectName(attributeName.c_str());
    parameterSelector->setToolTip(description.c_str());
    parameterSelector->setProperty("AttributeName", attributeName.c_str());
    inputParametersLayout->addRow(parameterLabel, parameterSelector);
    }
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::resetOutputWidgets()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  vtkSlicerDynamicModellerLogic* meshModifyLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  vtkSlicerDynamicModellerRule* rule = nullptr;
  if (meshModifyLogic && d->DynamicModellerNode)
    {
    rule = meshModifyLogic->GetDynamicModellerRule(d->DynamicModellerNode);
    }

  QList<QWidget*> widgets = d->OutputNodesCollapsibleButton->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
  for (QWidget* widget : widgets)
    {
    widget->deleteLater();
    }

  if (rule == nullptr || rule->GetNumberOfOutputNodes() == 0)
    {
    d->OutputNodesCollapsibleButton->setEnabled(false);
    return;
    }
  d->OutputNodesCollapsibleButton->setEnabled(true);

  QWidget* outputNodesWidget = new QWidget();
  QFormLayout* outputNodesLayout = new QFormLayout();
  outputNodesWidget->setLayout(outputNodesLayout);
  d->OutputNodesCollapsibleButton->layout()->addWidget(outputNodesWidget);
  for (int i = 0; i < rule->GetNumberOfOutputNodes(); ++i)
    {
    std::string name = rule->GetNthOutputNodeName(i);
    std::string description = rule->GetNthOutputNodeDescription(i);
    std::string referenceRole = rule->GetNthOutputNodeReferenceRole(i);
    vtkStringArray* classNameArray = rule->GetNthOutputNodeClassNames(i);
    QStringList classNames;
    for (int i = 0; i < classNameArray->GetNumberOfValues(); ++i)
      {
      vtkStdString className = classNameArray->GetValue(i);
      classNames << className.c_str();
      }

    QLabel* nodeLabel = new QLabel();
    std::stringstream labelTextSS;
    labelTextSS << name << ":";
    std::string labelText = labelTextSS.str();
    nodeLabel->setText(labelText.c_str());
    nodeLabel->setToolTip(description.c_str());

    qMRMLNodeComboBox* nodeSelector = new qMRMLNodeComboBox();
    nodeSelector->setNodeTypes(classNames);
    nodeSelector->setToolTip(description.c_str());
    nodeSelector->setNoneEnabled(true);
    nodeSelector->setMRMLScene(this->mrmlScene());
    nodeSelector->setProperty("ReferenceRole", referenceRole.c_str());
    nodeSelector->setAddEnabled(true);
    nodeSelector->setRemoveEnabled(true);
    nodeSelector->setRenameEnabled(true);

    outputNodesLayout->addRow(nodeLabel, nodeSelector);

    connect(nodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
      this, SLOT(updateMRMLFromWidget()));
    }
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::updateInputWidgets()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  if (!d->DynamicModellerNode)
    {
    return;
    }

  QList<qMRMLNodeComboBox*> inputNodeSelectors = d->InputNodesCollapsibleButton->findChildren<qMRMLNodeComboBox*>();
  for (qMRMLNodeComboBox* inputNodeSelector : inputNodeSelectors)
    {
    QString referenceRole = inputNodeSelector->property("ReferenceRole").toString();
    vtkMRMLNode* referenceNode = d->DynamicModellerNode->GetNodeReference(referenceRole.toUtf8());
    bool wasBlocking = inputNodeSelector->blockSignals(true);
    inputNodeSelector->setCurrentNode(referenceNode);
    inputNodeSelector->blockSignals(wasBlocking);
    }
}


//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::updateParameterWidgets()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  vtkSlicerDynamicModellerLogic* meshModifyLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  vtkSlicerDynamicModellerRule* rule = nullptr;
  if (meshModifyLogic && d->DynamicModellerNode)
    {
    rule = meshModifyLogic->GetDynamicModellerRule(d->DynamicModellerNode);
    }
  if (!d->DynamicModellerNode || rule == nullptr || rule->GetNumberOfInputParameters() == 0)
    {
    return;
    }

  for (int i = 0; i < rule->GetNumberOfInputParameters(); ++i)
    {
    std::string name = rule->GetNthInputParameterName(i);
    std::string description = rule->GetNthInputParameterDescription(i);
    std::string attributeName = rule->GetNthInputParameterAttributeName(i);
    vtkVariant value = rule->GetNthInputParameterValue(i, d->DynamicModellerNode);
    int type = rule->GetNthInputParameterType(i);

    QWidget* parameterSelector = d->ParametersCollapsibleButton->findChild<QWidget*>(attributeName.c_str());
    if (type == vtkSlicerDynamicModellerRule::PARAMETER_BOOL)
      {
      QCheckBox* checkBox = qobject_cast<QCheckBox*>(parameterSelector);
      if (!checkBox)
        {
        qCritical() << "Could not find widget for parameter " << name.c_str();
        continue;
        }

      bool wasBlocking = checkBox->blockSignals(true);
      bool checked = value.ToInt() != 0;
      checkBox->setChecked(checked);
      checkBox->blockSignals(wasBlocking);
      }
    else if (type == vtkSlicerDynamicModellerRule::PARAMETER_INT)
      {
      QSpinBox* spinBox = qobject_cast<QSpinBox*>(parameterSelector);
      if (!spinBox)
        {
        qCritical() << "Could not find widget for parameter " << name.c_str();
        continue;
        }
      bool wasBlocking = spinBox->blockSignals(true);
      spinBox->setValue(value.ToInt());
      spinBox->blockSignals(wasBlocking);
      }
    else if (type == vtkSlicerDynamicModellerRule::PARAMETER_DOUBLE)
      {
      ctkDoubleSpinBox* doubleSpinBox = qobject_cast<ctkDoubleSpinBox*>(parameterSelector);
      if (!doubleSpinBox)
        {
        qCritical() << "Could not find widget for parameter " << name.c_str();
        continue;
        }

      bool wasBlocking = doubleSpinBox->blockSignals(true);
      doubleSpinBox->setValue(value.ToDouble());
      doubleSpinBox->blockSignals(wasBlocking);
      }
    else
      {
      QLineEdit* lineEdit = qobject_cast<QLineEdit*>(parameterSelector);
      if (!lineEdit)
        {
        qCritical() << "Could not find widget for parameter " << name.c_str();
        continue;
        }
      int cursorPosition = lineEdit->cursorPosition();
      bool wasBlocking = lineEdit->blockSignals(true);
      lineEdit->setText(QString::fromStdString(value.ToString()));
      lineEdit->setCursorPosition(cursorPosition);
      lineEdit->blockSignals(wasBlocking);
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::updateOutputWidgets()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  if (!d->DynamicModellerNode)
    {
    return;
    }

  QList< qMRMLNodeComboBox*> outputNodeSelectors = d->OutputNodesCollapsibleButton->findChildren<qMRMLNodeComboBox*>();
  for (qMRMLNodeComboBox* outputNodeSelector : outputNodeSelectors)
    {
    QString referenceRole = outputNodeSelector->property("ReferenceRole").toString();
    vtkMRMLNode* referenceNode = d->DynamicModellerNode->GetNodeReference(referenceRole.toUtf8());
    bool wasBlocking = outputNodeSelector->blockSignals(true);
    outputNodeSelector->setCurrentNode(referenceNode);
    outputNodeSelector->blockSignals(wasBlocking);
    }
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::updateWidgetFromMRML()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  vtkSlicerDynamicModellerLogic* meshModifyLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  vtkSlicerDynamicModellerRule* rule = nullptr;
  if (meshModifyLogic && d->DynamicModellerNode)
    {
    rule = meshModifyLogic->GetDynamicModellerRule(d->DynamicModellerNode);
    }
  d->ApplyButton->setEnabled(rule != nullptr);

  std::string ruleName = "";
  if (d->DynamicModellerNode && d->DynamicModellerNode->GetRuleName())
    {
    ruleName = d->DynamicModellerNode->GetRuleName();
    }

  if (ruleName != d->CurrentRuleName)
    {
    this->resetInputWidgets();
    this->resetParameterWidgets();
    this->resetOutputWidgets();
    d->CurrentRuleName = ruleName;
    }

  this->updateInputWidgets();
  this->updateParameterWidgets();
  this->updateOutputWidgets();

  int ruleIndex = 0;
  if (!d->DynamicModellerNode)
    {
    d->RuleComboBox->setEnabled(false);
    }
  else
    {
    ruleIndex = d->RuleComboBox->findData(ruleName.c_str());
    d->RuleComboBox->setEnabled(true);
    }

  bool wasBlocking = d->RuleComboBox->blockSignals(true);
  d->RuleComboBox->setCurrentIndex(ruleIndex);
  d->RuleComboBox->blockSignals(wasBlocking);

  wasBlocking = d->ApplyButton->blockSignals(true);
  if (d->DynamicModellerNode && d->DynamicModellerNode->GetContinuousUpdate())
    {
    d->ApplyButton->setCheckState(Qt::Checked);
    }
  else
    {
    d->ApplyButton->setCheckState(Qt::Unchecked);
    }
  d->ApplyButton->blockSignals(wasBlocking);
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::updateMRMLFromWidget()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  if (!d->DynamicModellerNode)
    {
    return;
    }

  MRMLNodeModifyBlocker blocker(d->DynamicModellerNode);

  QString ruleName = d->RuleComboBox->currentData().toString();
  d->DynamicModellerNode->SetRuleName(ruleName.toUtf8());
  d->DynamicModellerNode->SetContinuousUpdate(d->ApplyButton->checkState() == Qt::Checked);

  vtkSlicerDynamicModellerLogic* dynamicModellerLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  vtkSlicerDynamicModellerRule* rule = nullptr;
  if (dynamicModellerLogic && d->DynamicModellerNode)
    {
    rule = dynamicModellerLogic->GetDynamicModellerRule(d->DynamicModellerNode);
    }
  if (!rule)
    {
    return;
    }

  QList<qMRMLNodeComboBox*> inputNodeSelectors = d->InputNodesCollapsibleButton->findChildren<qMRMLNodeComboBox*>();
  int i = 0;
  for (qMRMLNodeComboBox* inputNodeSelector : inputNodeSelectors)
    {
    QString referenceRole = inputNodeSelector->property("ReferenceRole").toString();
    QString currentNodeID = inputNodeSelector->currentNodeID();
    d->DynamicModellerNode->SetAndObserveNodeReferenceID(referenceRole.toUtf8(), currentNodeID.toUtf8(), rule->GetNthInputNodeEvents(i));
    ++i;
    }

  QList<qMRMLNodeComboBox*> outputNodeSelectors = d->OutputNodesCollapsibleButton->findChildren<qMRMLNodeComboBox*>();
  for (qMRMLNodeComboBox* outputNodeSelector : outputNodeSelectors)
    {
    QString referenceRole = outputNodeSelector->property("ReferenceRole").toString();
    QString currentNodeID = outputNodeSelector->currentNodeID();
    d->DynamicModellerNode->SetNodeReferenceID(referenceRole.toUtf8(), currentNodeID.toUtf8());
    }

  d->ApplyButton->setToolTip("");
  d->ApplyButton->setCheckBoxUserCheckable(true);
  // If a node is selected in both the input and outputs, then disable continuous updates
  if (dynamicModellerLogic->HasCircularReference(d->DynamicModellerNode))
    {
    d->DynamicModellerNode->SetContinuousUpdate(false);
    d->ApplyButton->setToolTip("Output node detected in input. Continuous update is not availiable.");
    d->ApplyButton->setCheckBoxUserCheckable(false);    
    }

  QList<QWidget*> parameterSelectors = d->ParametersCollapsibleButton->findChildren<QWidget*>();
  for (QWidget* parameterSelector : parameterSelectors)
    {
    
    vtkVariant value;
    QCheckBox* checkBox = qobject_cast<QCheckBox*>(parameterSelector);
    if (checkBox)
      {
      value = checkBox->isChecked() ? 1 : 0;
      }

    QSpinBox* spinBox = qobject_cast<QSpinBox*>(parameterSelector);
    if (spinBox)
      {
      value = spinBox->value();
      }

    ctkDoubleSpinBox* doubleSpinBox = qobject_cast<ctkDoubleSpinBox*>(parameterSelector);
    if (doubleSpinBox)
      {
      value = doubleSpinBox->value();
      }

    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(parameterSelector);
    if (lineEdit)
      {
      std::string text = lineEdit->text().toStdString();
      value = text.c_str();
      }

    std::string attributeName = parameterSelector->property("AttributeName").toString().toStdString();
    if (attributeName != "")
      {
      d->DynamicModellerNode->SetAttribute(attributeName.c_str(), value.ToString());
      }
    }
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModuleWidget::onApplyButtonClicked()
{
  Q_D(qSlicerDynamicModellerModuleWidget);
  if (!d->DynamicModellerNode)
    {
    return;
    }
  this->updateMRMLFromWidget();

  /// Checkbox is checked. Should be handled by continuous update in logic
  if (d->ApplyButton->checkState() == Qt::Checked)
    {
    return;
    }

  /// Continuous update is off, trigger manual update.
  vtkSlicerDynamicModellerLogic* meshModifyLogic = vtkSlicerDynamicModellerLogic::SafeDownCast(this->logic());
  meshModifyLogic->RunDynamicModellerRule(d->DynamicModellerNode);
}
