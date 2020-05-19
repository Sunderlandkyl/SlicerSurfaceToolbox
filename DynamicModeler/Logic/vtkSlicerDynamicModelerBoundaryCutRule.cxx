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

// MRML includes
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkClipPolyData.h>
#include <vtkCommand.h>
#include <vtkConnectivityFilter.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkFeatureEdges.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPlane.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStripper.h>
#include <vtkTransformPolyDataFilter.h>

// DynamicModelerLogic includes
#include "vtkImplicitPolyDataPointDistance.h"

// DynamicModelerMRML includes
#include "vtkMRMLDynamicModelerNode.h"

//----------------------------------------------------------------------------
vtkRuleNewMacro(vtkSlicerDynamicModelerBoundaryCutRule);

const char* INPUT_BORDER_REFERENCE_ROLE = "BoundaryCut.InputBorder";
const char* INPUT_MODEL_REFERENCE_ROLE = "BoundaryCut.InputModel";
const char* INPUT_SEED_REFERENCE_ROLE = "BoundaryCut.InputSeed";
const char* OUTPUT_MODEL_REFERENCE_ROLE = "BoundaryCut.OutputModel";

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerBoundaryCutRule::vtkSlicerDynamicModelerBoundaryCutRule()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputMarkupEvents;
  inputMarkupEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputMarkupEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);

  vtkNew<vtkStringArray> inputBorderClassNames;
  inputBorderClassNames->InsertNextValue("vtkMRMLMarkupsCurveNode");
  inputBorderClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  NodeInfo inputBorder(
    "Border node",
    "Markup node that creates part of the border for the region that will be extracted.",
    inputBorderClassNames,
    INPUT_BORDER_REFERENCE_ROLE,
    true,
    true,
    inputMarkupEvents
    );
  this->InputNodeInfo.push_back(inputBorder);

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
    INPUT_MODEL_REFERENCE_ROLE,
    true,
    false,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  vtkNew<vtkStringArray> inputSeedFiducialClassNames;
  inputSeedFiducialClassNames->InsertNextValue("vtkMRMLMarkupsFiducialNode");
  NodeInfo inputSeed(
    "Seed fiducial node",
    "Markup fiducial node that designates the region from the surface that should be preserved.",
    inputSeedFiducialClassNames,
    INPUT_SEED_REFERENCE_ROLE,
    false,
    false,
    inputMarkupEvents
  );
  this->InputNodeInfo.push_back(inputSeed);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Model node",
    "Output model containing the cut region.",
    inputModelClassNames,
    OUTPUT_MODEL_REFERENCE_ROLE,
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  this->TransformPolyDataFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
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
  if (!inputModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkPolyData* inputPolyData = inputModelNode->GetPolyData();
  if (!inputPolyData || inputPolyData->GetNumberOfPoints() < 1)
    {
    // Nothing to output
    return true;
    }


  vtkNew<vtkAppendPolyData> appendFilter;
  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(INPUT_BORDER_REFERENCE_ROLE);
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(INPUT_BORDER_REFERENCE_ROLE, i);
    double currentCenter_World[3] = { 0.0 };

    vtkNew<vtkPolyData> outputLinePolyData;
    vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
    if (planeNode)
      {
      double normal_World[3] = { 0.0 };
      planeNode->GetNormalWorld(normal_World);

      double origin_World[3] = { 0.0 };
      planeNode->GetOriginWorld(origin_World);

      vtkNew<vtkPlane> plane;
      plane->SetNormal(normal_World);
      plane->SetOrigin(origin_World);

      vtkNew<vtkExtractPolyDataGeometry> planeExtractor;
      planeExtractor->SetInputData(inputPolyData);
      planeExtractor->SetImplicitFunction(plane);
      planeExtractor->ExtractInsideOff();
      planeExtractor->ExtractBoundaryCellsOff();

      vtkNew<vtkFeatureEdges> boundaryEdges;
      boundaryEdges->SetInputConnection(planeExtractor->GetOutputPort());
      boundaryEdges->BoundaryEdgesOn();
      boundaryEdges->FeatureEdgesOff();
      boundaryEdges->NonManifoldEdgesOff();
      boundaryEdges->ManifoldEdgesOff();


      vtkNew<vtkStripper> boundaryStrips;
      boundaryStrips->SetInputConnection(boundaryEdges->GetOutputPort());
      boundaryStrips->Update();

      outputLinePolyData->SetPoints(boundaryStrips->GetOutput()->GetPoints());
      outputLinePolyData->SetLines(boundaryStrips->GetOutput()->GetLines());
      }

    vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
    if (curveNode)
      {
      vtkPoints* curvePoints = curveNode->GetCurvePointsWorld();

      vtkNew<vtkIdList> line;
      for (int i = 0; i < curvePoints->GetNumberOfPoints(); ++i)
        {
        line->InsertNextId(i);
        }
      vtkNew<vtkCellArray> lines;
      lines->InsertNextCell(line);

      outputLinePolyData->SetPoints(curvePoints);
      outputLinePolyData->SetLines(lines);
      }
    appendFilter->AddInputData(outputLinePolyData);
    }
  vtkNew<vtkCleanPolyData> cleanFilter;
  cleanFilter->SetInputConnection(appendFilter->GetOutputPort());
  cleanFilter->Update();

  vtkNew<vtkImplicitPolyDataPointDistance> distance;
  distance->SetInput(cleanFilter->GetOutput());

  double epsilon = 1e-5;

  double closestPointRegion_World[3] = { 0.0 };
  this->GetPositionForClosestPointRegion(surfaceEditorNode, closestPointRegion_World);

  vtkNew<vtkClipPolyData> clipPolyData;
  clipPolyData->SetInputData(inputPolyData);
  clipPolyData->SetClipFunction(distance);
  clipPolyData->SetValue(epsilon);
  clipPolyData->InsideOutOn();
  clipPolyData->GenerateClippedOutputOn();
  clipPolyData->Update();

  vtkNew<vtkConnectivityFilter> connectivity;
  connectivity->SetInputData(clipPolyData->GetClippedOutput());
  connectivity->SetExtractionModeToClosestPointRegion();
  connectivity->SetClosestPoint(closestPointRegion_World);
  connectivity->Update();

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(connectivity->GetOutput());
  outputModelNode->SetAndObserveMesh(outputPolyData);

  return true;
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerBoundaryCutRule::GetPositionForClosestPointRegion(vtkMRMLDynamicModelerNode* surfaceEditorNode, double closestPointRegion_World[3])
{
  if (surfaceEditorNode->GetNumberOfNodeReferences(INPUT_SEED_REFERENCE_ROLE) > 0)
    {
    vtkMRMLMarkupsFiducialNode* seedNode = vtkMRMLMarkupsFiducialNode::SafeDownCast(
      surfaceEditorNode->GetNthNodeReference(INPUT_SEED_REFERENCE_ROLE, 0));
    if (seedNode && seedNode->GetNumberOfControlPoints() > 0)
      {
      seedNode->GetNthControlPointPositionWorld(0, closestPointRegion_World);
      return;
      }
    }

  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(INPUT_BORDER_REFERENCE_ROLE);
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(INPUT_BORDER_REFERENCE_ROLE, i);
    double currentCenter_World[3] = { 0.0 };

    vtkNew<vtkPolyData> outputLinePolyData;
    vtkMRMLMarkupsPlaneNode* planeNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
    if (planeNode)
      {
      planeNode->GetOriginWorld(currentCenter_World);
      }
    vtkMRMLMarkupsCurveNode* curveNode = vtkMRMLMarkupsCurveNode::SafeDownCast(inputNode);
    if (curveNode)
      {
      double inv_N = 1. / curveNode->GetNumberOfControlPoints();
      for (int i = 0; i < curveNode->GetNumberOfControlPoints(); ++i)
        {
        double p[4];
        curveNode->GetNthControlPointPositionWorld(i, p);
        currentCenter_World[0] += p[0] * inv_N;
        currentCenter_World[1] += p[1] * inv_N;
        currentCenter_World[2] += p[2] * inv_N;
        }
      }
    vtkMath::MultiplyScalar(currentCenter_World, 1.0 / numberOfInputNodes);
    vtkMath::Add(currentCenter_World, closestPointRegion_World, closestPointRegion_World);
    }
}