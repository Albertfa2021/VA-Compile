/*
 *
 *    VVV        VVV A
 *     VVV      VVV AAA        Virtual Acoustics
 *      VVV    VVV   AAA       Real-time auralisation for virtual reality
 *       VVV  VVV     AAA
 *        VVVVVV       AAA     (c) Copyright Institut für Technische Akustik (ITA)
 *         VVVV         AAA        RWTH Aachen (http://www.akustik.rwth-aachen.de)
 *
 *  ---------------------------------------------------------------------------------
 *
 *    Datei:			VAHRIRDatasetSH.cpp
 *
 *    Zweck:			HRIR-Datensätze mittels Spherical Harmonics (SH)
 *
 *    Autor(en):		Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)
 *
 *  ---------------------------------------------------------------------------------
 */

// $Id: VAHRIRDatasetSH.cpp 2773 2012-07-02 08:29:37Z fwefers $

#include "VAHRIRDatasetSH.h"

#include <ITANumericUtils.h>
#include <ITASampleFrame.h>
#include <ITAStringUtils.h>
#include <VAException.h>
#include <cassert>
#include <cmath>
#include <sstream>
//#include <boost/math/special_functions/legendre.hpp>
#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/math/special_functions/spherical_harmonic.hpp>
#include <math.h>
#include <stdio.h>

CVAHRIRDatasetSH::CVAHRIRDatasetSH( const std::string& sFilename, const std::string& sName, double dDesiredSamplerate )
    : m_sFilename( sFilename )
    , m_sName( sName )
    , pi( 4.0 * std::atan2( 1.0, 1.0 ) )
    , c( 340 )
    , dirac( 0 )
    , hankelVector( 0 )
    , hankelLUT( 0 )
    , hankelCoeff( 0 )
    , numFreq( 0 )
    , fftStruct( 0 )
    , fftBuffer( 0 )
    , fftBufferSize( 0 )
    , maxSHCoeff( 0 )
{
	/* Fehler so ...
	    VA_EXCEPT1( std::string("Could not load HRIR dataset from file \"") +
	                sFilename + std::string("\".") );
	*/

	// TODO: Wichtig Latenzwert muss bekannt sein
	m_b3D      = true; // 3D-Interpolation
	m_iSHOrder = 42;
	// m_iFilterLength = 4711;
	m_iFilterLength = 256;
	m_fLatency      = 0.815F;

	// Eigenschaften definieren
	m_oProps.sFilename          = sFilename;
	m_oProps.sName              = sName;
	m_oProps.iFilterLength      = m_iFilterLength;
	m_oProps.fFilterLatency     = m_fLatency;
	m_oProps.bFullSphere        = true;
	m_oProps.bSpaceDiscrete     = false;
	m_oProps.bDistanceDependent = m_b3D;

	// read the file
	readFile( sFilename );
	init( );
}

CVAHRIRDatasetSH::~CVAHRIRDatasetSH( )
{
	freqVector.clear( );
	dataVector.clear( );
	ippsFree( dirac );
	ippsFree( hankelVector );
	ippsFree( hankelCoeff );
	freeFFT( );

	// free the hankel lut
	hankelLUT.clear( );
}

std::string CVAHRIRDatasetSH::GetFilename( ) const
{
	return m_sFilename;
}

std::string CVAHRIRDatasetSH::GetName( ) const
{
	return m_sName;
}

std::string CVAHRIRDatasetSH::GetDesc( ) const
{
	char buf[1024];
	sprintf( buf, "SH order %d, %s, full sphere, continuous, %d taps", m_iSHOrder, ( m_b3D ? "3D" : "2D" ), m_oProps.iFilterLength );
	return buf;
}

const CVAHRIRDatasetProperties* CVAHRIRDatasetSH::GetProperties( ) const
{
	return &m_oProps;
}

void CVAHRIRDatasetSH::GetNearestNeighbour( float fAzimuthDeg, float fElevationDeg, int* piIndex, bool* pbOutOfBounds )
{
	VA_EXCEPT1( "Method not supported for space continuous data" );
}

