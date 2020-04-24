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

#include "vtkSlicerDynamicModellerRule.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>

///
#include "vtkMRMLDynamicModellerNode.h"

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRule::vtkSlicerDynamicModellerRule()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRule::~vtkSlicerDynamicModellerRule()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRule* vtkSlicerDynamicModellerRule::Clone()
{
  vtkSlicerDynamicModellerRule* clone = this->CreateRuleInstance();
  return clone;
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModellerRule::GetNumberOfInputNodes()
{
  return this->InputNodeInfo.size();
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModellerRule::GetNumberOfInputParameters()
{
  return this->InputParameterInfo.size();
}

//----------------------------------------------------------------------------
int vtkSlicerDynamicModellerRule::GetNumberOfOutputNodes()
{
  return this->OutputNodeInfo.size();
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthInputNodeName(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return "";
    }
  return this->InputNodeInfo[n].Name;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthInputNodeDescription(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return "";
    }
  return this->InputNodeInfo[n].Description;
}

//----------------------------------------------------------------------------
vtkStringArray* vtkSlicerDynamicModellerRule::GetNthInputNodeClassNames(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return nullptr;
    }
  return this->InputNodeInfo[n].ClassNames;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthInputNodeReferenceRole(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return "";
    }
  return this->InputNodeInfo[n].ReferenceRole;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModellerRule::GetNthInputNodeRequired(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return false;
    }
  return this->InputNodeInfo[n].Required;
}


//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthOutputNodeName(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return "";
    }
  return this->OutputNodeInfo[n].Name;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthOutputNodeDescription(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return "";
    }
  return this->OutputNodeInfo[n].Description;
}

//----------------------------------------------------------------------------
vtkStringArray* vtkSlicerDynamicModellerRule::GetNthOutputNodeClassNames(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return nullptr;
    }
  return this->OutputNodeInfo[n].ClassNames;
}

//----------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthOutputNodeReferenceRole(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return "";
    }
  return this->OutputNodeInfo[n].ReferenceRole;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModellerRule::GetNthOutputNodeRequired(int n)
{
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return false;
    }
  return this->OutputNodeInfo[n].Required;
}

//---------------------------------------------------------------------------
void vtkSlicerDynamicModellerRule::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
  os << indent << "Name:\t" << this->GetName() << std::endl;
}

//---------------------------------------------------------------------------
vtkIntArray* vtkSlicerDynamicModellerRule::GetNthInputNodeEvents(int n)
{
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return nullptr;
    }
  return this->InputNodeInfo[n].Events;
}

//---------------------------------------------------------------------------
vtkMRMLNode* vtkSlicerDynamicModellerRule::GetNthInputNode(int n, vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if(!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node");
    return nullptr;
    }
  if (n >= this->InputNodeInfo.size())
    {
    vtkErrorMacro("Input node " << n << " is out of range!");
    return nullptr;
    }
  std::string referenceRole = this->GetNthInputNodeReferenceRole(n);
  return surfaceEditorNode->GetNodeReference(referenceRole.c_str());
}

//---------------------------------------------------------------------------
vtkMRMLNode* vtkSlicerDynamicModellerRule::GetNthOutputNode(int n, vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if(!surfaceEditorNode)
    {
    vtkErrorMacro("Invalid parameter node");
    return nullptr;
    }
  if (n >= this->OutputNodeInfo.size())
    {
    vtkErrorMacro("Output node " << n << " is out of range!");
    return nullptr;
    }
  std::string referenceRole = this->GetNthOutputNodeReferenceRole(n);
  return surfaceEditorNode->GetNodeReference(referenceRole.c_str());
}

//---------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthInputParameterName(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return "";
    }
  return this->InputParameterInfo[n].Name;
}

//---------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthInputParameterDescription(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return "";
    }
  return this->InputParameterInfo[n].Description;
}

//---------------------------------------------------------------------------
std::string vtkSlicerDynamicModellerRule::GetNthInputParameterAttributeName(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return "";
    }
  return this->InputParameterInfo[n].AttributeName;
}

//---------------------------------------------------------------------------
int vtkSlicerDynamicModellerRule::GetNthInputParameterType(int n)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return PARAMETER_STRING;
    }
  return this->InputParameterInfo[n].Type;
}

//---------------------------------------------------------------------------
vtkVariant vtkSlicerDynamicModellerRule::GetNthInputParameterValue(int n, vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if (n >= this->InputParameterInfo.size())
    {
    vtkErrorMacro("Parameter " << n << " is out of range!");
    return PARAMETER_STRING;
    }
  std::string attributeName = this->GetNthInputParameterAttributeName(n);
  const char* parameterValue = surfaceEditorNode->GetAttribute(attributeName.c_str());
  if (!parameterValue)
    {
    return this->InputParameterInfo[n].DefaultValue;
    }
  return vtkVariant(parameterValue);
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModellerRule::HasRequiredInputs(vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  for (int i = 0; i < this->GetNumberOfInputNodes(); ++i)
    {
    if (!this->GetNthInputNodeRequired(i))
      {
      continue;
      }

    std::string referenceRole = this->GetNthInputNodeReferenceRole(i);
    if (surfaceEditorNode->GetNodeReference(referenceRole.c_str()) == nullptr)
      {
      return false;
      }
    }
  return true;
}

//---------------------------------------------------------------------------
bool vtkSlicerDynamicModellerRule::Run(vtkMRMLDynamicModellerNode* surfaceEditorNode)
{
  if (!this->HasRequiredInputs(surfaceEditorNode))
    {
    vtkErrorMacro("Input node missing!");
    return false;
    }
  return this->RunInternal(surfaceEditorNode);
}
