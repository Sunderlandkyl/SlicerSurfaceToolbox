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

#include "vtkSlicerDynamicModelerBoundaryCutRule.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkClipPolyData.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
#include <vtkConnectivityFilter.h>
#include <vtkCutter.h>
#include <vtkDataSetAttributes.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkFeatureEdges.h>
#include <vtkGeneralTransform.h>
#include <vtkImplicitBoolean.h>
#include <vtkIntArray.h>
#include <vtkObjectFactory.h>
#include <vtkPlane.h>
#include <vtkPlaneCollection.h>
#include <vtkReverseSense.h>
#include <vtkSelectPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStripper.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkRuleNewMacro(vtkSlicerDynamicModelerBoundaryCutRule);

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerBoundaryCutRule::vtkSlicerDynamicModelerBoundaryCutRule()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputCurveEvents;
  inputCurveEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputCurveEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputCurveEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputCurveClassNames;
  inputCurveClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  inputCurveClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  NodeInfo inputCurve(
    "Curve node",
    "Curve node to cut the model node.",
    inputCurveClassNames,
    "BoundaryCut.InputCurve",
    true,
    true,
    inputCurveEvents
    );
  this->InputNodeInfo.push_back(inputCurve);

  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo inputModel(
    "Model node",
    "Model node to be cut with the curve.",
    inputModelClassNames,
    "BoundaryCut.InputModel",
    true,
    false,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Model node",
    "Output model containing the cut region.",
    inputModelClassNames,
    "BoundaryCut.OutputModel",
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerBoundaryCutRule::~vtkSlicerDynamicModelerBoundaryCutRule()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerBoundaryCutRule::GetName()
{
  return "BoundaryCut";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerBoundaryCutRule::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthOutputNode(0, surfaceEditorNode));
  if (!outputModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthInputNode(1, surfaceEditorNode));
  if (!inputModelNode || !inputModelNode->GetPolyData())
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  std::vector<vtkMRMLMarkupsPlaneNode*> planeNodes;
  std::vector<vtkMRMLMarkupsCurveNode*> curveNodes;
  std::string nthInputReferenceRole = this->GetNthInputNodeReferenceRole(0);
  for (int i = 0; i < surfaceEditorNode->GetNumberOfNodeReferences(nthInputReferenceRole.c_str()); ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(nthInputReferenceRole.c_str(), i);

    vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
    if (planeNode)
      {
      planeNodes.push_back(planeNode);
      continue;
      }

    vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
    if (curveNode)
      {
      curveNodes.push_back(curveNode);
      continue;
      }
    }

  /// TODO: transform input mesh to world coordinates

  vtkNew<vtkImplicitBoolean> implicitBoolean;
  for (vtkMRMLMarkupsPlaneNode* planeNode : planeNodes)
    {
    double origin_World[3] = { 0.0 };
    planeNode->GetOriginWorld(origin_World);

    double normal_World[3] = { 0.0 };
    planeNode->GetNormalWorld(normal_World);

    vtkNew<vtkPlane> plane_World;
    plane_World->SetOrigin(origin_World);
    plane_World->SetNormal(normal_World);
    implicitBoolean->AddFunction(plane_World);
    }

  vtkNew<vtkExtractPolyDataGeometry> extractFilter;
  extractFilter->SetImplicitFunction(implicitBoolean);
  extractFilter->ExtractInsideOff();
  extractFilter->ExtractBoundaryCellsOff();
  extractFilter->SetInputData(inputModelNode->GetPolyData());
  extractFilter->Update();

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(extractFilter->GetOutput());

  vtkNew<vtkSelectPolyData>          selectionFilter;
  selectionFilter->GenerateSelectionScalarsOn();
  selectionFilter->SetSelectionModeToSmallestRegion();

  vtkNew<vtkClipPolyData>            clipFilter;
  clipFilter->SetInputConnection(selectionFilter->GetOutputPort());
  clipFilter->InsideOutOn();

  vtkNew<vtkConnectivityFilter>      connectivityFilter;
  connectivityFilter->SetInputConnection(clipFilter->GetOutputPort());

  
  //this->CleanFilter->SetInputConnection(this->ConnectivityFilter->GetOutputPort());

  for (vtkMRMLMarkupsCurveNode* curveNode : curveNodes)
    {
    vtkPoints* curvePointsWorld = curveNode->GetCurvePointsWorld();
    selectionFilter->SetLoop(curvePointsWorld);
    selectionFilter->SetInputData(outputPolyData);
    connectivityFilter->Update();
    outputPolyData->DeepCopy(connectivityFilter->GetOutput());
    outputPolyData->Modified();
    }

  vtkNew<vtkCleanPolyData> cleanPolyData;
  cleanPolyData->SetInputData(outputPolyData);
  cleanPolyData->Update();
  outputPolyData->DeepCopy(cleanPolyData->GetOutput());

  if (outputModelNode->GetPolyData())
    {
    outputModelNode->GetPolyData()->DeepCopy(outputPolyData);
    }
  else
    {
    outputModelNode->SetAndObserveMesh(outputPolyData);
    }

  return true;
}