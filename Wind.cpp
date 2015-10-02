/*
 * Copyright 2010,2011,2012,2013 Robert Huitema robert@42.co.nz
 *
 * This file is part of FreeBoard. (http://www.42.co.nz/freeboard)
 *
 *  FreeBoard is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  FreeBoard is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with FreeBoard.  If not, see <http://www.gnu.org/licenses/>.
 */
/*=== MWV - Wind Speed and Angle ===
 
 ------------------------------------------------------------------------------
 1  2  3  4 5  6
 |  |  |  | |  |
 $--MWV,x.x,a,x.x,a,A*hh<CR><LF>
 ------------------------------------------------------------------------------
 
 Field Number: 
 
 1. Wind Angle, 0 to 360 degrees
 2. Reference, R = Relative, T = True
 3. Wind Speed
 4. Wind Speed Units, Km/Meters/kNots
 5. Status, A = Data Valid
 6. Checksum
 */

/*
 * The wind rotor turns anti-clockwise from below ( eg from the deck).
 *
 * In ULTIMETER Weather Stations, speed is determined by measuring the time interval between
 * two successive closures of the speed reed. Calibration is done as follows (RPS = revolutions
 * per second):
 * 0.010 < RPS < 3.229 (approximately 0.2 < MPH < 8.2):
 * windSpeedDur<309693
 * MPH = -0.1095(RPS2) + 2.9318(RPS) – 0.1412
 * KNTS = -0.09515(RPS2) + 2.5476(RPS) – 0.1226
 *
 * 3.230 < RPS < 54.362 (approximately 8.2 < MPH < 136.0):
 * windSpeedDur < 18395
 * MPH = 0.0052(RPS2) + 2.1980(RPS) + 1.1091
 * KNTS = 0.0045(RPS2) + 1.9099(RPS) + 0.9638
 *
 * 54.363 < RPS < 66.332 (approximately 136.0 < MPH < 181.5):
 *
 * MPH = 0.1104(RPS2) – 9.5685(RPS) + 329.87
 * KNTS = 0.09593(RPS2) – 8.3147(RPS) + 286.65
 *
 * Conversions used are: mph * 0.86897 = knots; mph * 1.6094 = kmph; mph * 0.48037 = m/s
 *
 */

#include "Wind.h"

//lots out here because they are accessed by interrupts
const unsigned long MILLIS_DEBOUNCE = 20ul;
const unsigned long MICROS_DEBOUNCE = 20000ul;
volatile unsigned long lastSpeedPulse, lastDirPulse = 0ul;

volatile unsigned long windSpeedMicros, windSpeedMicrosLast, windSpeedMicrosMean, windSpeedMicrosLastTmp, windSpeedMicrosTmp = 0ul;
volatile unsigned long windDirMicros, windDirMicrosLast = 0ul;

unsigned long windSpeedDur, windDirDur = 0ul;

volatile bool windSpeedFlag;

typedef volatile unsigned long val;
const byte MAX_NUMBER_OF_READINGS = 3;
val dirStorage[MAX_NUMBER_OF_READINGS] = { 0 };

const unsigned int isinTable16[] = { 0, 1144, 2287, 3430, 4571, 5712, 6850, 7987,
		9121, 10252, 11380, 12505, 13625, 14742, 15854, 16962, 18064, 19161, 20251,
		21336, 22414, 23486, 24550, 25607, 26655, 27696, 28729, 29752, 30767, 31772, 32768,

		33753, 34728, 35693, 36647, 37589, 38521, 39440, 40347, 41243, 42125, 42995,
		43851, 44695, 45524, 46340, 47142, 47929, 48702, 49460, 50203, 50930,
		51642, 52339, 53019, 53683, 54331, 54962, 55577, 56174, 56755,

		57318, 57864, 58392, 58902, 59395, 59869, 60325, 60763, 61182, 61583,
		61965, 62327, 62671, 62996, 63302, 63588, 63855, 64103, 64331, 64539, 64728,
		64897, 65047, 65176, 65286, 65375, 65445, 65495, 65525, 65535, };

AverageList<val> dirList = AverageList<val>(dirStorage, MAX_NUMBER_OF_READINGS);

Wind::Wind(FreeBoardModel* model) {
	this->model = model;
	//initialise the wind interrupt
	windSpeedMicros = micros();
	windSpeedMicrosLast = windSpeedMicros;
	//windDirDur = 0;
	windSpeedDur = 0;
	windSpeedFlag = true;
	dirList.reset();

	// read the last wind alarm values
	if (model->getWindAlarmSpeed() > 99) {
		model->setWindAlarmSpeed(99);
	}
}