void CVAHRIRDatasetSH::GetHRIRByIndex( ITASampleFrame* psfDest, const int iIndex, const float fDistanceMeters )
{
	VA_EXCEPT1( "Method not supported for space continuous data" )
}

void CVAHRIRDatasetSH::GetHRIR( ITASampleFrame* psfDest, const float fAzimuthDeg, const float fElevationDeg, const float fDistanceMeters, int* piIndex,
                                bool* pbOutOfBounds )
{
	// TODO;


	// This is continuous data... No index. Always full sphere.
	if( piIndex )
		*piIndex = 0;
	if( pbOutOfBounds )
		*pbOutOfBounds = false;

	// check psfDest
	if( !psfDest )
		return;

	// check numChannels/numFreq
	if( psfDest->channels( ) != this->getNumChannels( ) )
		return;

	if( psfDest->length( ) != this->getLength( 0 ) )
		return;

	ippRealDataType* data = 0;

	for( int channelIndex = 0; channelIndex < psfDest->channels( ); ++channelIndex )
	{
		data = ( *psfDest )[channelIndex].data( );
		// data = myIppRealMalloc(getNumFreq(0)*2-2);
		this->getFilter( channelIndex, fAzimuthDeg, fElevationDeg, fDistanceMeters, data );
	}
}

void CVAHRIRDatasetSH::init( )
{
	int order    = getMaxSHOrder( );
	int numCoeff = std::pow( (double)order + 1, 2 );
	if( numCoeff > 0 )
	{
		// find the nearest multiple of two
		int modulo = numCoeff % 2;
		if( modulo != 0 )
		{
			numCoeff += 1;
		}
		dirac        = myIppMalloc( numCoeff );
		hankelVector = myIppMalloc( numCoeff );
		hankelCoeff  = myIppMalloc( numCoeff );
	}
	int maxNumFreq = getMaxNumFreq( );
	if( maxNumFreq > 0 )
	{
		// tempFunction = myIppMalloc((maxNumFreq-1)*2);
		transferFunction = myIppMalloc( ( maxNumFreq - 1 ) * 2 );
	}
	generateHankelLUT( );
	initFFT( );
}

void CVAHRIRDatasetSH::generateHankelLUT( )
{
	int orderIndex = 0;
	int rIndex     = 0;
	int maxOrder   = getMaxSHOrder( );
	dHankel        = 0.01;
	numPoints      = 2 / dHankel;

	double freqDist = samplingRate / 2 / ( numFreq - 1 );
	double k;
	double kr;
	ippDataType* hankel = 0;
	hankelLUT.resize( numFreq );
	std::complex<long double> referenceHankel;
	std::complex<long double> tmpHankel, tmpHankel2;
	for( int freqIndex = 1; freqIndex < numFreq; ++freqIndex )
	{
		hankelLUT[freqIndex].resize( maxOrder + 1 );
		// calculate the frequency
		k = 2 * pi * freqIndex * freqDist / c;

		for( orderIndex = 0; orderIndex <= maxOrder; ++orderIndex )
		{
			hankelLUT[freqIndex][orderIndex].resize( numPoints );
			// calculate the reference
			referenceHankel.real( boost::math::sph_bessel( orderIndex, k * measurementDistance ) );
			referenceHankel.imag( -boost::math::sph_neumann( orderIndex, k * measurementDistance ) );
			for( rIndex = 1; rIndex <= numPoints; ++rIndex )
			{
				// calculate kr
				kr     = k * rIndex * dHankel;
				hankel = myIppMalloc( 1 );
				tmpHankel.real( boost::math::sph_bessel( orderIndex, kr ) );
				tmpHankel.imag( -boost::math::sph_neumann( orderIndex, kr ) );
				// divide hankel and referenceHankel
				tmpHankel2 = tmpHankel / referenceHankel;
				// tmpHankel2 = tmpHankel;

				hankel->re                                   = tmpHankel2.real( );
				hankel->im                                   = tmpHankel2.imag( );
				hankelLUT[freqIndex][orderIndex][rIndex - 1] = hankel;
			}
		}
	}

	maxIndexLUT.resize( maxOrder + 1 );
	for( orderIndex = 0; orderIndex <= maxOrder; ++orderIndex )
	{
		maxIndexLUT[orderIndex] = pow( (double)( orderIndex + 1 ), 2 );
	}
}

