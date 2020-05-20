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
  this->Input = nullptr;
  this->Locator = vtkSmartPointer<vtkPointLocator>::New();
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::SetInput(vtkPolyData* input)
{
  if ( this->Input != input )
    {
    this->Input = input;

    this->Input->BuildLinks();
    this->NoValue = this->Input->GetLength();

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
vtkImplicitPolyDataPointDistance::~vtkImplicitPolyDataPointDistance() = default;

//-----------------------------------------------------------------------------
double vtkImplicitPolyDataPointDistance::EvaluateFunction(double x[3])
{
  if (!this->Locator || !this->Input)
    {
    return this->NoValue;
    }

  vtkIdType id = this->Locator->FindClosestPoint(x);
  double closestPoint[3] = { 0.0 };
  this->Input->GetPoint(id, closestPoint);
  return vtkMath::Distance2BetweenPoints(x, closestPoint);
}

//-----------------------------------------------------------------------------
void vtkImplicitPolyDataPointDistance::EvaluateGradient(double x[3], double g[3])
{
  if (!this->Locator || !this->Input)
    {
    g[0] = this->NoGradient[0];
    g[1] = this->NoGradient[1];
    g[2] = this->NoGradient[2];
    return;
    }

  vtkIdType id = this->Locator->FindClosestPoint(x);
  double closestPoint[3] = { 0.0 };
  this->Input->GetPoint(id, closestPoint);
  vtkMath::Subtract(x, closestPoint, g);
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
