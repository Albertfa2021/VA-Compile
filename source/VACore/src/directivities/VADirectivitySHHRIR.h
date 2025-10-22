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
 *    Datei:			VAHRIRDatasetSH.h
 *
 *    Zweck:			HRIR-Datensätze mittels Spherical Harmonics (SH)
 *
 *    Autor(en):		Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)
 *
 *  ---------------------------------------------------------------------------------
 */

// $Id: VAHRIRDatasetSH.h 2773 2012-07-02 08:29:37Z fwefers $

#ifndef __VA_HRIRDATASET_SH__
#define __VA_HRIRDATASET_SH__

#include "VAHRIRDataset.h"

#include <string>

/**
 * Diese Klasse implementiert HRIR-Datensätze welche als Spherical Harmonics (SH)
 * gespeichert sind.
 */

#define USEFLOAT 1

#include <complex>
#include <vector>
//#include "VAHRIRDatasetSHHeader.h"

#if( USEFLOAT == 1 )
typedef Ipp32fc ippDataType;
typedef Ipp32f ippRealDataType;
typedef IppsFFTSpec_C_32fc ippFftData;
#else
typedef Ipp64fc ippDataType;
typedef Ipp64f ippRealDataType;
typedef IppsFFTSpec_C_64fc ippFftData;
#endif


typedef std::vector<std::vector<ippDataType*> > ippDataPointerVector;
typedef std::vector<std::vector<contentHeader> > freqHeaderVector;


class CVAHRIRDatasetSH : public IVAHRIRDataset
{
public:
	//! Lade-Konstruktor. Wirft CVAException im Fehlerfall.
	CVAHRIRDatasetSH( const std::string& sFilename, const std::string& sName, double dDesiredSamplerate );

	//! Destruktor
	virtual ~CVAHRIRDatasetSH( );

	// --= Schnittstelle "IVAHRIRDataset" =-----------------------

	std::string GetFilename( ) const;
	std::string GetName( ) const;
	std::string GetDesc( ) const;
	const CVAHRIRDatasetProperties* GetProperties( ) const;

	void GetNearestNeighbour( float fAzimuthDeg, float fElevationDeg, int* piIndex, bool* pbOutOfBounds = NULL );

	void GetHRIRByIndex( ITASampleFrame* psfDest, const int iIndex, const float fDistanceMeters );

	void GetHRIR( ITASampleFrame* psfDest, const float fAzimuthDeg, const float fElevationDeg, const float fDistanceMeters, int* piIndex = NULL,
	              bool* pbOutOfBounds = NULL );

	void getFilter( int iChannel, double dAzimuth, double dElevation, double dRadius, ippRealDataType* impulseResponse );
	int getNumFreq( int channelIndex ) const;
	static ippRealDataType* myIppRealMalloc( int length );
	void writeTestData( );

protected:
	virtual void init( );
	// generate the hankel lut used in range extrapolation
	void generateHankelLUT( );
	bool getUseHankel( double k, double r, int numCoeff );

	// ipp
	ippDataType* myIppMalloc( int length );
	// static ippRealDataType* myIppRealMalloc(int length);
	void dotProd( ippDataType* source1, ippDataType* source2, int len, ippDataType* dest );
	void mulPro( ippDataType* source1, ippDataType* source2, ippDataType* dest, int len );
	void mulFactor( ippDataType* source, ippDataType* dest, ippDataType factor, int len );
	void mulFactorIc( ippDataType* sourcedest, ippDataType factor, int len );
	void mulProI( ippDataType* source1, ippDataType* sourcedest, int len );
	void addC_I( ippDataType* pSrc, ippDataType val, int len );
	void maxVector( const ippRealDataType* source, ippRealDataType* maxValue, int len );
	void mulFactorI( ippRealDataType* sourceDest, ippRealDataType factor, int len );


	// fft methods
	void initFFT( );
	void freeFFT( );
	void iFFT_I( ippRealDataType* dest, int length );

	// get properties
	int getNumChannels( ) const { return numChannels; }
	// int getNumFreq(int channelIndex) const;
	int getLength( int channelIndex ) const;
	int getMaxSHOrder( ) const;
	int getMaxNumFreq( ) const;
	int getNumSHs( int channelIndex, int freqIndex ) const;
	bool getOffset( int channelIndex, int freqIndex, double& x, double& y, double& z ) const;

	bool getFrequencies( int channelIndex, std::vector<double>& output ) const;

	// data functions
	bool getData( int channelIndex, int freqIndex, std::vector<ippDataType>& output ) const;
	int getRawData( int channelIndex, int freqIndex, ippDataType** data ) const;

	// void getFilter(int iChannel, double dAzimuth, double dElevation, double dRadius, ippRealDataType* impulseResponse);
	// void getFilterRaw(int iChannel, double dAzimuth, double dElevation, double dRadius, complexD* pfDest) const;


	// hankel function
	void getHankelFunction( int order, int frequencyIndex, double r, ippDataType* data );
	void getHankelFromLUT( int order, int frequencyIndex, double r, ippDataType* data );

	// file io
	bool readFile( const std::string& fileName );
	// debug file io
	void writeFile( const std::string& fileName, int size, ippDataType* data ) const;
	void writeRealFile( const std::string& fileName, int size, ippRealDataType* data );

	// sh-dirac
	bool generateSpatialDirac( double azimuth, double elevation, double radius, double order, ippDataType* data ) const;
	bool calculateSH( int m, int n, double theta, double phi, ippDataType& returnValue ) const;
	double faculty( double x ) const;

	// coordinate system
	void transformCoordinateSystem( int iChannel, double& dRadius, double& dElevation, double& dAzimuth, double& dMeasurementDistance );

private:
	CVAHRIRDatasetProperties m_oProps;
	std::string m_sFilename;
	std::string m_sName;
	bool m_b3D;
	int m_iSHOrder;
	float m_fLatency;
	int m_iFilterLength;

	ippDataPointerVector dataVector;
	freqHeaderVector freqVector;
	std::string fileName;
	int numChannels;
	std::string comment;
	int maxSHCoeff;

	double measurementDistance;
	double pi;
	double c;
	double samplingRate;
	int numFreq;
	ippDataType* dirac;
	ippDataType* hankelVector;
	ippDataType* hankelCoeff;
	ippDataType* transferFunction;
	// ippDataType* tempFunction;

	std::vector<std::vector<std::vector<ippDataType*> > > hankelLUT;
	int numPoints;
	double dHankel;

	std::vector<int> maxIndexLUT;

	// fft data
	ippFftData* fftStruct;
	Ipp8u* fftBuffer;
	int fftBufferSize;
};

#endif // __VA_HRIRDATASET_SH__