bool CVAHRIRDatasetSH::getUseHankel( double k, double r, int maxSHOrder )
{
	// polyfit revealed that the 0.25 db error line between 1/r and hankel
	// lies on y = (6.143*r-0.606)*maxOrder + 2.0843*r-0.3318
	// so if kr > y for order x then don't use hankel (but 1/r)
	if( k * r > ( 6.1435 * r - 0.606 ) * maxSHOrder + 2.0843 * r - 0.3318 )
	{
		return false;
	}
	return true;
}

// ipp functions
ippDataType* CVAHRIRDatasetSH::myIppMalloc( int length )
{
#if( USEFLOAT == 1 )
	return ippsMalloc_32fc( length );

#else
	return ippsMalloc_64fc( length );
#endif
}

ippRealDataType* CVAHRIRDatasetSH::myIppRealMalloc( int length )
{
#if( USEFLOAT == 1 )
	return ippsMalloc_32f( length );

#else
	return ippsMalloc_64f( length );
#endif
}

void CVAHRIRDatasetSH::dotProd( ippDataType* source1, ippDataType* source2, int len, ippDataType* dest )
{
#if( USEFLOAT == 1 )
	ippsDotProd_32fc( source1, source2, len, dest );
#else
	ippsDotProd_64fc( source1, source2, len, dest );
#endif
}

void CVAHRIRDatasetSH::mulProI( ippDataType* source1, ippDataType* sourcedest, int len )
{
#if( USEFLOAT == 1 )
	ippsMul_32fc_I( source1, sourcedest, len );
#else
	ippsMul_64fc_I( source1, sourcedest, len );
#endif
}

void CVAHRIRDatasetSH::mulPro( ippDataType* source1, ippDataType* source2, ippDataType* dest, int len )
{
#if( USEFLOAT == 1 )
	ippsMul_32fc( source1, source2, dest, len );
#else
	ippsMul_64fc( source1, source2, dest, len );
#endif
}

void CVAHRIRDatasetSH::addC_I( ippDataType* pSrc, ippDataType val, int len )
{
#if( USEFLOAT == 1 )
	ippsAddC_32fc_I( val, pSrc, len );
#else
	ippsAddC_64fc_I( val, pSrc, len );
#endif
}

void CVAHRIRDatasetSH::mulFactor( ippDataType* source, ippDataType* dest, ippDataType factor, int len )
{
#if( USEFLOAT == 1 )
	ippsMulC_32fc( source, factor, dest, len );
#else
	ippsMulC_64fc( source, factor, dest, len );
#endif
}

void CVAHRIRDatasetSH::mulFactorIc( ippDataType* sourceDest, ippDataType factor, int len )
{
#if( USEFLOAT == 1 )
	ippsMulC_32fc_I( factor, sourceDest, len );
#else
	ippsMulC_64fc_I( factor, sourceDest, len );
#endif
}

void CVAHRIRDatasetSH::mulFactorI( ippRealDataType* sourceDest, ippRealDataType factor, int len )
{
#if( USEFLOAT == 1 )
	ippsMulC_32f_I( factor, sourceDest, len );
#else
	ippsMulC_64f_I( factor, sourceDest, len );
#endif
}

void CVAHRIRDatasetSH::maxVector( const ippRealDataType* source, ippRealDataType* maxValue, int len )
{
#if( USEFLOAT == 1 )
	ippsMax_32f( source, len, maxValue );
#else
	ippsMax_64f( source, len, maxValue );
#endif
}

// fft methods
void CVAHRIRDatasetSH::initFFT( )
{
	// ld of numFreq
	int order = std::log( (double)numFreq ) / std::log( (double)2 );
	++order;
#if( USEFLOAT == 1 )
	ippsFFTInitAlloc_C_32fc( &fftStruct, order, IPP_FFT_DIV_INV_BY_N, ippAlgHintFast );
	ippsFFTGetBufSize_C_32fc( fftStruct, &fftBufferSize );
#else
	ippsFFTInitAlloc_C_64fc( &fftStruct, order, IPP_FFT_DIV_INV_BY_N, ippAlgHintFast );
	ippsFFTGetBufSize_C_64fc( fftStruct, &fftBufferSize );
#endif

	fftBuffer = ippsMalloc_8u( fftBufferSize );
}

