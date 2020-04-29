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

#include "vtkSlicerDynamicModelerPlaneCutRule.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkClipClosedSurface.h>
#include <vtkClipPolyData.h>
#include <vtkCollection.h>
#include <vtkCommand.h>
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
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkStripper.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkRuleNewMacro(vtkSlicerDynamicModelerPlaneCutRule);

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerPlaneCutRule::vtkSlicerDynamicModelerPlaneCutRule()
{
  /////////
  // Inputs
  vtkNew<vtkIntArray> inputPlaneEvents;
  inputPlaneEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputPlaneEvents->InsertNextTuple1(vtkMRMLMarkupsNode::PointModifiedEvent);
  inputPlaneEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputPlaneClassNames;
  inputPlaneClassNames->InsertNextValue("vtkMRMLMarkupsPlaneNode");
  inputPlaneClassNames->InsertNextValue("vtkMRMLSliceNode");
  NodeInfo inputPlane(
    "Plane node",
    "Plane node to cut the model node.",
    inputPlaneClassNames,
    "PlaneCut.InputPlane",
    true,
    true,
    inputPlaneEvents
    );
  this->InputNodeInfo.push_back(inputPlane);

  vtkNew<vtkIntArray> inputModelEvents;
  inputModelEvents->InsertNextTuple1(vtkCommand::ModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLModelNode::MeshModifiedEvent);
  inputModelEvents->InsertNextTuple1(vtkMRMLTransformableNode::TransformModifiedEvent);
  vtkNew<vtkStringArray> inputModelClassNames;
  inputModelClassNames->InsertNextValue("vtkMRMLModelNode");
  NodeInfo inputModel(
    "Model node",
    "Model node to be cut.",
    inputModelClassNames,
    "PlaneCut.InputModel",
    true,
    false,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputPositiveModel(
    "Clipped output model (positive side)",
    "Portion of the cut model that is on the same side of the plane as the normal.",
    inputModelClassNames,
    "PlaneCut.OutputPositiveModel",
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputPositiveModel);

  NodeInfo outputNegativeModel(
    "Clipped output model (negative side)",
    "Portion of the cut model that is on the opposite side of the plane as the normal.",
    inputModelClassNames,
    "PlaneCut.OutputNegativeModel",
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputNegativeModel);

  /////////
  // Parameters

  /////////
  // Parameters
  ParameterInfo parameterCapSurface(
    "Cap surface",
    "Create a closed surface by triangulating the clipped region",
    "CapSurface",
    PARAMETER_BOOL,
    true);
  this->InputParameterInfo.push_back(parameterCapSurface);

  ParameterInfo parameterOperationType(
    "Operation type",
    "Method used for combining the planes",
    "OperationType",
    PARAMETER_STRING_ENUM,
    "Union");

  vtkNew<vtkStringArray> possibleValues;
  parameterOperationType.PossibleValues = possibleValues;
  parameterOperationType.PossibleValues->InsertNextValue("Union");
  parameterOperationType.PossibleValues->InsertNextValue("Intersection");
  parameterOperationType.PossibleValues->InsertNextValue("Difference");
  this->InputParameterInfo.push_back(parameterOperationType);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->PlaneClipper = vtkSmartPointer<vtkClipPolyData>::New();
  this->PlaneClipper->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->PlaneClipper->SetValue(0.0);

  this->OutputPositiveModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputPositiveWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputPositiveModelToWorldTransformFilter->SetTransform(this->OutputPositiveWorldToModelTransform);
  this->OutputPositiveModelToWorldTransformFilter->SetInputConnection(this->PlaneClipper->GetOutputPort()); 

  this->OutputNegativeModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputNegativeWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputNegativeModelToWorldTransformFilter->SetTransform(this->OutputNegativeWorldToModelTransform);
  this->OutputNegativeModelToWorldTransformFilter->SetInputConnection(this->PlaneClipper->GetClippedOutputPort());
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerPlaneCutRule::~vtkSlicerDynamicModelerPlaneCutRule()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerPlaneCutRule::GetName()
{
  return "Plane cut";
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModelerPlaneCutRule::CreateEndCap(vtkPolyData* polyData)
{
  // Now extract feature edges
  vtkNew<vtkFeatureEdges> boundaryEdges;
  boundaryEdges->SetInputData(polyData);
  boundaryEdges->BoundaryEdgesOn();
  boundaryEdges->FeatureEdgesOff();
  boundaryEdges->NonManifoldEdgesOff();
  boundaryEdges->ManifoldEdgesOff();

  vtkNew<vtkStripper> boundaryStrips;
  boundaryStrips->SetInputConnection(boundaryEdges->GetOutputPort());
  boundaryStrips->Update();

  vtkNew<vtkPolyData> boundaryPolyData;
  boundaryPolyData->SetPoints(boundaryStrips->GetOutput()->GetPoints());
  boundaryPolyData->SetPolys(boundaryStrips->GetOutput()->GetLines());

  vtkNew<vtkAppendPolyData> append;
  append->AddInputData(polyData);
  append->AddInputData(boundaryPolyData);
  append->Update();
  polyData->DeepCopy(append->GetOutput());
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerPlaneCutRule::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Invalid number of inputs");
    return false;
    }

  vtkMRMLModelNode* outputPositiveModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthOutputNode(0, surfaceEditorNode));
  vtkMRMLModelNode* outputNegativeModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthOutputNode(1, surfaceEditorNode));
  if (!outputPositiveModelNode && !outputNegativeModelNode)
    {
    // Nothing to output
    return true;
    }

  vtkNew<vtkImplicitBoolean> planes;
  std::string operationType = this->GetNthInputParameterValue(1, surfaceEditorNode).ToString();
  if (operationType == "Intersection")
    {
    planes->SetOperationTypeToIntersection();
    }
  else if (operationType == "Difference")
    {
    planes->SetOperationTypeToDifference();
    }
  else
    {
    planes->SetOperationTypeToUnion();
    }

  std::string planeReferenceRole = this->GetNthInputNodeReferenceRole(0);
  std::vector<vtkMRMLNode*> planeNodes;
  surfaceEditorNode->GetNodeReferences(planeReferenceRole.c_str(), planeNodes);
  int planeIndex = 0;
  for (vtkMRMLNode* planeNode : planeNodes)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(planeReferenceRole.c_str(), planeIndex);
    vtkMRMLMarkupsPlaneNode* inputPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
    vtkMRMLSliceNode* inputSliceNode = vtkMRMLSliceNode::SafeDownCast(inputNode);
    if (!inputPlaneNode && !inputSliceNode)
      {
      vtkErrorMacro("Invalid input plane nodes!");
      return false;
      }

    double origin_World[3] = { 0.0, 0.0, 0.0 };
    double normal_World[3] = { 0.0, 0.0, 1.0 };
    if (inputPlaneNode)
      {
      inputPlaneNode->GetOriginWorld(origin_World);
      inputPlaneNode->GetNormalWorld(normal_World);
      }
    if (inputSliceNode)
      {
      vtkMatrix4x4* sliceToRAS = inputSliceNode->GetSliceToRAS();
      vtkNew<vtkTransform> sliceToRASTransform;
      sliceToRASTransform->SetMatrix(sliceToRAS);
      sliceToRASTransform->TransformPoint(origin_World, origin_World);
      sliceToRASTransform->TransformVector(normal_World, normal_World);
      }

    vtkNew<vtkPlane> currentPlane;
    currentPlane->SetNormal(normal_World);
    currentPlane->SetOrigin(origin_World);
    planes->AddFunction(currentPlane);
    ++planeIndex;
    }
  this->PlaneClipper->SetClipFunction(planes);

  vtkMRMLModelNode* inputModelNode = vtkMRMLModelNode::SafeDownCast(this->GetNthInputNode(1, surfaceEditorNode));
  if (!inputModelNode)
    {
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
    inputModelNode->GetMesh()->Initialize();
    return true;
    }

  if (inputModelNode->GetParentTransformNode())
    {
    inputModelNode->GetParentTransformNode()->GetTransformToWorld(this->InputModelNodeToWorldTransform);
    }
  else
    {
    this->InputModelNodeToWorldTransform->Identity();
    }
  if (outputPositiveModelNode && outputPositiveModelNode->GetParentTransformNode())
    {
    outputPositiveModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputPositiveWorldToModelTransform);
    }
  if (outputNegativeModelNode && outputNegativeModelNode->GetParentTransformNode())
    {
    outputNegativeModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputNegativeWorldToModelTransform);
    }

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());



  bool capSurface = this->GetNthInputParameterValue(0, surfaceEditorNode).ToInt() != 0;
  if (outputPositiveModelNode)
    {
    this->OutputPositiveModelToWorldTransformFilter->Update();
    vtkNew<vtkPolyData> outputMesh;
    outputMesh->DeepCopy(this->OutputPositiveModelToWorldTransformFilter->GetOutput());
    if (capSurface)
      {
      this->CreateEndCap(outputMesh);
      }

    MRMLNodeModifyBlocker blocker(outputPositiveModelNode);
    outputPositiveModelNode->SetAndObserveMesh(outputMesh);
    outputPositiveModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  if (outputNegativeModelNode)
    {
    this->PlaneClipper->GenerateClippedOutputOn();
    this->OutputNegativeModelToWorldTransformFilter->Update();
    vtkNew<vtkPolyData> outputMesh;
    outputMesh->DeepCopy(this->OutputNegativeModelToWorldTransformFilter->GetOutput());
    if (capSurface)
      {
      this->CreateEndCap(outputMesh);
      }

    MRMLNodeModifyBlocker blocker(outputNegativeModelNode);
    outputNegativeModelNode->SetAndObserveMesh(outputMesh);
    outputNegativeModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
    }

  return true;
}