//routines for fast sin and cos

long Wind::isinLong(long x) {
	boolean pos = true; // positive - keeps an eye on the sign.
	if (x < 0) {
		x = -x;
		pos = !pos;
	}
	if (x >= 360) x %= 360;
	if (x > 180) {
		x -= 180;
		pos = !pos;
	}
	if (x > 90) x = 180 - x;
	//Serial.print("DEBUG:sin angle=");
			//Serial.print(x);
			//Serial.print(", sin=");
			//if(!pos)Serial.print("-");
			//Serial.println(isinTable16[x]);


	if (pos) return isinTable16[x]; // = /65535.0
	return 0l - isinTable16[x];
}

float Wind::isin(long x) {
	return isinLong(x) * 0.0000152590219;
}

long Wind::icosLong(long x) {
	return isinLong(x + 90l);
}

float Wind::icos(long x) {
	return isin(x + 90l);
}

float Wind::itan(long x) {
	return isin(x) / icos(x);
}

float Wind::fsin(float d) {
	float a = isin(d);
	float b = isin(d + 1);
	return a + (d - int(d)) * (b - a);
}

/*
 * return the average of all circular (0-360) values in DEGREES added with an addRotationalValue call
 * Range is 0-360 DEGREES, return is -180 to 180 degrees
 */
int Wind::getRotationalAverage() {
	long x =0;
	long y = 0;
	float xf, yf = 0;

	byte i = 0;

	for (; i < dirList.getCurrentSize(); i++) {
		x = x + icosLong(dirList.getValue(i));
		y = y + isinLong(dirList.getValue(i));

	}
	//TODO:watch out for zeros
	if (x == 0l && y == 0l) {
		return 0l;
	}
	xf = (x* 0.0000152590219) / i;
	yf = (y* 0.0000152590219) / i;

	return degrees(atan2(yf, xf));

}

/* NMEA Wind routines and alarm
 */
void Wind::readWindDataSpeed() {
	//called inside an interrupt so no interrupts execute in here
	//fastest rps = 15ms - avoid bounce
	if ((millis() - lastSpeedPulse) > MILLIS_DEBOUNCE) {
//		if (windSpeedFlag) {
			//called by speed pin interrupt
			windSpeedMicrosLast = windSpeedMicros;
			windSpeedMicros = micros();
			//windSpeedFlag = false;
//		}
		lastSpeedPulse = millis();
	}
}

void Wind::readWindDataDir() {
	if((millis() - lastDirPulse) > MILLIS_DEBOUNCE) {
//		if (!windSpeedFlag) {
			windDirMicrosLast = windDirMicros;
			windDirMicros = micros();
			//need consistent snapshot
			windSpeedMicrosLastTmp = windSpeedMicrosLast;
			windSpeedMicrosTmp = windSpeedMicros;
			//windSpeedFlag = true;
//		}
		lastDirPulse=millis();
	}
}

/*
 * The anemometer turns up to 60rps at 140knots - so min 16ms/16000us per turn
 * Mostly much lower :-)
 */