void CVAHRIRDatasetSH::iFFT_I( ippRealDataType* dest, int length )
{
#if( USEFLOAT == 1 )
	transferFunction[0].im      = 0;
	transferFunction[length].im = 0;
	// converts the transferFunction vector to the "complex data" format
	ippsConjCcs_32fc_I( transferFunction, length * 2 - 2 );
	// fft
	ippsFFTInv_CToC_32fc_I( transferFunction, fftStruct, fftBuffer );
	// return only the real values of transferFunction to dest
	ippsReal_32fc( transferFunction, dest, length * 2 - 2 );
#else
	transferFunction[0].im      = 0;
	transferFunction[length].im = 0;
	// converts the transferFunction vector to the "complex data" format
	ippsConjCcs_64fc_I( transferFunction, length * 2 - 2 );
	// fft
	ippsFFTInv_CToC_64fc_I( transferFunction, fftStruct, fftBuffer );
	// return only the real values of transferFunction to dest
	ippsReal_64fc( transferFunction, dest, length * 2 - 2 );
#endif
}

void CVAHRIRDatasetSH::freeFFT( )
{
#if( USEFLOAT == 1 )
	ippsFFTFree_C_32fc( fftStruct );
#else
	ippsFFTFree_C_64fc( fftStruct );
#endif
	ippsFree( fftBuffer );
}

// properties
int CVAHRIRDatasetSH::getNumFreq( int channelIndex ) const
{
	if( ( channelIndex < numChannels ) && ( channelIndex >= 0 ) )
	{
		return freqVector.at( channelIndex ).size( );
	}
	return 0;
}

int CVAHRIRDatasetSH::getLength( int channelIndex ) const
{
	int numFreq = getNumFreq( channelIndex );
	return numFreq * 2 - 2;
}

int CVAHRIRDatasetSH::getMaxSHOrder( ) const
{
	return std::sqrt( (double)maxSHCoeff ) - 1;
}

int CVAHRIRDatasetSH::getMaxNumFreq( ) const
{
	return numFreq;
}

int CVAHRIRDatasetSH::getNumSHs( int channelIndex, int freq ) const
{
	if( ( channelIndex >= numChannels ) || ( channelIndex < 0 ) )
	{
		return -1;
	}
	int numFreq = getNumFreq( channelIndex );
	if( ( freq >= numFreq ) || ( freq < 0 ) )
	{
		return -1;
	}

	return freqVector[channelIndex][freq].numSH;
}

bool CVAHRIRDatasetSH::getFrequencies( int channelIndex, std::vector<double>& returnVector ) const
{
	return false;
}

bool CVAHRIRDatasetSH::getData( int channelIndex, int freqIndex, std::vector<ippDataType>& returnVector ) const
{
	int numSH = getNumSHs( channelIndex, freqIndex );
	if( numSH < 0 )
	{
		return false;
	}

	returnVector.resize( numSH );
	ippDataType* data = dataVector[channelIndex][freqIndex];
	for( int shIndex = 0; shIndex < numSH; ++shIndex )
	{
		returnVector[shIndex] = data[shIndex];
	}

	return true;
}

int CVAHRIRDatasetSH::getRawData( int channelIndex, int freqIndex, ippDataType** data ) const
{
	int numSH = getNumSHs( channelIndex, freqIndex );
	if( numSH < 0 )
	{
		return 0;
	}
	*data = dataVector[channelIndex][freqIndex];
	return numSH;
}

