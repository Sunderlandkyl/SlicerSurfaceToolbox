/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImplicitPolyDataCellDistance.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkImplicitPolyDataCellDistance.h"

#include "vtkCellData.h"
#include "vtkGenericCell.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include <vtkPointLocator.h>
#include "vtkPolyData.h"
#include "vtkPolygon.h"
#include "vtkTriangleFilter.h"
#include "vtkSmartPointer.h"

vtkStandardNewMacro(vtkImplicitPolyDataCellDistance);

//-----------------------------------------------------------------------------
vtkImplicitPolyDataCellDistance::vtkImplicitPolyDataCellDistance()
{
  this->NoClosestPoint[0] = 0.0;
  this->NoClosestPoint[1] = 0.0;
  this->NoClosestPoint[2] = 0.0;

  this->NoGradient[0] = 0.0;
  this->NoGradient[1] = 0.0;
  this->NoGradient[2] = 1.0;

  this->NoValue = 0.0;

  this->Input = nullptr;
  this->Locator = nullptr;
  this->Tolerance = 1e-12;
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataCellDistance::SetInput(vtkPolyData* input)
{
  if ( this->Input != input )
  {
    //vtkSmartPointer<vtkPolyData> inputPolyData = vtkSmartPointer<vtkPolyData>::New();
    //inputPolyData->DeepCopy(input);
    this->Input = input;

    //this->Input = inputPolyData;

    this->Input->BuildLinks();
    this->NoValue = this->Input->GetLength();

    this->CreateDefaultLocator();
    this->Locator->SetDataSet(this->Input);
    this->Locator->SetTolerance(this->Tolerance);
    this->Locator->SetNumberOfPointsPerBucket(10);
    this->Locator->AutomaticOn();
    this->Locator->BuildLocator();
  }
}

//-----------------------------------------------------------------------------
vtkMTimeType vtkImplicitPolyDataCellDistance::GetMTime()
{
  vtkMTimeType mTime=this->vtkImplicitFunction::GetMTime();
  vtkMTimeType InputMTime;

  if ( this->Input != nullptr )
  {
    InputMTime = this->Input->GetMTime();
    mTime = (InputMTime > mTime ? InputMTime : mTime);
  }

  return mTime;
}

//-----------------------------------------------------------------------------
vtkImplicitPolyDataCellDistance::~vtkImplicitPolyDataCellDistance()
{
  if ( this->Locator )
  {
    this->Locator->UnRegister(this);
    this->Locator = nullptr;
  }
}

//----------------------------------------------------------------------------
void vtkImplicitPolyDataCellDistance::CreateDefaultLocator()
{
  if ( this->Locator == nullptr)
  {
    this->Locator = vtkPointLocator::New();
  }
}

//-----------------------------------------------------------------------------
double vtkImplicitPolyDataCellDistance::EvaluateFunction(double x[3])
{
  double distance2 = VTK_DOUBLE_MAX;
  vtkIdType id = this->Locator->FindClosestPointWithinRadius(1.0, x, distance2);
  return distance2;
  //double closestPoint[3] = { 0.0 };
  //this->Locator->GetDataSet()->GetPoint(id, closestPoint);
  //return vtkMath::Distance2BetweenPoints(x, closestPoint);
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataCellDistance::EvaluateGradient(double x[3], double g[3])
{
  // TODO
  vtkErrorMacro("EvaluateGradient not implemented!");
}


//-----------------------------------------------------------------------------
void vtkImplicitPolyDataCellDistance::PrintSelf(ostream& os, vtkIndent indent)
{
  vtkImplicitFunction::PrintSelf(os,indent);

  os << indent << "NoValue: " << this->NoValue << "\n";
  os << indent << "NoGradient: (" << this->NoGradient[0] << ", "
     << this->NoGradient[1] << ", " << this->NoGradient[2] << ")\n";
  os << indent << "Tolerance: " << this->Tolerance << "\n";

  if (this->Input)
  {
    os << indent << "Input : " << this->Input << "\n";
  }
  else
  {
    os << indent << "Input : (none)\n";
  }
}
