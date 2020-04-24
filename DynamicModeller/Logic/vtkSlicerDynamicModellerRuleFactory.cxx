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

// DynamicModeller includes
#include "vtkSlicerDynamicModellerRuleFactory.h"
#include "vtkSlicerDynamicModellerRule.h"
#include "vtkSlicerDynamicModellerPlaneCutRule.h"

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkDataObject.h>

// STD includes
#include <algorithm>

//----------------------------------------------------------------------------
// The compression rule manager singleton.
// This MUST be default initialized to zero by the compiler and is
// therefore not initialized here.  The ClassInitialize and ClassFinalize methods handle this instance.
static vtkSlicerDynamicModellerRuleFactory* vtkSlicerDynamicModellerRuleFactoryInstance;


//----------------------------------------------------------------------------
// Must NOT be initialized.  Default initialization to zero is necessary.
unsigned int vtkSlicerDynamicModellerRuleFactoryInitialize::Count;

//----------------------------------------------------------------------------
// Implementation of vtkSlicerDynamicModellerRuleFactoryInitialize class.
//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRuleFactoryInitialize::vtkSlicerDynamicModellerRuleFactoryInitialize()
{
  if (++Self::Count == 1)
    {
    vtkSlicerDynamicModellerRuleFactory::classInitialize();
    }
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRuleFactoryInitialize::~vtkSlicerDynamicModellerRuleFactoryInitialize()
{
  if (--Self::Count == 0)
    {
    vtkSlicerDynamicModellerRuleFactory::classFinalize();
    }
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------
// Up the reference count so it behaves like New
vtkSlicerDynamicModellerRuleFactory* vtkSlicerDynamicModellerRuleFactory::New()
{
  vtkSlicerDynamicModellerRuleFactory* ret = vtkSlicerDynamicModellerRuleFactory::GetInstance();
  ret->Register(nullptr);
  return ret;
}

//----------------------------------------------------------------------------
// Return the single instance of the vtkSlicerDynamicModellerRuleFactory
vtkSlicerDynamicModellerRuleFactory* vtkSlicerDynamicModellerRuleFactory::GetInstance()
{
  if (!vtkSlicerDynamicModellerRuleFactoryInstance)
    {
    // Try the factory first
    vtkSlicerDynamicModellerRuleFactoryInstance = (vtkSlicerDynamicModellerRuleFactory*)vtkObjectFactory::CreateInstance("vtkSlicerDynamicModellerRuleFactory");
    // if the factory did not provide one, then create it here
    if (!vtkSlicerDynamicModellerRuleFactoryInstance)
      {
      vtkSlicerDynamicModellerRuleFactoryInstance = new vtkSlicerDynamicModellerRuleFactory;
#ifdef VTK_HAS_INITIALIZE_OBJECT_BASE
      vtkSlicerDynamicModellerRuleFactoryInstance->InitializeObjectBase();
#endif
      }
    }
  // return the instance
  return vtkSlicerDynamicModellerRuleFactoryInstance;
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRuleFactory::vtkSlicerDynamicModellerRuleFactory()
= default;

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRuleFactory::~vtkSlicerDynamicModellerRuleFactory()
= default;

//----------------------------------------------------------------------------
void vtkSlicerDynamicModellerRuleFactory::PrintSelf(ostream & os, vtkIndent indent)
{
  this->vtkObject::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModellerRuleFactory::classInitialize()
{
  // Allocate the singleton
  vtkSlicerDynamicModellerRuleFactoryInstance = vtkSlicerDynamicModellerRuleFactory::GetInstance();

  vtkSlicerDynamicModellerRuleFactoryInstance->RegisterDynamicModellerRule(vtkSmartPointer<vtkSlicerDynamicModellerPlaneCutRule>::New());
}

//----------------------------------------------------------------------------
void vtkSlicerDynamicModellerRuleFactory::classFinalize()
{
  vtkSlicerDynamicModellerRuleFactoryInstance->Delete();
  vtkSlicerDynamicModellerRuleFactoryInstance = nullptr;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModellerRuleFactory::RegisterDynamicModellerRule(vtkSmartPointer<vtkSlicerDynamicModellerRule> rule)
{
  for (unsigned int i = 0; i < this->RegisteredRules.size(); ++i)
    {
    if (strcmp(this->RegisteredRules[i]->GetClassName(), rule->GetClassName()) == 0)
      {
      vtkWarningMacro("RegisterStreamingCodec failed: rule is already registered");
      return false;
      }
    }
  this->RegisteredRules.push_back(rule);
  return true;
}

//----------------------------------------------------------------------------
bool vtkSlicerDynamicModellerRuleFactory::UnregisterDynamicModellerRuleByClassName(const std::string & className)
{
  std::vector<vtkSmartPointer<vtkSlicerDynamicModellerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = *ruleIt;
    if (strcmp(rule->GetClassName(), className.c_str()) == 0)
      {
      this->RegisteredRules.erase(ruleIt);
      return true;
      }
    }
  vtkWarningMacro("UnRegisterStreamingCodecByClassName failed: rule not found");
  return false;
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRule* vtkSlicerDynamicModellerRuleFactory::CreateRuleByClassName(const std::string & className)
{
  std::vector<vtkSmartPointer<vtkSlicerDynamicModellerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = *ruleIt;
    if (strcmp(rule->GetClassName(), className.c_str()) == 0)
      {
      return rule->CreateRuleInstance();
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
vtkSlicerDynamicModellerRule* vtkSlicerDynamicModellerRuleFactory::CreateRuleByName(const std::string name)
{
  std::vector<vtkSmartPointer<vtkSlicerDynamicModellerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = *ruleIt;
    if (rule->GetName() == name)
      {
      return rule->CreateRuleInstance();
      }
    }
  return nullptr;
}

//----------------------------------------------------------------------------
const std::vector<std::string> vtkSlicerDynamicModellerRuleFactory::GetDynamicModellerRuleClassNames()
{
  std::vector<std::string> ruleClassNames;
  std::vector<vtkSmartPointer<vtkSlicerDynamicModellerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = *ruleIt;
    ruleClassNames.emplace_back(rule->GetClassName());
    }
  return ruleClassNames;
}

//----------------------------------------------------------------------------
const std::vector<std::string> vtkSlicerDynamicModellerRuleFactory::GetDynamicModellerRuleNames()
{
  std::vector<std::string> names;
  std::vector<vtkSmartPointer<vtkSlicerDynamicModellerRule> >::iterator ruleIt;
  for (ruleIt = this->RegisteredRules.begin(); ruleIt != this->RegisteredRules.end(); ++ruleIt)
    {
    vtkSmartPointer<vtkSlicerDynamicModellerRule> rule = *ruleIt;
    names.push_back(rule->GetName());
    }
  return names;
}