void CVAHRIRDatasetSH::getFilter( int iChannel, double dAzimuth, double dElevation, double dRadius, ippRealDataType* impulseResponse )
{
	// get the spatial dirac at the desired direction
	int order                   = getMaxSHOrder( );
	int numCoeff                = std::pow( (double)order + 1, 2 );
	double dOldRadius           = dRadius;
	double dMeasurementDistance = measurementDistance;
	// transform the input parameters into the local ear coordinate system
	transformCoordinateSystem( iChannel, dRadius, dElevation, dAzimuth, dMeasurementDistance );

	bool ok = generateSpatialDirac( dAzimuth, dElevation, dRadius, order, dirac );
	if( !ok )
		return;

	int numFreq         = getNumFreq( iChannel );
	double freqBinWidth = samplingRate / 2 / ( numFreq - 1 );
	int shIndex         = 0;
	int numSH           = 0;
	int maxSize         = 0;
	ippDataType* data   = 0;
	double k;
	double kr;
	std::complex<double> approxHankelComplex;
	ippDataType approxHankel;
	approxHankel.im = 0;
	bool useHankel  = true;
#pragma omp parallel for
	bool rangeExtrapolation = true;
	for( int freqIndex = 0; freqIndex < numFreq; ++freqIndex )
	{
		numSH   = getRawData( iChannel, freqIndex, &data );
		maxSize = std::min( numCoeff, numSH );
		// range extrapolation
		// for this we need to calculate the hankel function
		if( rangeExtrapolation )
		{
			k         = 2 * pi * freqIndex * freqBinWidth / c;
			kr        = k * dRadius;
			useHankel = getUseHankel( k, dRadius, order );
			if( ( freqIndex > 0 ) && ( useHankel ) )
			{
				getHankelFromLUT( sqrt( (double)maxSize ) - 1, freqIndex, dRadius, hankelVector );
				// getHankelFunction(sqrt((double)maxSize)-1,freqIndex,dRadius,hankelVector);
				mulPro( hankelVector, data, hankelCoeff, maxSize + 1 );

				approxHankelComplex = std::exp( std::complex<double>( 0, 2 * pi * freqIndex * freqBinWidth * ( -1 + dOldRadius ) / c ) );
				approxHankelComplex *= dOldRadius;
				approxHankel.re = approxHankelComplex.real( );
				approxHankel.im = approxHankelComplex.imag( );
				mulFactorIc( hankelCoeff, approxHankel, maxSize + 1 );

				dotProd( hankelCoeff, dirac, maxSize, &( transferFunction[freqIndex] ) );
			}
			else
			{
				// if the radius is different, VA will devide by a wrong 1/r correction term
				// this is anticipated and corrected here
				//   approxHankelComplex = std::exp(std::complex<double>(0,2*pi*freqIndex*freqBinWidth*(1+dMeasurementDistance+dOldRadius-dRadius)/c));
				// approxHankelComplex *= dMeasurementDistance*dOldRadius/(dRadius);
				// approxHankel.re = dMeasurementDistance;
				// approxHankel.im = approxHankelComplex.imag();
				// mulFactor(data,hankelCoeff,approxHankel,maxSize+1);
				// dotProd(hankelCoeff,dirac,maxSize,&(transferFunction[freqIndex]));
				//}
				// else
				//{
				dotProd( data, dirac, maxSize, &( transferFunction[freqIndex] ) );
				transferFunction[freqIndex].re = ( transferFunction[freqIndex].re ) * dMeasurementDistance;
				transferFunction[freqIndex].im = ( transferFunction[freqIndex].im ) * dMeasurementDistance;
				//}
			}
		}
		else
		{
			dotProd( data, dirac, maxSize, &( transferFunction[freqIndex] ) );
		}
	}

	// writeFile("filterIntern",numFreq,transferFunction);
	// correct an error in the fft function (everything was times2)
	// if not, normalize
	ippRealDataType normValue;
	// compute the fft
	iFFT_I( impulseResponse, numFreq );


	// maxVector(impulseResponse,&normValue,getLength(iChannel));
	////// if the filter is clipping (maximum > 1) normalize
	// if (normValue < 1)
	//{
	//	normValue = 1;
	//}

	// mulFactorI(impulseResponse,1/normValue,getLength(iChannel));
	// writeRealFile("filterIntern",256,impulseResponse);
}


