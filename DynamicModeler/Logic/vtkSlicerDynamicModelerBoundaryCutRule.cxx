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
#include <vtkContourLoopExtraction.h>
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

#include <vtkImplicitDataSet.h>
#include <vtkImplicitPolyDataCellDistance.h>
#include <vtkImplicitModeller.h>
#include <vtkLoopBooleanPolyDataFilter.h>

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

  vtkPolyData* inputPolyData = inputModelNode->GetPolyData();
  if (!inputPolyData)
    {
    return true;
    }

  double center_World[3] = { 0.0 };
  vtkNew<vtkAppendPolyData> appendFilter;
  std::string nthInputReferenceRole = this->GetNthInputNodeReferenceRole(0);
  int numberOfInputNodes = surfaceEditorNode->GetNumberOfNodeReferences(nthInputReferenceRole.c_str());
  for (int i = 0; i < numberOfInputNodes; ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(nthInputReferenceRole.c_str(), i);
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

      //vtkNew<vtkClipPolyData> clipper;
      //clipper->SetInputData(inputPolyData);
      //clipper->SetClipFunction(plane);

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

      planeNode->GetOriginWorld(currentCenter_World);
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
    vtkMath::Add(currentCenter_World, center_World, center_World);

    appendFilter->AddInputData(outputLinePolyData);
    }

  vtkNew<vtkCleanPolyData> cleanFilter;
  cleanFilter->SetInputConnection(appendFilter->GetOutputPort());
  cleanFilter->Update();

  vtkNew<vtkImplicitDataSet> distance;
  distance->SetDataSet(inputPolyData);
  //vtkNew<vtkImplicitPolyDataCellDistance> distance;
  //vtkNew<vtkImplicitModeller> distance;
  //distance->SetInputData(cleanFilter->GetOutput());
  //distance->SetInput(cleanFilter->GetOutput());
  //vtkImplicitModeller 
  vtkNew<vtkClipPolyData> clipPolyData;
  clipPolyData->SetClipFunction(distance);
  double epsilon = 1e-5;
  clipPolyData->SetValue(epsilon);
  clipPolyData->InsideOutOn();
  clipPolyData->GenerateClipScalarsOn();
  clipPolyData->GenerateClippedOutputOn();
  clipPolyData->SetInputData(inputPolyData);
  clipPolyData->Update();

  //vtkNew<vtkLoopBooleanPolyDataFilter> loop;
  //loop->SetInputData(0, inputPolyData);
  //loop->SetInputData(1, cleanFilter->GetOutput());
  //loop->Update();

  //vtkNew<vtkContourLoopExtraction> contourLoopExtraction;
  //contourLoopExtraction->SetInputConnection(cleanFilter->GetOutputPort());
  //contourLoopExtraction->SetOutputModeToPolylines();
  //contourLoopExtraction->Update();

  vtkNew<vtkConnectivityFilter> connectivity;
  connectivity->SetInputData(clipPolyData->GetClippedOutput());
  connectivity->SetExtractionModeToClosestPointRegion();
  connectivity->SetClosestPoint(center_World);
  connectivity->Update();

  vtkNew<vtkPolyData> outputPolyData;
  //outputPolyData->SetPoints(cleanFilter->GetOutput()->GetPoints());
  //outputPolyData->SetLines(cleanFilter->GetOutput()->GetLines());
  outputPolyData->DeepCopy(connectivity->GetOutput());
  //if (outputModelNode->GetPolyData())
  //  {
  //  outputModelNode->GetPolyData()->DeepCopy(outputPolyData);
  //  }
  //else
  //  {
    outputModelNode->SetAndObserveMesh(outputPolyData);
    //}

  return true;
}