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

// DynamicModeller Logic includes
#include <vtkSlicerDynamicModellerLogic.h>

// DynamicModeller includes
#include "qSlicerDynamicModellerModule.h"
#include "qSlicerDynamicModellerModuleWidget.h"

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_ExtensionTemplate
class qSlicerDynamicModellerModulePrivate
{
public:
  qSlicerDynamicModellerModulePrivate();
};

//-----------------------------------------------------------------------------
// qSlicerDynamicModellerModulePrivate methods

//-----------------------------------------------------------------------------
qSlicerDynamicModellerModulePrivate::qSlicerDynamicModellerModulePrivate()
= default;

//-----------------------------------------------------------------------------
// qSlicerDynamicModellerModule methods

//-----------------------------------------------------------------------------
qSlicerDynamicModellerModule::qSlicerDynamicModellerModule(QObject* _parent)
  : Superclass(_parent)
  , d_ptr(new qSlicerDynamicModellerModulePrivate)
{
}

//-----------------------------------------------------------------------------
qSlicerDynamicModellerModule::~qSlicerDynamicModellerModule()
= default;

//-----------------------------------------------------------------------------
QString qSlicerDynamicModellerModule::helpText() const
{
  return "This module allows surface mesh editing using dynamic modelling rules and operations";
}

//-----------------------------------------------------------------------------
QString qSlicerDynamicModellerModule::acknowledgementText() const
{
  return "This work was partially funded by CANARIE's Research Software Program,"
    "OpenAnatomy, and Brigham and Women's Hospital through NIH grant R01MH112748.";
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModellerModule::contributors() const
{
  QStringList moduleContributors;
  moduleContributors << QString("Kyle Sunderland (PerkLab, Queen's)");
  return moduleContributors;
}

//-----------------------------------------------------------------------------
QIcon qSlicerDynamicModellerModule::icon() const
{
  return QIcon(":/Icons/DynamicModeller.png");
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModellerModule::categories() const
{
  return QStringList() << "Surface Models";
}

//-----------------------------------------------------------------------------
QStringList qSlicerDynamicModellerModule::dependencies() const
{
  return QStringList();
}

//-----------------------------------------------------------------------------
void qSlicerDynamicModellerModule::setup()
{
  this->Superclass::setup();
}

//-----------------------------------------------------------------------------
qSlicerAbstractModuleRepresentation* qSlicerDynamicModellerModule
::createWidgetRepresentation()
{
  return new qSlicerDynamicModellerModuleWidget;
}

//-----------------------------------------------------------------------------
vtkMRMLAbstractLogic* qSlicerDynamicModellerModule::createLogic()
{
  return vtkSlicerDynamicModellerLogic::New();
}