void CVAHRIRDatasetSH::getHankelFunction( int order, int frequencyIndex, double r, ippDataType* data )
{
	int minIndex    = 0;
	int maxIndex    = 0;
	int degreeIndex = -1;


	int maxOrder    = getMaxSHOrder( );
	double freqDist = samplingRate / 2 / ( numFreq - 1 );
	double k        = 2 * pi * frequencyIndex * freqDist / c;

	std::complex<long double> referenceHankel;
	std::complex<long double> tmpHankel;
	ippDataType hankel;
	for( int orderIndex = 0; orderIndex <= order; ++orderIndex )
	{
		minIndex = maxIndex;
		maxIndex = maxIndexLUT[orderIndex];

		// calculate the reference
		referenceHankel.real( boost::math::sph_bessel( orderIndex, k * measurementDistance ) );
		referenceHankel.imag( -boost::math::sph_neumann( orderIndex, k * measurementDistance ) );

		tmpHankel.real( boost::math::sph_bessel( orderIndex, k * r ) );
		tmpHankel.imag( -boost::math::sph_neumann( orderIndex, k * r ) );
		tmpHankel = tmpHankel / referenceHankel;
		hankel.re = tmpHankel.real( );
		hankel.im = tmpHankel.imag( );
		for( degreeIndex = minIndex; degreeIndex < maxIndex; ++degreeIndex )
		{
			data[degreeIndex] = hankel;
		}
	}
	// char buffer[50];
	// sprintf(buffer,"hankel%u",frequencyIndex);
	// writeFile(buffer,maxIndex,data);
}

void CVAHRIRDatasetSH::getHankelFromLUT( int order, int freqIndex, double r, ippDataType* data )
{
	int minIndex    = 0;
	int maxIndex    = 0;
	int degreeIndex = -1;
	ippDataType hankel;
	double u = r / dHankel;
	int rIndex( u );
	u -= rIndex;
	--rIndex;
	if( rIndex >= numPoints - 1 )
	{
		rIndex = numPoints - 2;
	}
	for( int orderIndex = 0; orderIndex <= order; ++orderIndex )
	{
		minIndex = maxIndex;
		maxIndex = maxIndexLUT[orderIndex];
		/* Linear RRW-Interpolation: very fast*/
		// krIndex = std::floor(kr*2)-1;

		hankel.re =
		    hankelLUT[freqIndex][orderIndex][rIndex]->re + ( hankelLUT[freqIndex][orderIndex][rIndex + 1]->re - hankelLUT[freqIndex][orderIndex][rIndex]->re ) * u;
		hankel.im =
		    hankelLUT[freqIndex][orderIndex][rIndex]->im + ( hankelLUT[freqIndex][orderIndex][rIndex + 1]->im - hankelLUT[freqIndex][orderIndex][rIndex]->im ) * u;


		for( degreeIndex = minIndex; degreeIndex < maxIndex; ++degreeIndex )
		{
			// data[degreeIndex].re = hankel.re;
			// data[degreeIndex].im = hankel.im;
			data[degreeIndex] = hankel;
		}
	}
	// char buffer[50];
	// sprintf(buffer,"hankel%u",freqIndex);
	// writeFile(buffer,maxIndex,data);
}

