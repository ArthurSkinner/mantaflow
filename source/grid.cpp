/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2011 Tobias Pfaff, Nils Thuerey 
 *
 * This program is free software, distributed under the terms of the
 * GNU General Public License (GPL) 
 * http://www.gnu.org/licenses
 *
 * Grid representation
 *
 ******************************************************************************/

#include "grid.h"
#include "levelset.h"
#include "kernel.h"
#include <limits>
#include <sstream>
#include <cstring>
#include "fileio.h"

using namespace std;
namespace Manta {

//******************************************************************************
// GridBase members

GridBase::GridBase (FluidSolver* parent) 
	: PbClass(parent), mType(TypeNone)
{
	checkParent();
	m3D = getParent()->is3D();
}

//******************************************************************************
// Grid<T> members

// helpers to set type
template<class T> inline GridBase::GridType typeList() { return GridBase::TypeNone; }
template<> inline GridBase::GridType typeList<Real>()  { return GridBase::TypeReal; }
template<> inline GridBase::GridType typeList<int>()   { return GridBase::TypeInt;  }
template<> inline GridBase::GridType typeList<Vec3>()  { return GridBase::TypeVec3; }

template<class T>
Grid<T>::Grid(FluidSolver* parent, bool show)
	: GridBase(parent)
{
	mType = typeList<T>();
	mSize = parent->getGridSize();
	mData = parent->getGridPointer<T>();
	
	mStrideZ = parent->is2D() ? 0 : (mSize.x * mSize.y);
	mDx = 1.0 / mSize.max();
	clear();
	setHidden(!show);
}

template<class T>
Grid<T>::Grid(const Grid<T>& a) : GridBase(a.getParent()) {
	mSize = a.mSize;
	mType = a.mType;
	mStrideZ = a.mStrideZ;
	mDx = a.mDx;
	FluidSolver *gp = a.getParent();
	mData = gp->getGridPointer<T>();
	memcpy(mData, a.mData, sizeof(T) * a.mSize.x * a.mSize.y * a.mSize.z);
}

template<class T>
Grid<T>::~Grid() {
	mParent->freeGridPointer<T>(mData);
}

template<class T>
void Grid<T>::clear() {
	memset(mData, 0, sizeof(T) * mSize.x * mSize.y * mSize.z);    
}

template<class T>
void Grid<T>::swap(Grid<T>& other) {
	if (other.getSizeX() != getSizeX() || other.getSizeY() != getSizeY() || other.getSizeZ() != getSizeZ())
		errMsg("Grid::swap(): Grid dimensions mismatch.");
	
	T* dswap = other.mData;
	other.mData = mData;
	mData = dswap;
}

template<class T>
void Grid<T>::load(string name) {
	if (name.find_last_of('.') == string::npos)
		errMsg("file '" + name + "' does not have an extension");
	string ext = name.substr(name.find_last_of('.'));
	if (ext == ".raw")
		readGridRaw(name, this);
	else if (ext == ".uni")
		readGridUni(name, this);
	else
		errMsg("file '" + name +"' filetype not supported");
}

template<class T>
void Grid<T>::save(string name) {
	if (name.find_last_of('.') == string::npos)
		errMsg("file '" + name + "' does not have an extension");
	string ext = name.substr(name.find_last_of('.'));
	if (ext == ".raw")
		writeGridRaw(name, this);
	else if (ext == ".uni")
		writeGridUni(name, this);
	else if (ext == ".vol")
		writeGridVol(name, this);
	else if (ext == ".txt")
		writeGridTxt(name, this);
	else
		errMsg("file '" + name +"' filetype not supported");
}

//******************************************************************************
// Grid<T> operators

//! Kernel: Compute min value of Real grid
KERNEL(idx, reduce=min) returns(Real minVal=std::numeric_limits<Real>::max())
Real CompMinReal(Grid<Real>& val) {
	if (val[idx] < minVal)
		minVal = val[idx];
}

//! Kernel: Compute max value of Real grid
KERNEL(idx, reduce=max) returns(Real maxVal=-std::numeric_limits<Real>::max())
Real CompMaxReal(Grid<Real>& val) {
	if (val[idx] > maxVal)
		maxVal = val[idx];
}

//! Kernel: Compute min value of int grid
KERNEL(idx, reduce=min) returns(int minVal=std::numeric_limits<int>::max())
int CompMinInt(Grid<int>& val) {
	if (val[idx] < minVal)
		minVal = val[idx];
}

//! Kernel: Compute max value of int grid
KERNEL(idx, reduce=max) returns(int maxVal=-std::numeric_limits<int>::min())
int CompMaxInt(Grid<int>& val) {
	if (val[idx] > maxVal)
		maxVal = val[idx];
}

//! Kernel: Compute min norm of vec grid
KERNEL(idx, reduce=min) returns(Real minVal=std::numeric_limits<Real>::max())
Real CompMinVec(Grid<Vec3>& val) {
	const Real s = normSquare(val[idx]);
	if (s < minVal)
		minVal = s;
}

//! Kernel: Compute max norm of vec grid
KERNEL(idx, reduce=max) returns(Real maxVal=0)
Real CompMaxVec(Grid<Vec3>& val) {
	const Real s = normSquare(val[idx]);
	if (s > maxVal)
		maxVal = s;
}


template<class T> Grid<T>& Grid<T>::safeDivide (const Grid<T>& a) {
	gridSafeDiv<T> (*this, a);
	return *this;
}
template<class T> Grid<T>& Grid<T>::operator= (const Grid<T>& a) {
	assertMsg (a.mSize.x == mSize.x && a.mSize.y == mSize.y && a.mSize.z == mSize.z, "different grid resolutions "<<a.mSize<<" vs "<<this->mSize );
	memcpy(mData, a.mData, sizeof(T) * mSize.x * mSize.y * mSize.z);
	mType = a.mType; // copy type marker
	return *this;
}
/*template<class T> Grid<T>& Grid<T>::operator= (const T& a) {
	FOR_IDX(*this) { mData[idx] = a; }
	return *this;
}*/

PYTHON void setConstant    (Grid<Real>& grid, Real value=0.) { gridSetConst<Real>(grid,value); }
PYTHON void setConstantVec3(Grid<Vec3>& grid, Vec3 value=0.) { gridSetConst<Vec3>(grid,value); }
PYTHON void setConstantInt (Grid<int >& grid, int  value=0.) { gridSetConst<int>(grid,value); }

template<class T> void Grid<T>::add(const Grid<T>& a) {
	gridAdd<T,T>(*this, a);
}
template<class T> void Grid<T>::sub(const Grid<T>& a) {
	gridSub<T,T>(*this, a);
}
KERNEL(idx) template<class T> void knGridSetAdded (Grid<T>& me, const Grid<T>& a, const Grid<T>& b) { 
	me[idx] = a[idx] + b[idx]; }
template<class T> void Grid<T>::setAdd(const Grid<T>& a, const Grid<T>& b) {
	knGridSetAdded<T>(*this, a, b);
}
KERNEL(idx) template<class T> void knGridSetSubtracted (Grid<T>& me, const Grid<T>& a, const Grid<T>& b) { 
	me[idx] = a[idx] - b[idx]; }
template<class T> void Grid<T>::setSub(const Grid<T>& a, const Grid<T>& b) {
	knGridSetSubtracted<T>(*this, a, b);
}
KERNEL(idx) template<class T> void knGridAddConstReal (Grid<T>& me, T val) { 
	me[idx] += val; }
template<class T> void Grid<T>::addConstReal(Real a) {
	knGridAddConstReal<T>( *this, T(a) );
}
KERNEL(idx) template<class T> void knGridMultConstReal (Grid<T>& me, Real val) { 
	me[idx] *= val; }
template<class T> void Grid<T>::multiplyConstReal(Real a) {
	knGridMultConstReal<T>( *this, a );
}

template<class T> void Grid<T>::addScaledReal(const Grid<T>& b, const Real& factor) { 
	gridScaledAdd<T,T> (*this, b, factor); 
}
template<class T> void Grid<T>::multiply(const Grid<T>& b) {
	gridMult<T,T> (*this, b);
}

KERNEL(idx) template<class T> void knGridClamp (Grid<T>& me, T min, T max) { me[idx] = clamp( me[idx], min, max); }
template<class T> void Grid<T>::clamp(Real min, Real max) {
	knGridClamp<T> (*this, T(min), T(max) );
}

//! Grid a += b*factor (note, shouldnt be part of the grid class! can cause problems with python instantiation)
//  (the template T class in argument list causes errors in fromPy etc. functions).
//  Also the python integration doesnt support templated functions for now (only classes)
//  So real and vec3 version are seperately declared here
/*PYTHON void scaledAddReal(Grid<Real>& a, const Grid<Real>& b, const Real& factor) {
	gridScaledAdd<Real,Real> (a, b, factor);
}
PYTHON void scaledAddVec3(Grid<Vec3>& a, const Grid<Vec3>& b, const Vec3& factor) {
	gridScaledAdd<Vec3,Vec3> (a, b, factor);
} */
template<> Real Grid<Real>::getMaxValue() {
	return CompMaxReal (*this);
}
template<> Real Grid<Real>::getMinValue() {
	return CompMinReal (*this);
}
template<> Real Grid<Real>::getMaxAbsValue() {
	Real amin = CompMinReal (*this);
	Real amax = CompMaxReal (*this);
	return max( fabs(amin), fabs(amax));
}
template<> Real Grid<Vec3>::getMaxValue() {
	return sqrt(CompMaxVec (*this));
}
template<> Real Grid<Vec3>::getMinValue() { 
	return sqrt(CompMinVec (*this));
}
template<> Real Grid<Vec3>::getMaxAbsValue() {
	return sqrt(CompMaxVec (*this));
}
template<> Real Grid<int>::getMaxValue() {
	return (Real) CompMaxInt (*this);
}
template<> Real Grid<int>::getMinValue() {
	return (Real) CompMinInt (*this);
}
template<> Real Grid<int>::getMaxAbsValue() {
	int amin = CompMinInt (*this);
	int amax = CompMaxInt (*this);
	return max( fabs((Real)amin), fabs((Real)amax));
}

// compute maximal diference of two cells in the grid
// used for testing
PYTHON Real gridMaxDiff(Grid<Real>& g1, Grid<Real>& g2 )
{
	double maxVal = 0.;
	FOR_IJK(g1) {
		maxVal = std::max(maxVal, (double)fabs( g1(i,j,k)-g2(i,j,k) ));
	}
	return maxVal; 
}
PYTHON Real gridMaxDiffInt(Grid<int>& g1, Grid<int>& g2 )
{
	double maxVal = 0.;
	FOR_IJK(g1) {
		maxVal = std::max(maxVal, (double)fabs( (double)g1(i,j,k)-g2(i,j,k) ));
	}
	return maxVal; 
}
PYTHON Real gridMaxDiffVec3(Grid<Vec3>& g1, Grid<Vec3>& g2 )
{
	double maxVal = 0.;
	FOR_IJK(g1) {
		// accumulate differences with double precision
		// note - don't use norm here! should be as precise as possible...
		double d = 0.;
		for(int c=0; c<3; ++c) { 
			d += fabs( (double)g1(i,j,k)[c] - (double)g2(i,j,k)[c] );
		}
		maxVal = std::max(maxVal, d );
		//maxVal = std::max(maxVal, (double)fabs( norm(g1(i,j,k)-g2(i,j,k)) ));
	}
	return maxVal; 
}

// simple helper functions to convert mac to vec3 , and levelset to real grids
// (are assumed to be the same for running the test cases - in general they're not!)
PYTHON void convertMacToVec3 (MACGrid &source, Grid<Vec3>& target)
{
	FOR_IJK(target) {
		target(i,j,k) = source(i,j,k);
	}
}

PYTHON void convertLevelsetToReal (LevelsetGrid &source , Grid<Real> &target)
{
	FOR_IJK(target) {
		target(i,j,k) = source(i,j,k);
	}
}


template<class T> void Grid<T>::printGrid(int zSlice, bool printIndex) {
	std::ostringstream out;
	out << std::endl;
	const int bnd = 1;
	FOR_IJK_BND(*this,bnd) {
		int idx = (*this).index(i,j,k);
		if(zSlice>=0 && k==zSlice) { 
			out << " ";
			if(printIndex) out << "  "<<i<<","<<j<<","<<k <<":";
			out << (*this)[idx]; 
			if(i==(*this).getSizeX()-1 -bnd) out << std::endl; 
		}
	}
	out << endl; debMsg("Printing " << this->getName() << out.str().c_str() , 1);
}

// helper functions for UV grid data (stored grid coordinates as Vec3 values, and uv weight in entry zero)

// make uv weight accesible in python
PYTHON Real getUvWeight (Grid<Vec3> &uv) { return uv[0][0]; }

// note - right now the UV grids have 0 values at the border after advection... could be fixed with an extrapolation step...

// compute normalized modulo interval
static inline Real computeUvGridTime(Real t, Real resetTime) {
	return fmod( (t / resetTime), (Real)1. );
}
// create ramp function in 0..1 range with half frequency
static inline Real computeUvRamp(Real t) {
	Real uvWeight = 2. * t; 
	if (uvWeight>1.) uvWeight=2.-uvWeight;
	return uvWeight;
}

KERNEL void knResetUvGrid (Grid<Vec3>& target) { target(i,j,k) = Vec3((Real)i,(Real)j,(Real)k); }

PYTHON void resetUvGrid (Grid<Vec3> &target)
{
	knResetUvGrid reset(target); // note, llvm complains about anonymous declaration here... ?
}
PYTHON void updateUvWeight(Real resetTime, int index, int numUvs, Grid<Vec3> &uv , bool info=false)
{
	const Real t   = uv.getParent()->getTime();
	Real  timeOff  = resetTime/(Real)numUvs;

	Real lastt = computeUvGridTime(t +(Real)index*timeOff - uv.getParent()->getDt(), resetTime);
	Real currt = computeUvGridTime(t +(Real)index*timeOff                  , resetTime);
	Real uvWeight = computeUvRamp(currt);

	// normalize the uvw weights , note: this is a bit wasteful...
	Real uvWTotal = 0.;
	for(int i=0; i<numUvs; ++i) {
		uvWTotal += computeUvRamp( computeUvGridTime(t +(Real)i*timeOff , resetTime) );
	}
	if(uvWTotal<=VECTOR_EPSILON) { uvWeight =  uvWTotal = 1.; }
	else                           uvWeight /= uvWTotal;

	// check for reset
	if( currt < lastt ) 
		knResetUvGrid reset( uv );

	// write new weight value to grid
	uv[0] = Vec3( uvWeight, 0.,0.);

	// print info about uv weights?
	if(info) debMsg("Uv grid "<<index<<"/"<<numUvs<< " t="<<currt<<" w="<<uvWeight<<", reset:"<<(int)(currt<lastt) , 1);
}

PYTHON void setBoundaries(Grid<Real>& grid, Real value=0., int boundaryWidth=1) {
	const int w = boundaryWidth;
	FOR_IJK(grid) {
		bool bnd = (i<=w || i>=grid.getSizeX()-1-w || j<=w || j>=grid.getSizeY()-1-w || (grid.is3D() && (k<=w || k>=grid.getSizeZ()-1-w)));
		if (bnd) 
			grid(i,j,k) = value;
	}
}

//******************************************************************************
// Specialization classes

void FlagGrid::initDomain(int boundaryWidth) {
	FOR_IDX(*this)
		mData[idx] = TypeEmpty;
	initBoundaries(boundaryWidth);
}

void FlagGrid::initBoundaries(int boundaryWidth) {
	const int w = boundaryWidth;
	FOR_IJK(*this) {
		bool bnd = (i<=w || i>=mSize.x-1-w || j<=w || j>=mSize.y-1-w || (is3D() && (k<=w || k>=mSize.z-1-w)));
		if (bnd) 
			mData[index(i,j,k)] = TypeObstacle;
	}
}

void FlagGrid::updateFromLevelset(LevelsetGrid& levelset) {
	FOR_IDX(*this) {
		if (!isObstacle(idx)) {
			const Real phi = levelset[idx];
			if (phi <= levelset.invalidTimeValue()) continue;
			
			mData[idx] &= ~(TypeEmpty | TypeFluid); // clear empty/fluid flags
			mData[idx] |= (phi <= 0) ? TypeFluid : TypeEmpty; // set resepctive flag
		}
	}
}   

void FlagGrid::fillGrid(int type) {
	FOR_IDX(*this) {
		if ((mData[idx] & TypeObstacle)==0)
			mData[idx] = (mData[idx] & ~(TypeEmpty | TypeFluid)) | type;
	}
}

// explicit instantiation
template class Grid<int>;
template class Grid<Real>;
template class Grid<Vec3>;

//template void scaledAdd<Real,Real>(const Grid<Real>& a, const Grid<Real>& b, const Real& factor);

#if ENABLE_GRID_TEST_DATATYPE==1
// instantiate test datatype , not really required for simulations, mostly here for demonstration purposes
template class Grid<nbVector>;
#endif // ENABLE_GRID_TEST_DATATYPE


} //namespace