void Wind::calcWindSpeedAndDir() {
//grab data
// an interrupt could fire in here
	noInterrupts();
	unsigned long wsMicrosLast = windSpeedMicrosLastTmp;
	unsigned long wsMicros = windSpeedMicrosTmp;
	unsigned long wdMicrosLast = windDirMicrosLast;
	unsigned long wdMicros = windDirMicros;
	interrupts();

	/*Serial.print("wsMicros,");
		Serial.print(wsMicros);
		Serial.print(",wdMicros,");
		Serial.print(wdMicros);
		Serial.print(",wsMicrosLast,");
		Serial.print(wsMicrosLast);
		Serial.print(",wdMicrosLast,");
		Serial.println(wdMicrosLast);*/
//micros resets every 50 min,
// avoid 0, bad data, rollover and too fast (bounce? <25ms)
	if (wsMicrosLast >= wsMicros || wsMicros - wsMicrosLast < MICROS_DEBOUNCE) {
		//Serial.println("DEBUG:wsMicrosLast >= wsMicros || wsMicros - wsMicrosLast < MICROS_DEBOUNCE");
		return;
	}
	if (wdMicrosLast >= wdMicros || wdMicros - wdMicrosLast < MICROS_DEBOUNCE) {
		//Serial.println("DEBUG:wdMicrosLast >= wdMicros || wdMicros - wdMicrosLast < MICROS_DEBOUNCE");
		return;
	}
	//wd after ws, both above 0
	if(wdMicros==0 || wdMicrosLast==0 || wsMicros==0 || wsMicrosLast==0 || wdMicrosLast<=wsMicrosLast || wdMicros < wsMicros){
		//Serial.println("DEBUG:wdMicros==0 || wdMicrosLast==0 || wsMicros==0 || wsMicrosLast==0 || wdMicrosLast<=wsMicrosLast || wdMicros < wsMicros");
		return;
	}
//speed in micros, both speed and dir should agree
	//smooth it?
	windSpeedDur = ((wsMicros - wsMicrosLast)+(wdMicros-wdMicrosLast))/2;

//direction
//FROM ULTIMETER:
//AT 130 KNTS ABOUT 46US PER DEGREE OF ROTATION
	// avoid 0, bad data, rollover and too fast (bounce? <25ms)

	//average
	windDirDur = ((wdMicrosLast - wsMicrosLast)+(wdMicros-wsMicros))/2;
	unsigned long windDirDurTmp =0l;
	if (windDirDur > 0 && windDirDur<windSpeedDur) {

		//convert to degrees, this is deg clockwise from arbitrary 'north'
		windDirDurTmp = (windDirDur * 360ul) / windSpeedDur;
		//correct the dir to clockwise
		if(windDirDurTmp>=0 && windDirDurTmp <360){
			dirList.addValue(windDirDurTmp);
		}

	}
	/*Serial.print("wsMicros,");
	Serial.print(wsMicros);
	Serial.print(",wdMicros,");
	Serial.print(wdMicros);
	Serial.print(",wsMicrosLast,");
	Serial.print(wsMicrosLast);
	Serial.print(",wdMicrosLast,");
	Serial.print(wdMicrosLast);
	Serial.print(",windDirDurTmp,");
	Serial.println(windDirDurTmp);*/

}

/*
 * Calculates wind data. Direction is apparent, 0-360 deg off the bow, clockwise, in degrees.
 */
void Wind::calcWindData() {

//fix rollover
	if (millis() < lastSpeedPulse) lastSpeedPulse = millis();
	if (millis() < lastDirPulse) lastDirPulse = millis();

	model->setWindLastUpdate(millis());

//convert to windAverage
	if (millis() - lastSpeedPulse > 3000) {
		//no rotation, no wind
		model->setWindAverage(0);
		//Serial.println("Wind speed reset");
	} else {
		//windSpeedDur is type long -  max sensor value = 3000000 micros
		if (windSpeedDur > 0) {
			// arduino long = -2,147,483,648 to 2,147,483,647
			//1000 millis = 1 rps - this is 1000 x rps (for int arithmetic) range 333 - 33333
			long windSpeedRps = 100000000 / windSpeedDur;
			//NOTE:converted multipliers to KNTS
			//need to avoid div/0 errors
			if (windSpeedRps < 323) {
				//need extra accuracy here, zero is very unlikely
				windSpeedRps = windSpeedRps * 10;
				model->setWindAverage((((((windSpeedRps * windSpeedRps) / -105) + ((25476 * windSpeedRps) / 100) - 12260)) / model->getWindFactor())/10);
			} else if (windSpeedRps < 5436) {
				//rps2 = min 10426441, max 30,864,197, cant get div/0 here?
				model->setWindAverage((((windSpeedRps * windSpeedRps) / 2222) + ((19099 * windSpeedRps) / 100) + 9638) / model->getWindFactor());
			} else {
				model->setWindAverage(((((windSpeedRps * windSpeedRps) / 1042) * 100) - (8314700 * windSpeedRps) + 2866500) / model->getWindFactor());
			}
		}
		//update gusts
		if (model->getWindAverage() > model->getWindMax()) model->setWindMax(model->getWindAverage());
		//Serial.print("getWindAverage,");
		//Serial.println(model->getWindAverage());
		// calc direction, in degrees clockwise
		//should round to int, min 1
		int dir = (int) getRotationalAverage();
		dir = dir-90; //radians start at 270deg
		//limit to +-360, after adjust zero
		//C = A – B * (A / B)
		dir = (dir + model->getWindZeroOffset()); // %360;
		//if (dir != 0) {
		//	dir = dir - 360 * (dir / 360);
		//}
		//if minus, then reverse to +ve
		if (dir < 0) {
			dir = 360 + dir;
		}
		model->setWindApparentDir(dir);
		//Serial.print("setWindApparentDir,");
		//Serial.println(dir);
	}
}

