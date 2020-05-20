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

#include "vtkSlicerDynamicModelerAppendRule.h"

#include "vtkMRMLDynamicModelerNode.h"

// MRML includes
#include <vtkMRMLMarkupsPlaneNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLTransformNode.h>

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkCellData.h>
#include <vtkCleanPolyData.h>
#include <vtkCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkIntArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkTransformPolyDataFilter.h>

//----------------------------------------------------------------------------
vtkRuleNewMacro(vtkSlicerDynamicModelerAppendRule);

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerAppendRule::vtkSlicerDynamicModelerAppendRule()
{
  /////////
  // Inputs
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
    "Append.InputModel",
    true,
    true,
    inputModelEvents
    );
  this->InputNodeInfo.push_back(inputModel);

  /////////
  // Outputs
  NodeInfo outputModel(
    "Model node",
    "Output model containing the cut region.",
    inputModelClassNames,
    "Append.OutputModel",
    false,
    false
    );
  this->OutputNodeInfo.push_back(outputModel);

  this->AppendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

  this->CleanFilter = vtkSmartPointer<vtkCleanPolyData>::New();
  this->CleanFilter->SetInputConnection(this->AppendFilter->GetOutputPort());

  this->OutputWorldToModelTransform = vtkSmartPointer<vtkGeneralTransform>::New();
  this->OutputWorldToModelTransformFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  this->OutputWorldToModelTransformFilter->SetInputConnection(this->CleanFilter->GetOutputPort());
  this->OutputWorldToModelTransformFilter->SetTransform(this->OutputWorldToModelTransform);
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModelerAppendRule::~vtkSlicerDynamicModelerAppendRule()
= default;

//----------------------------------------------------------------------------
const char* vtkSlicerDynamicModelerAppendRule::GetName()
{
  return "Append";
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerAppendRule::RunInternal(vtkMRMLDynamicModelerNode* surfaceEditorNode)
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
  
  this->AppendFilter->RemoveAllInputs();

  std::string nthInputReferenceRole = this->GetNthInputNodeReferenceRole(0);
  for (int i = 0; i < surfaceEditorNode->GetNumberOfNodeReferences(nthInputReferenceRole.c_str()); ++i)
    {
    vtkMRMLNode* inputNode = surfaceEditorNode->GetNthNodeReference(nthInputReferenceRole.c_str(), i);
    vtkMRMLModelNode* modelNode = vtkMRMLModelNode::SafeDownCast(inputNode);
    if (!modelNode)
      {
      continue;
      }

    vtkNew<vtkGeneralTransform> modelToWorldTransform;
    if (modelNode->GetParentTransformNode())
      {
      modelNode->GetParentTransformNode()->GetTransformToWorld(modelToWorldTransform);
      }

    vtkNew<vtkTransformPolyDataFilter> modelToWorldTransformFilter;
    modelToWorldTransformFilter->SetInputData(modelNode->GetPolyData());
    modelToWorldTransformFilter->SetTransform(modelToWorldTransform);
    this->AppendFilter->AddInputConnection(modelToWorldTransformFilter->GetOutputPort());
    }

  if (outputModelNode->GetParentTransformNode())
    {
    outputModelNode->GetParentTransformNode()->GetTransformFromWorld(this->OutputWorldToModelTransform);
    }
  else
    {
    this->OutputWorldToModelTransform->Identity();
    }
  this->OutputWorldToModelTransformFilter->Update();

  vtkNew<vtkPolyData> outputPolyData;
  outputPolyData->DeepCopy(this->OutputWorldToModelTransformFilter->GetOutput());
  this->RemoveDuplicateCells(outputPolyData);

  MRMLNodeModifyBlocker blocker(outputModelNode);
  outputModelNode->SetAndObservePolyData(outputPolyData);
  outputModelNode->InvokeCustomModifiedEvent(vtkMRMLModelNode::MeshModifiedEvent);

  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModelerAppendRule::RemoveDuplicateCells(vtkPolyData* input)
{
  vtkNew<vtkPolyData> output;
  if (input->GetNumberOfPolys() == 0)
    {
    // set up a polyData with same data arrays as input, but
    // no points, polys or data.
    output->ShallowCopy(input);
    return 1;
    }

  // Copy over the original points. Assume there are no degenerate points.
  output->SetPoints(input->GetPoints());

  // Remove duplicate polys.
  std::map<std::set<int>, vtkIdType> polySet;
  std::map<std::set<int>, vtkIdType>::iterator polyIter;

  // Now copy the polys.
  vtkNew<vtkIdList> polyPoints;
  const vtkIdType numberOfPolys = input->GetNumberOfPolys();
  vtkIdType progressStep = numberOfPolys / 100;
  if (progressStep == 0)
    {
    progressStep = 1;
    }

  output->Allocate(input->GetNumberOfCells());
  int ndup = 0;

  output->GetPointData()->PassData(input->GetPointData());
  output->GetCellData()->CopyAllocate(input->GetCellData(), numberOfPolys);

  for (int id = 0; id < numberOfPolys; ++id)
    {
    // duplicate points do not make poly vertices or triangles
    // strips degenerate so don't remove them
    int polyType = input->GetCellType(id);
    if (polyType == VTK_POLY_VERTEX || polyType == VTK_TRIANGLE_STRIP)
      {
      input->GetCellPoints(id, polyPoints);
      vtkIdType newId = output->InsertNextCell(polyType, polyPoints);
      output->GetCellData()->CopyData(input->GetCellData(), id, newId);
      continue;
      }

    input->GetCellPoints(id, polyPoints);
    std::set<int> nn;
    std::vector<int> ptIds;
    for (int i = 0; i < polyPoints->GetNumberOfIds(); ++i)
      {
      int polyPtId = polyPoints->GetId(i);
      nn.insert(polyPtId);
      ptIds.push_back(polyPtId);
      }

    // this conditional may generate non-referenced nodes
    polyIter = polySet.find(nn);

    // only copy a cell to the output if it is neither degenerate nor duplicate
    if (nn.size() == static_cast<unsigned int>(polyPoints->GetNumberOfIds()) &&
      polyIter == polySet.end())
      {
      vtkIdType newId = output->InsertNextCell(input->GetCellType(id), polyPoints);
      output->GetCellData()->CopyData(input->GetCellData(), id, newId);
      polySet[nn] = newId;
      }
    else if (polyIter != polySet.end())
      {
      ++ndup; // cell has duplicate(s)
      }
    }

  if (ndup)
    {
    vtkDebugMacro(<< "vtkRemoveDuplicatePolys : " << ndup
      << " duplicate polys (multiple instances of a polygon) have been"
      << " removed." << endl);

    output->Squeeze();
    }
  input->DeepCopy(output);
}