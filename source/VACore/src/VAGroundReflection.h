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
 *    Datei:			VAGroundReflection.h
 *
 *    Zweck:			Hilfsfunktionen zur Berechnung von Bodenreflektionen
 *
 *    Autor(en):		Frank Wefers (Frank.Wefers@akustik.rwth-aachen.de)
 *
 *  ---------------------------------------------------------------------------------
 */

// $Id: $

#ifndef __VA_GROUNDREFLECTION_H__
#define __VA_GROUNDREFLECTION_H__

#include <Filtering/VAThirdOctaveMagnitudes.h>
#include <ITAConstants.h>
#include <RG_Vector.h>
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <complex>

#define PI 3.14159265359


// Rückgabe: Fehlerfreie Konstruktion
bool calcGroundReflection( const RG_Vector& vSourcePos, const RG_Vector& vListenerPos,
                           const float fGroundLevel,    // Höhe der Bodenebene [m]
                           float& fPathLength,          // Länge des Reflektionspfades [m]
                           float& fReflectionAngleRAD ) // (Einfalls-/Ausfalls-)winkel [Bodenmaß]
{
	/*
	 *  Ansatz: - 3D => 2D konvertieren. Hörer stets im Nullpunkt.
	 *          - Spekulare Reflektion an der Bodenebene (plan!)
	 *          - Berechnung des Reflektionspunktes (2D)
	 *          - Bestimmung des Umweges (Pfadlänge)
	 *
	 *  Herleitung: [fwe] Blatt Papier + Maple ;-)
	 */

	// Abstand Quelle<->Hörer bei Projektion auf den Boden
	float t1 = vSourcePos.x - vListenerPos.x;
	float t2 = vSourcePos.z - vListenerPos.z;
	float sx = sqrt( t1 * t1 + t2 * t2 );

	// Höhen Quelle, Hörer relativ zum Bodenniveau
	float sy = vSourcePos.y - fGroundLevel;
	float ly = vListenerPos.y - fGroundLevel;

	if( ( sy < 0 ) || ( ly <= 0 ) )
		return false;

	// assert( sy+ly > 0 ); // Klären was passieren soll wenn sich Hörer und Quelle auf einer Ebene mit dem Bodenniveau befinden
	if( ( sy + ly ) == 0.0f )
		return false;

	// Entfernung des Reflektionspunktes vom Hörer
	// Der Punkt liegt natürlich in der Bodenebene yr=0
	float xr = ( sx * ly ) / ( sy + ly );

	// Pfadlänge
	t1          = ( sx - xr );
	t2          = xr;
	fPathLength = sqrt( t1 * t1 + sy * sy ) + sqrt( t2 * t2 + ly * ly );

	// Reflektionswinkel
	fReflectionAngleRAD = atan2( sx, sy + ly );

	return true;
}

// Winkelabhängiger Absorptionsgrad als Funktion des
// Wandnormalen Absorptiongrades
float calcAngleDependentAbsorptionCoeff( const float alpha_norm, const float phiRAD )
{
	/*
	 *  Herleitung wiefolgt:
	 *  - R_norm = sqrt(1-alpha_norm)
	 *  - Daraus Z_norm = Z0*(1+R)/(1-R)
	 *  - Dann hieraus R_phi = [Z_norm*cos(phi)-Z0] / [Z_norm*cos(phi)+Z0]
	 *  - Dies dann wieder zu alpha_phi = 1-R_phi^2
	 *
	 *  Man nehme Maple und koche diese Suppe durch... et voila:
	 */

	float cp        = cos( phiRAD );
	float alpha_phi = alpha_norm * cp / ( 4 * ( cp * ( 1 + sqrt( 1 - alpha_norm ) ) + cp + 1 ) );
	return alpha_phi;
}

// Absorptionskoeffizienten für eine Bodenreflektion bestimmen [Faktoren, nicht Dezibel]
void getGroundReflectionAbsorption( const float fReflectionAngleRAD, CVAThirdOctaveMagnitudes& oTOGains )
{
	/*
	 *  Berechnung mittels empirischer Formel nach
	 *  Delaney, Bazley, "Acoustical properties of fibrous absorbent materials",
	 *  Applied Acoustics, Vol.3, No. 2, 1970
	 */

	float rho   = 1.225f; // Statische Luftdichte [kg/m³]
	float sigma = 250000; // Spezifischer Strömungswiderstand des Bodens (Standardwert nach Paper) [kg/(sm³)]

	for( int i = 0; i < 31; i++ )
	{
		const float f = CVAThirdOctaveMagnitudes::fCenterFreqs[i];

		float eta = 2 * PI * rho * f / sigma;

		std::complex<float> Z( 1 + exp( log( 6.86f * eta ) * ( -0.75f ) ), exp( log( 4.36f * eta ) * ( -0.73f ) ) );
		std::complex<float> nu  = std::complex<float>( 1, 0 ) / Z;
		float cp                = cos( fReflectionAngleRAD );
		std::complex<float> Rt1 = cp - nu;
		std::complex<float> Rt2 = cp + nu;
		std::complex<float> R   = Rt1 / Rt2;
		float Rabs              = sqrt( R.real( ) * R.real( ) + R.imag( ) * R.imag( ) );

		oTOGains[i] = ( std::max )( 0.0f, Rabs );
	}
	/*
	// Absorptionsgrad der Bodenoberfläche
	// Hier: Pflaster aus RAVEN
	float alpha[31] = {
	    0.12f,0.12f,0.12f,0.13f,0.14f,0.15f,0.15f,0.15f,0.15f,0.183333f,0.216667f,0.25f,0.3f,0.35f,0.4f,0.45f,0.5f,0.55f,0.566667f,0.583333f,0.6f,0.6f,0.6f,0.6f,0.6f,0.6f,0.6f,0.6f,0.6f,0.6f,0.6
	};

	for (int i=0; i<31; i++)
	    oTOGains[i] = 1-calcAngleDependentAbsorptionCoeff(alpha[i], fReflectionAngleRAD);
	//oTOGains.Identity();
	*/
}

#endif // __VA_GROUNDREFLECTION_H__
