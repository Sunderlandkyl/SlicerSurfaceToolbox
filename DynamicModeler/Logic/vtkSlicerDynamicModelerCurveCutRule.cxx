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

#include "vtkSlicerDynamicModelerCurveCutRule.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
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

// vtkAddon
#include <vtkAddonMathUtilities.h>

// TODO
#include <vtkMRMLScene.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkDijkstraGraphGeodesicPath.h>
#include <vtkPointLocator.h>
#include <vtkCellLocator.h>

//----------------------------------------------------------------------------
vtkRuleNewMacro(vtkSlicerDynamicModelerCurveCutRule);

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerCurveCutRule::vtkSlicerDynamicModelerCurveCutRule()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputCurveEvents;
  inputCurveEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputCurveEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputCurveEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputCurveClassNames;
  inputCurveClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  NodeInfo inputCurve(
    "Curve node",
    "Curve node to cut the model node.",
    inputCurveClassNames,
    "CurveCut.InputCurve",
    true,
    false,
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
    "CurveCut.InputModel",
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
    "CurveCut.OutputModel",
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  this->ModelToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();

  this->ModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->ModelToWorldTransformFilter->SetTransform(this->ModelToWorldTransform);

  this->SelectionFilter = vtkSmartPointer<vtkSelectPolyData>::New();
  this->SelectionFilter->GenerateSelectionScalarsOn();
  this->SelectionFilter->SetSelectionModeToSmallestRegion();

  this->ClipFilter = vtkSmartPointer<vtkClipPolyData>::New();
  this->ClipFilter->SetInputConnection(this->SelectionFilter->GetOutputPort());
  this->ClipFilter->InsideOutOn();

  this->ConnectivityFilter = vtkSmartPointer<vtkConnectivityFilter>::New();
  this->ConnectivityFilter->SetInputConnection(this->ClipFilter->GetOutputPort());

  this->CleanFilter = vtkSmartPointer<vtkCleanPolyData>::New();
  this->CleanFilter->SetInputConnection(this->ConnectivityFilter->GetOutputPort());
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerCurveCutRule::~vtkSlicerDynamicModelerCurveCutRule()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerCurveCutRule::GetName()
{
  return "Curve cut";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerCurveCutRule::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
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

  vtkMRMLNode* inputNode = this->GetNthInputNode(0, surfaceEditorNode);
  vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
  vtkMRMLMarkupsClosedCurveNode* closedCurveNode = vtkMRMLMarkupsClosedCurveNode::SafeDownCast(inputNode);
  if (!curveNode)
    {
    vtkErrorMacro("Invalid input curve node!");
    return false;
    }

  vtkMRMLMarkupsPlaneNode* plane = vtkMRMLMarkupsPlaneNode::SafeDownCast(curveNode->GetScene()->GetFirstNodeByName("PlanePreview"));
  if (!plane)
  {
    plane = vtkMRMLMarkupsPlaneNode::SafeDownCast(curveNode->GetScene()->AddNewNodeByClass("vtkMRMLMarkupsPlaneNode", "PlanePreview"));
    plane->SetSizeMode(vtkMRMLMarkupsPlaneNode::SizeModeAbsolute);
    plane->SetPlaneBounds(-25, 25, -12.5, 12.5, 0.0, 0.0);
  }
  MRMLNodeModifyBlocker b(plane);

  double roughCenterOfMass[3] = { 0.0 };
  double bounds_World[6] = {0.0};
  inputModelNode->GetRASBounds(bounds_World);
  for (int i = 0; i < 3; ++i)
    {
    roughCenterOfMass[i] = (bounds_World[2 * i] + bounds_World[2 * i + 1]) / 2.0;
    }

  vtkSmartPointer<vtkPoints> curvePointsWorld = curveNode->GetCurvePointsWorld();
  if (!closedCurveNode && curvePointsWorld && curvePointsWorld->GetNumberOfPoints() > 1)
    {
    vtkNew<vtkMatrix4x4> planeMatrix;
    vtkAddonMathUtilities::FitPlaneToPoints(curvePointsWorld, planeMatrix);

    vtkNew<vtkTransform> planeTransform;
    planeTransform->SetMatrix(planeMatrix);
    double planeNormal[3] = { 0.0, 0.0, 1000.0 };
    planeTransform->TransformVector(planeNormal, planeNormal);

    double origin[3] = { 0.0 };
    planeTransform->TransformPoint(origin, origin);

    double vector[3] = { 0.0 };
    vtkMath::Subtract(roughCenterOfMass, origin, vector);
    if (vtkMath::Dot(planeNormal, vector) > 0.0)
      {
      vtkMath::MultiplyScalar(planeNormal, -1.0);
      }
    plane->SetNormalWorld(planeNormal);
    plane->SetOriginWorld(origin);

    vtkSmartPointer<vtkPoints> tempPointsWorld = vtkSmartPointer<vtkPoints>::New();

    //double firstPoint[3] = { 0.0 };
    //curvePointsWorld->GetPoint(0, firstPoint);
    //vtkMath::Add(firstPoint, planeNormal, firstPoint);
    //tempPointsWorld->InsertNextPoint(firstPoint);

    for (int i = 0; i < curvePointsWorld->GetNumberOfPoints(); ++i)
      {
      tempPointsWorld->InsertNextPoint(curvePointsWorld->GetPoint(i));
      }

    //vtkCellLocator
    vtkNew<vtkPointLocator> locator;
    locator->SetDataSet(inputModelNode->GetMesh());
    locator->BuildLocator();

    double firstPoint[3] = { 0.0 };
    curvePointsWorld->GetPoint(0, firstPoint);    
    vtkIdType firstPointId0 = locator->FindClosestPoint(firstPoint);
    vtkMath::Add(firstPoint, planeNormal, firstPoint);
    vtkIdType firstPointId1 = locator->FindClosestPoint(firstPoint);

    vtkNew<vtkDijkstraGraphGeodesicPath> firstGraph;
    firstGraph->SetEndVertex(firstPointId0);
    firstGraph->SetStartVertex(firstPointId1);
    firstGraph->SetInputData(inputModelNode->GetMesh());
    firstGraph->Update();

    vtkPolyData* firstPath = firstGraph->GetOutput();
    for (int i = 1; i < firstPath->GetNumberOfPoints(); ++i)
      {
      double point[3] = { 0.0 };
      firstPath->GetPoint(i, point);
      tempPointsWorld->InsertNextPoint(point);
      }

    double lastPoint[3] = { 0.0 };
    curvePointsWorld->GetPoint(curvePointsWorld->GetNumberOfPoints() - 1, lastPoint);
    vtkIdType lastPointId0 = locator->FindClosestPoint(lastPoint);
    vtkMath::Add(lastPoint, planeNormal, lastPoint);
    vtkIdType lastPointId1 = locator->FindClosestPoint(lastPoint);

    vtkNew<vtkDijkstraGraphGeodesicPath> lastGraph;
    lastGraph->SetEndVertex(lastPointId1);
    lastGraph->SetStartVertex(lastPointId0);
    lastGraph->SetInputData(inputModelNode->GetMesh());
    lastGraph->Update();

    vtkPolyData* lastPath = lastGraph->GetOutput();
    for (int i = 1; i < lastPath->GetNumberOfPoints(); ++i)
      {
      double point[3] = { 0.0 };
      lastPath->GetPoint(i, point);
      tempPointsWorld->InsertNextPoint(point);
      }

    //double lastPoint[3] = { 0.0 };
    //curvePointsWorld->GetPoint(curvePointsWorld->GetNumberOfPoints()-1, lastPoint);
    //vtkMath::Add(lastPoint, planeNormal, lastPoint);
    //tempPointsWorld->InsertNextPoint(lastPoint);

    curvePointsWorld = tempPointsWorld;
    }

  this->ModelToWorldTransformFilter->SetInputData(inputModelNode->GetPolyData());
  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->ModelToWorldTransform);
    }
  else
    {
    this->ModelToWorldTransform->Identity();
    }

  this->SelectionFilter->SetInputConnection(this->ModelToWorldTransformFilter->GetOutputPort());
  this->SelectionFilter->SetLoop(curvePointsWorld);
  this->CleanFilter->Update();

  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->CleanFilter->GetOutput());
  
  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
  
  return true;
}