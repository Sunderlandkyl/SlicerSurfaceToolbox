/*==============================================================================

  Program: 3D Slicer

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

#ifndef __vtkMRMLDynamicModellerNode_h
#define __vtkMRMLDynamicModellerNode_h

// MRML includes
#include <vtkMRMLNode.h>

// DynamicModeller includes
#include "vtkSlicerDynamicModellerModuleMRMLExport.h"

/// \ingroup DynamicModeller
/// \brief Parameter node for DynamicModeller
///
/// Stores the rule name, update status and input/output node references required for running dynamic modelling.
/// The rule name is used by the logic to determine what input/output nodes are required to process the dynamic modelling rule,
/// and runs the rule on the input if requested.
/// If ContinuousUpdate and Updating are both true, then the output nodes will automatically be updated when the input nodes
/// are changed.
class VTK_SLICER_DYNAMICMODELLER_MODULE_MRML_EXPORT vtkMRMLDynamicModellerNode : public vtkMRMLNode
{
public:
  static vtkMRMLDynamicModellerNode *New();
  vtkTypeMacro(vtkMRMLDynamicModellerNode, vtkMRMLNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /// Create instance of a DynamicModeller node.
  vtkMRMLNode* CreateNodeInstance() override;

  /// Set node attributes from name/value pairs
  void ReadXMLAttributes( const char** atts) override;

  /// Write this node's information to a MRML file in XML format.
  void WriteXML(ostream& of, int indent) override;

  /// Copy the node's attributes to this object
  void Copy(vtkMRMLNode *node) override;

  /// Get unique node XML tag name (like Volume, Model)
  const char* GetNodeTagName() override { return "DynamicModeller"; }

public:
  enum
  {
    InputNodeModifiedEvent = 18000, // Event that is invoked when one of the input nodes have been modified
  };

  /// The name of the vtkSlicerDynamicModellerRule that should be used for this node
  vtkGetStringMacro(RuleName);
  vtkSetStringMacro(RuleName);

  /// If continuous update is enabled, the specified rule will be run each time that any of the input nodes are modified.
  vtkGetMacro(ContinuousUpdate, bool);
  vtkSetMacro(ContinuousUpdate, bool);
  vtkBooleanMacro(ContinuousUpdate, bool);

protected:
  vtkMRMLDynamicModellerNode();
  ~vtkMRMLDynamicModellerNode() override;
  vtkMRMLDynamicModellerNode(const vtkMRMLDynamicModellerNode&);
  void operator=(const vtkMRMLDynamicModellerNode&);

  void ProcessMRMLEvents(vtkObject* caller, unsigned long eventID, void* callData) override;

  char* RuleName{ nullptr };
  bool ContinuousUpdate{ false };
};

#endif // __vtkMRMLDynamicModellerNode_h
