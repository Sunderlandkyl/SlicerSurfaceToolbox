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

#include "vtkSlicerDynamicModelerMirrorRule.h"

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
vtkRuleNewMacro(vtkSlicerDynamicModelerMirrorRule);

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerMirrorRule::vtkSlicerDynamicModelerMirrorRule()
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
    "Plane node to mirror the model node.",
    inputPlaneClassNames,
    "Mirror.InputPlane",
    true,
    false,
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
    "Model node to be mirrored.",
    inputModelClassNames,
    "Mirror.InputModel",
    true,
    false,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Mirrored output model",
    "Input model mirrored accross the plane",
    inputModelClassNames,
    "Mirror.OutputModel",
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  this->InputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->InputModelNodeToWorldTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->InputModelToWorldTransformFilter->SetTransform(this->InputModelNodeToWorldTransform);

  this->MirrorTransform = vtkSmartPointer<vtkTransform>::New();
  this->MirrorTransform->PostMultiply();

  this->MirrorFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->MirrorFilter ->SetInputConnection(this->InputModelToWorldTransformFilter->GetOutputPort());
  this->MirrorFilter ->SetTransform(this->MirrorTransform);

  this->ReverseNormalFilter = vtkSmartPointer<vtkReverseSense>::New();
  this->ReverseNormalFilter->SetInputConnection(this->MirrorFilter->GetOutputPort());

  this->OutputModelToWorldTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputModelToWorldTransformFilter->SetTransform(this->OutputWorldToModelTransform);
  this->OutputModelToWorldTransformFilter->SetInputConnection(this->ReverseNormalFilter->GetOutputPort());
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerMirrorRule::~vtkSlicerDynamicModelerMirrorRule()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerMirrorRule::GetName()
{
  return "Mirror";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerMirrorRule::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
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
    vtkErrorMacro("Invalid input model node!");
    return false;
    }

  vtkMRMLNode* inputNode = this->GetNthInputNode(0, surfaceEditorNode);
  vtkMRMLMarkupsPlaneNode* inputPlaneNode = vtkMRMLMarkupsPlaneNode::SafeDownCast(inputNode);
  vtkMRMLSliceNode* inputSliceNode = vtkMRMLSliceNode::SafeDownCast(inputNode);
  if (!inputPlaneNode && !inputSliceNode)
    {
    vtkErrorMacro("Invalid input plane node!");
    return false;
    }

  if (!inputModelNode->GetMesh() || inputModelNode->GetMesh()->GetNumberOfPoints() == 0)
    {
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
  if (outputModelNode && outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
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

  this->InputModelToWorldTransformFilter->SetInputConnection(inputModelNode->GetMeshConnection());

  double translateWorldOriginToPlaneOrigin[3] = { 0.0 };
  vtkMath::Add(translateWorldOriginToPlaneOrigin, origin_World, translateWorldOriginToPlaneOrigin);

  double translatePlaneOriginToWorldOrigin[3] = { 0.0 };
  vtkMath::Add(translatePlaneOriginToWorldOrigin, origin_World, translatePlaneOriginToWorldOrigin);
  vtkMath::MultiplyScalar(translatePlaneOriginToWorldOrigin, -1);

  vtkNew<vtkMatrix4x4> mirrorMatrix;
  mirrorMatrix->SetElement(0, 0, 1 - 2 * normal_World[0] * normal_World[0]);
  mirrorMatrix->SetElement(0, 1, - 2 * normal_World[0] * normal_World[1]);
  mirrorMatrix->SetElement(0, 2, - 2 * normal_World[0] * normal_World[2]);

  mirrorMatrix->SetElement(1, 0, - 2 * normal_World[0] * normal_World[1]);
  mirrorMatrix->SetElement(1, 1, 1 - 2 * normal_World[1] * normal_World[1]);
  mirrorMatrix->SetElement(1, 2, - 2 * normal_World[1] * normal_World[2]);

  mirrorMatrix->SetElement(2, 0, - 2 * normal_World[0] * normal_World[2]);
  mirrorMatrix->SetElement(2, 1, - 2 * normal_World[1] * normal_World[2]);
  mirrorMatrix->SetElement(2, 2, 1 - 2 * normal_World[2] * normal_World[2]);

  this->MirrorTransform->Identity();
  this->MirrorTransform->Translate(translatePlaneOriginToWorldOrigin);
  this->MirrorTransform->Concatenate(mirrorMatrix);
  this->MirrorTransform->Translate(translateWorldOriginToPlaneOrigin);

  this->OutputModelToWorldTransformFilter->Update();
  vtkNew<vtkPolyData> outputMesh;
  outputMesh->DeepCopy(this->OutputModelToWorldTransformFilter->GetOutput());

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObserveMesh(outputMesh);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);
  
  return true;
}