// file io
bool CVAHRIRDatasetSH::readFile( const std::string& fileName )
{
	FILE* m_file = fopen( fileName.c_str( ), "rb" );

	// we assume the fileHeader at the beginning of the file
	fileHeader fHeader;
	fread( &fHeader, 1, FILE_HEADER_SIZE, m_file );

	// todo: check if version number etc match


	mainHeader mHeader;
	fread( &mHeader, 1, MAIN_HEADER_SIZE, m_file );
	numChannels         = mHeader.numChannels;
	samplingRate        = mHeader.samplingRate;
	measurementDistance = mHeader.measurementDistance;
	numFreq             = mHeader.samplingRate / mHeader.filterLength;
	// numFreq = 512;
	// read all the content header
	contentHeader cHeader;
	freqVector.resize( numChannels );
	int freqIndex = 0;
	for( int channelIndex = 0; channelIndex < numChannels; ++channelIndex )
	{
		freqVector[channelIndex].resize( numFreq );
		for( freqIndex = 0; freqIndex < numFreq; ++freqIndex )
		{
			fread( &cHeader, 1, CONTENT_HEADER_SIZE, m_file );
			freqVector[channelIndex][freqIndex] = cHeader;

			// get the max SH number
			if( cHeader.numSH > maxSHCoeff )
			{
				maxSHCoeff = cHeader.numSH;
			}
		}
	}
	dataVector.resize( numChannels );
	ippDataType* data = 0;
	int numCoeff;
	for( int earsIndex = 0; earsIndex < numChannels; ++earsIndex )
	{
		dataVector[earsIndex].resize( numFreq );
		for( freqIndex = 0; freqIndex < numFreq; ++freqIndex )
		{
			cHeader = freqVector[earsIndex][freqIndex];
			fseek( m_file, cHeader.dataOffset, 0 );

			numCoeff   = cHeader.numSH;
			int modulo = numCoeff % 2;
			if( modulo != 0 )
			{
				numCoeff += 1;
			}
			data = myIppMalloc( numCoeff );
			fread( data, cHeader.numSH, sizeof( ippDataType ), m_file );

			dataVector[earsIndex][freqIndex] = data;
		}
	}

	// read the comment
	// int commentLength = 0;
	// fread(&commentLength,sizeof(uint64_t),1,m_file);
	// char* commentChar = new char[commentLength];
	// fread(commentChar, sizeof(char),commentLength,m_file);
	// comment.copy(commentChar,commentLength);
	// delete commentChar;
	fclose( m_file );

	return true;
}

// sh-dirac
bool CVAHRIRDatasetSH::generateSpatialDirac( double azimuth, double elevation, double radius, double order, ippDataType* data ) const
{
	// the dirac coeff. for point x are the SHs for every order conj.
	int nIndex = 0;
	int mIndex = 0;

	ippDataType sh, sh2;
	int index     = 0;
	int zeroIndex = 0;
	for( nIndex = 0; nIndex <= order; ++nIndex )
	{
		// takes 70 micro for order 20
		zeroIndex = pow( (double)nIndex + 1, 2 ) - nIndex - 1;
		calculateSH( 0, nIndex, elevation, azimuth, data[zeroIndex] );
		for( mIndex = 1; mIndex <= nIndex; ++mIndex )
		{
			calculateSH( mIndex, nIndex, elevation, azimuth, sh );
			data[zeroIndex + mIndex] = sh;
			if( mIndex % 2 == 0 )
			{
				sh.im                    = -sh.im;
				data[zeroIndex - mIndex] = sh;
			}
			else
			{
				sh2.re                   = -sh.re;
				sh2.im                   = sh.im;
				data[zeroIndex - mIndex] = sh2;
			}
		}
		// takes 130 micro for order 20
		// for (mIndex = -nIndex; mIndex <= nIndex; ++mIndex)
		//{
		//	calculateSH(mIndex,nIndex,elevation,azimuth,data[index++]);
		//}
	}
	return true;
}

bool CVAHRIRDatasetSH::calculateSH( int m, int n, double theta, double phi, ippDataType& returnValue ) const
{
	// double nominator = (2*n+1)*faculty(n-m);
	// double denominator = 4*pi*faculty(n+m);
	// double sqrtTerm = sqrt(nominator/denominator);
	// double legendre = boost::math::legendre_p(n,m,std::cos(theta));
	// complexD phaseTerm = std::exp(complexD(0,m*phi));
	// complexD sh = sqrtTerm*legendre*phaseTerm;
	complexD sh    = boost::math::spherical_harmonic( n, m, theta, phi );
	returnValue.re = std::real( sh );
	returnValue.im = std::imag( sh );
	return true;
}

double CVAHRIRDatasetSH::faculty( double x ) const
{
	double returnDouble = 1;
	for( int i = 2; i <= x; ++i )
	{
		returnDouble *= i;
	}
	return returnDouble;
}

