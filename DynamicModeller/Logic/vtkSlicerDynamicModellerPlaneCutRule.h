/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

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

#ifndef __vtkSlicerDynamicModellerPlaneCutRule_h
#define __vtkSlicerDynamicModellerPlaneCutRule_h

#include "vtkSlicerDynamicModellerModuleLogicExport.h"

// VTK includes
#include <vtkObject.h>
#include <vtkSmartPointer.h>

// STD includes
#include <map>
#include <string>
#include <vector>

class vtkClipClosedSurface;
class vtkClipPolyData;
class vtkDataObject;
class vtkGeneralTransform;
class vtkGeometryFilter;
class vtkMRMLDynamicModellerNode;
class vtkPlane;
class vtkPolyData;
class vtkThreshold;
class vtkTransformPolyDataFilter;

#include "vtkSlicerDynamicModellerRule.h"

/// \brief Dynamic modelling rule for cutting a single surface mesh with planes
///
/// Has two node inputs (Plane and Surface), and two outputs (Positive/Negative direction surface segments)
class VTK_SLICER_DYNAMICMODELLER_MODULE_LOGIC_EXPORT vtkSlicerDynamicModellerPlaneCutRule : public vtkSlicerDynamicModellerRule
{
public:
  static vtkSlicerDynamicModellerPlaneCutRule* New();
  vtkSlicerDynamicModellerRule* CreateRuleInstance() override;
  vtkTypeMacro(vtkSlicerDynamicModellerPlaneCutRule, vtkSlicerDynamicModellerRule);

  /// Human-readable name of the mesh modification rule
  const char* GetName() override;

  /// Run the plane cut on the input model node
  bool RunInternal(vtkMRMLDynamicModellerNode* surfaceEditorNode) override;

  void CreateEndCap(vtkPolyData* surface);

protected:
  vtkSlicerDynamicModellerPlaneCutRule();
  ~vtkSlicerDynamicModellerPlaneCutRule() override;
  void operator=(const vtkSlicerDynamicModellerPlaneCutRule&);

protected:
  vtkSmartPointer<vtkTransformPolyDataFilter> InputModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        InputModelNodeToWorldTransform;

  vtkSmartPointer<vtkClipClosedSurface>       PlaneClipper;
  vtkSmartPointer<vtkPlane>                   Plane;

  vtkSmartPointer<vtkThreshold>               ThresholdFilter;
  vtkSmartPointer<vtkGeometryFilter>          GeometryFilter;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputPositiveModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputPositiveWorldToModelTransform;

  vtkSmartPointer<vtkTransformPolyDataFilter> OutputNegativeModelToWorldTransformFilter;
  vtkSmartPointer<vtkGeneralTransform>        OutputNegativeWorldToModelTransform;
};

#endif // __vtkSlicerDynamicModellerPlaneCutRule_h
