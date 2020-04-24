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
#include "vtkSlicerDynamicModellerLogic.h"
#include "vtkSlicerDynamicModellerRuleFactory.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLDynamicModellerNode.h>

// VTK includes
#include <vtkIntArray.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

// STD includes
#include <cassert>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerDynamicModellerLogic);

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerLogic::vtkSlicerDynamicModellerLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerLogic::~vtkSlicerDynamicModellerLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndImportEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::RegisterNodes()
{
  if (this->GetMRMLScene() == NULL)
    {
    vtkErrorMacro("Scene is invalid");
    return;
    }
  this->GetMRMLScene()->RegisterNodeClass(vtkSmartPointer<vtkMRMLDynamicModellerNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  vtkMRMLDynamicModellerNode* surfaceEditorNode = vtkMRMLDynamicModellerNode::SafeDownCast(node);
  if (!surfaceEditorNode)
    {
    return;
    }
  if (!this->GetMRMLScene() || this->GetMRMLScene()->IsImporting())
    {
    return;
    }

  this->Rules[surfaceEditorNode->GetID()] = nullptr;
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkCommand::ModifiedEvent);
  events->InsertNextValue(vtkMRMLDynamicModellerNode::InputNodeModifiedEvent);
  vtkObserveMRMLNodeEventsMacro(surfaceEditorNode, events);
  this->UpdateDynamicModellerRule(surfaceEditorNode);
  this->RunDynamicModellerRule(surfaceEditorNode);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* node)
{
  vtkMRMLDynamicModellerNode* surfaceEditorNode = vtkMRMLDynamicModellerNode::SafeDownCast(node);
  if (!surfaceEditorNode)
    {
    return;
    }

  DynamicModellerRuleList::iterator rule = this->Rules.find(surfaceEditorNode->GetID());
  if (rule == this->Rules.end())
    {
    return;
    }
  this->Rules.erase(rule);
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::OnMRMLSceneEndImport()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

  std::vector<vtkMRMLNode*> parametericSurfaceNodes;
  this->GetMRMLScene()->GetNodesByClass("vtkMRMLDynamicModellerNode", parametericSurfaceNodes);
  for (vtkMRMLNode* node : parametericSurfaceNodes)
    {
    vtkMRMLDynamicModellerNode* dynamicModellerNode =
      vtkMRMLDynamicModellerNode::SafeDownCast(node);
    if (!dynamicModellerNode)
      {
      continue;
      }

    this->Rules[dynamicModellerNode->GetID()] = nullptr;
    vtkNew<vtkIntArray> events;
    events->InsertNextValue(vtkCommand::ModifiedEvent);
    events->InsertNextValue(vtkMRMLDynamicModellerNode::InputNodeModifiedEvent);
    vtkObserveMRMLNodeEventsMacro(dynamicModellerNode, events);
    this->UpdateDynamicModellerRule(dynamicModellerNode);
    }
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::ProcessMRMLNodesEvents(vtkObject* caller, unsigned long event, void* callData)
{
  Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  if (!this->GetMRMLScene() || this->GetMRMLScene()->IsImporting())
    {
    return;
    }

  vtkMRMLDynamicModellerNode* surfaceEditorNode = vtkMRMLDynamicModellerNode::SafeDownCast(caller);
  if (!surfaceEditorNode)
    {
    return;
    }

  if (surfaceEditorNode && event == vtkCommand::ModifiedEvent)
    {
    this->UpdateDynamicModellerRule(surfaceEditorNode);
    if (surfaceEditorNode->GetContinuousUpdate() && this->HasCircularReference(surfaceEditorNode))
      {
      vtkWarningMacro("Circular reference detected. Disabling continuous update for: " << surfaceEditorNode->GetName());
      surfaceEditorNode->SetContinuousUpdate(false);
      return;
      }
    }

  if (surfaceEditorNode && surfaceEditorNode->GetContinuousUpdate())
    {
    vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = this->GetDynamicModellerRule(surfaceEditorNode);
    if (rule)
      {
      this->RunDynamicModellerRule(surfaceEditorNode);
      }
    }
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModellerLogic::HasCircularReference(vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return false;
    }
  vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = this->GetDynamicModellerRule(surfaceEditorNode);
  if (!rule)
    {
    return false;
    }

  std::vector<vtkMRMLNode*> inputNodes;
  for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
    {
    vtkMRMLNode* inputNode = rule->GetNthInputNode(i, surfaceEditorNode);
    if (inputNode)
      {
      inputNodes.push_back(inputNode);
      }
    }

  for (int i = 0; i < rule->GetNumberOfOutputNodes(); ++i)
    {
    vtkMRMLNode* outputNode = rule->GetNthOutputNode(i, surfaceEditorNode);
    if (!outputNode)
      {
      continue;
      }
    std::vector<vtkMRMLNode*>::iterator inputNodeIt = std::find(inputNodes.begin(), inputNodes.end(), outputNode);
    if (inputNodeIt != inputNodes.end())
      {
      return true;
      }
    }

  return false;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::UpdateDynamicModellerRule(vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid input node!");
    return;
    }

  MRMLNodeModifyBlocker blocker(surfaceEditorNode);

  vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = this->GetDynamicModellerRule(surfaceEditorNode);
  if (!rule || strcmp(rule->GetName(), surfaceEditorNode->GetRuleName()) != 0)
    {
    // We are changing rule types, and should remove observers to the previous rule
    if (rule)
      {
      for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
        {
        std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
        const char* referenceId = surfaceEditorNode->GetNodeReferenceID(referenceRole.c_str());
        // Current behavior is to add back references without observers
        // This preserves the selected nodes for each rule
        surfaceEditorNode->SetNodeReferenceID(referenceRole.c_str(), referenceId);
        }
      }

    rule = nullptr;
    if (surfaceEditorNode->GetRuleName())
      {
      rule = vtkSmartPointer<vtkSlicerDynamicModellerRule>::Take(
        vtkSlicerDynamicModellerRuleFactory::GetInstance()->CreateRuleByName(surfaceEditorNode->GetRuleName()));
      }
    this->Rules[surfaceEditorNode->GetID()] = rule;
    }

  if (rule)
    {
    // Update node observers to ensure that all input nodes are observed
    for (int i = 0; i < rule->GetNumberOfInputNodes(); ++i)
      {
      std::string referenceRole = rule->GetNthInputNodeReferenceRole(i);
      vtkMRMLNode* node = surfaceEditorNode->GetNodeReference(referenceRole.c_str());
      if (!node)
        {
        continue;
        }

      vtkIntArray* events = rule->GetNthInputNodeEvents(i);
      surfaceEditorNode->SetAndObserveNodeReferenceID(referenceRole.c_str(), node->GetID(), events);
      }
    }
}

//---------------------------------------------------------------------------
vtkSlicerDynamicModellerRule* vtkSlicerDynamicModellerLogic::GetDynamicModellerRule(vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = nullptr;
  DynamicModellerRuleList::iterator ruleIt = this->Rules.find(surfaceEditorNode->GetID());
  if (ruleIt == this->Rules.end())
    {
    return nullptr;
    }
  return ruleIt->second;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerLogic::RunDynamicModellerRule(vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if (!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node!");
    return;
    }
  if (!surfaceEditorNode->GetRuleName())
    {
    return;
    }

  vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = this->GetDynamicModellerRule(surfaceEditorNode);
  if (!rule)
    {
    vtkErrorMacro("Could not find rule with name: " << surfaceEditorNode->GetRuleName());
    return;
    }
  if (!rule->HasRequiredInputs(surfaceEditorNode))
    {
    return;
    }

  rule->Run(surfaceEditorNode);
}