// coordinate system
void CVAHRIRDatasetSH::transformCoordinateSystem( int iChannel, double& dRadius, double& dElevation, double& dAzimuth, double& dMeasurementDistance )
{
	// if (!boost::math::isnormal(dElevation))
	//	dElevation = 0;
	// if (!boost::math::isnormal(dAzimuth))
	//	dAzimuth = 0;
	// convert angles from degree to rad
	dElevation = dElevation * pi / 180;
	dAzimuth   = dAzimuth * pi / 180;


	// elevation 0 is front and not top
	dElevation = pi / 2 - dElevation;

	contentHeader header = freqVector.at( iChannel ).at( 0 );
	if( header.centerY != 0 )
	{
		// transform the input values to cartesian space
		double x = dRadius * sin( dElevation ) * cos( dAzimuth );
		double y = dRadius * sin( dElevation ) * sin( dAzimuth );
		double z = dRadius * cos( dElevation );

		// add the offset
		// currently, only the offset of the first freqValue is considered due to performance issues
		x += header.centerX;
		y += header.centerY;
		z += header.centerZ;

		// transform back
		dRadius    = sqrt( x * x + y * y + z * z );
		dElevation = std::acos( z / dRadius );
		dAzimuth   = std::atan2( y, x );


		// transform the measurement distance
		x = dMeasurementDistance * sin( dElevation ) * cos( dAzimuth );
		y = dMeasurementDistance * sin( dElevation ) * sin( dAzimuth );
		z = dMeasurementDistance * cos( dElevation );

		// add the offset
		// currently, only the offset of the first freqValue is considered due to performance issues
		x += header.centerX;
		y += header.centerY;
		z += header.centerZ;

		// transform back
		dMeasurementDistance = sqrt( x * x + y * y + z * z );
	}
}

void CVAHRIRDatasetSH::writeTestData( )
{
	ippDataType* refData;
	int size = getRawData( 0, 1, &refData );
	// write all coeffs
	// char buffer[50];
	// for (int index = 0; index < getMaxNumFreq(); ++index)
	//{
	//	getRawData(0,index,&refData);
	//	sprintf(buffer,"coeff%u",index);
	//	writeFile(buffer,size,refData);
	//}
	// writeFile("coeff",size,refData);

	// generateSpatialDirac(0.725757228967081,1.574025554005460,0.942756883058575,20,dirac);
	// writeFile("dirac",size,dirac);

	//	getHankelFromLUT(20,100,0.8,hankelVector);
	// writeFile("hankel",size,hankelVector);

	ippRealDataType* data = CVAHRIRDatasetSH::myIppRealMalloc( getNumFreq( 0 ) * 2 - 2 );
	getFilter( 0, 0, 0, 1, data );
	// writeFile("transferfunction",129,transferFunction);
	writeRealFile( "filter", getNumFreq( 0 ) * 2 - 2, data );
}

void CVAHRIRDatasetSH::writeRealFile( const std::string& fileName, int size, ippRealDataType* data )
{
	int dataSize;
	std::string appendix = fileName;
#if( USEFLOAT == 1 )
	dataSize = 4;
	appendix.append( "_float.test" );
#else
	dataSize = 8;
	appendix.append( "_double.test" );
#endif
	FILE* fid = fopen( appendix.c_str( ), "wb" );
	fwrite( &size, sizeof( int ), 1, fid );
	fwrite( &dataSize, sizeof( int ), 1, fid );

	for( int index = 0; index < size; ++index )
	{
		fwrite( &data[index], sizeof( ippRealDataType ), 1, fid );
	}
	fclose( fid );
}

void CVAHRIRDatasetSH::writeFile( const std::string& fileName, int size, ippDataType* data ) const
{
	int dataSize;
	std::string appendix = fileName;
#if( USEFLOAT == 1 )
	dataSize = 4;
	appendix.append( "_float.test" );
#else
	dataSize = 8;
	appendix.append( "_double.test" );
#endif
	FILE* fid = fopen( appendix.c_str( ), "wb" );
	fwrite( &size, sizeof( int ), 1, fid );
	fwrite( &dataSize, sizeof( int ), 1, fid );

	for( int index = 0; index < size; ++index )
	{
		fwrite( &data[index], sizeof( ippDataType ), 1, fid );
	}
	fclose( fid );
}