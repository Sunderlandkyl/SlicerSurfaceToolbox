/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImplicitPolyDataPointDistance.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkImplicitPolyDataPointDistance.h"

#include "vtkCellData.h"
#include "vtkGenericCell.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include <vtkPointLocator.h>
#include "vtkPolyData.h"
#include "vtkPolygon.h"
#include "vtkTriangleFilter.h"
#include "vtkSmartPointer.h"

vtkStandardNewMacro(vtkImplicitPolyDataPointDistance);

//-----------------------------------------------------------------------------
vtkImplicitPolyDataPointDistance::vtkImplicitPolyDataPointDistance()
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
void vtkImplicitPolyDataPointDistance::SetInput(vtkPolyData* input)
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
vtkMTimeType vtkImplicitPolyDataPointDistance::GetMTime()
{
  vtkMTimeType mTime=this->vtkImplicitFunction::GetMTime();
  vtkMTimeType inputMTime;

  if ( this->Input != nullptr )
    {
    inputMTime = this->Input->GetMTime();
    mTime = (inputMTime > mTime ? inputMTime : mTime);
    }

  return mTime;
}

//-----------------------------------------------------------------------------
vtkImplicitPolyDataPointDistance::~vtkImplicitPolyDataPointDistance()
{
  if ( this->Locator )
    {
    this->Locator->UnRegister(this);
    this->Locator = nullptr;
    }
}

//----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::CreateDefaultLocator()
{
  if ( this->Locator == nullptr)
  {
    this->Locator = vtkPointLocator::New();
  }
}

//-----------------------------------------------------------------------------
double vtkImplicitPolyDataPointDistance::EvaluateFunction(double x[3])
{
  vtkIdType id = this->Locator->FindClosestPoint(x);
  double closestPoint[3] = { 0.0 };
  this->Locator->GetDataSet()->GetPoint(id, closestPoint);
  return vtkMath::Distance2BetweenPoints(x, closestPoint);
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::EvaluateGradient(double x[3], double g[3])
{
  // TODO
  vtkErrorMacro("EvaluateGradient not implemented!");
}


//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::PrintSelf(ostream& os, vtkIndent indent)
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
