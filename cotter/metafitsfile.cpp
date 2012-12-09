#include "metafitsfile.h"
#include "mwaconfig.h"
#include "geometry.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cmath>

MetaFitsFile::MetaFitsFile(const char* filename)
{
	int status = 0;
	if(fits_open_file(&_fptr, filename, READONLY, &status))
		throwError(status, std::string("Cannot open file ") + filename);
	
	int hduCount;
	fits_get_num_hdus(_fptr, &hduCount, &status);
	checkStatus(status);
	
	std::cout << "There are " << hduCount << " HDUs in file " << filename << '\n';
	
	if(hduCount < 2)
		throw std::runtime_error("At least two HDUs are expected to be in the meta fitsfile");
}

void MetaFitsFile::ReadHeader(MWAHeader& header, MWAHeaderExt &headerExt)
{
	int status = 0, hduType;
	fits_movabs_hdu(_fptr, 1, &hduType, &status);
	checkStatus(status);
	
	int keywordCount;
	fits_get_hdrspace(_fptr, &keywordCount, 0, &status);
	checkStatus(status);
	
	for(int i=0; i!=keywordCount; ++i)
	{
		char keyName[80], keyValue[80];
		fits_read_keyn(_fptr, i+1, keyName, keyValue, 0, &status);
		parseKeyword(header, headerExt, keyName, keyValue);
	}
	header.correlationType = MWAHeader::BothCorrelations;
}

void MetaFitsFile::ReadTiles(std::vector<MWAInput> &inputs, std::vector<MWAAntenna> &antennae)
{
	int status = 0;
	
	int hduType;
	fits_movabs_hdu(_fptr, 2, &hduType, &status);
	checkStatus(status);
	
	char
		inputColName[] = "Input",
		antennaColName[] = "Antenna",
		tileColName[] = "Tile",
		polColName[] = "Pol",
		rxColName[] = "Rx",
		slotColName[] = "Slot",
		flagColName[] = "Flag",
		lengthColName[] = "Length",
		eastColName[] = "East",
		northColName[] = "North",
		heightColName[] = "Height";
	int inputCol, antennaCol, tileCol, polCol, rxCol, slotCol, flagCol, lengthCol, northCol, eastCol, heightCol;
		
	fits_get_colnum(_fptr, CASESEN, inputColName, &inputCol, &status);
	fits_get_colnum(_fptr, CASESEN, antennaColName, &antennaCol, &status);
	fits_get_colnum(_fptr, CASESEN, tileColName, &tileCol, &status);
	fits_get_colnum(_fptr, CASESEN, polColName, &polCol, &status);
	fits_get_colnum(_fptr, CASESEN, rxColName, &rxCol, &status);
	fits_get_colnum(_fptr, CASESEN, slotColName, &slotCol, &status);
	fits_get_colnum(_fptr, CASESEN, flagColName, &flagCol, &status);
	fits_get_colnum(_fptr, CASESEN, lengthColName, &lengthCol, &status);
	fits_get_colnum(_fptr, CASESEN, eastColName, &eastCol, &status);
	fits_get_colnum(_fptr, CASESEN, northColName, &northCol, &status);
	fits_get_colnum(_fptr, CASESEN, heightColName, &heightCol, &status);
	checkStatus(status);
	
	long int nrow;
	fits_get_num_rows(_fptr, &nrow, &status);
	checkStatus(status);
	antennae.resize(nrow/2);
	inputs.resize(nrow);
	
	for(long int i=0; i!=nrow; ++i)
	{
		int input, antenna, tile, rx, slot, flag;
		double north, east, height;
		char pol;
		char length[81] = "";
		char *lengthPtr[1] = { length };
		std::string lengthStr;
		fits_read_col(_fptr, TINT, inputCol, i+1, 1, 1, 0, &input, 0, &status);
		fits_read_col(_fptr, TINT, antennaCol, i+1, 1, 1, 0, &antenna, 0, &status);
		fits_read_col(_fptr, TINT, tileCol, i+1, 1, 1, 0, &tile, 0, &status);
		fits_read_col(_fptr, TBYTE, polCol, i+1, 1, 1, 0, &pol, 0, &status);
		fits_read_col(_fptr, TINT, rxCol, i+1, 1, 1, 0, &rx, 0, &status);
		fits_read_col(_fptr, TINT, slotCol, i+1, 1, 1, 0, &slot, 0, &status);
		fits_read_col(_fptr, TINT, flagCol, i+1, 1, 1, 0, &flag, 0, &status);
		fits_read_col(_fptr, TSTRING, lengthCol, i+1, 1, 1, 0, lengthPtr, 0, &status);
		fits_read_col(_fptr, TDOUBLE, eastCol, i+1, 1, 1, 0, &east, 0, &status);
		fits_read_col(_fptr, TDOUBLE, northCol, i+1, 1, 1, 0, &north, 0, &status);
		fits_read_col(_fptr, TDOUBLE, heightCol, i+1, 1, 1, 0, &height, 0, &status);
		checkStatus(status);
		
		if(pol == 'X')
		{
			std::stringstream nameStr;
			nameStr << "Tile";
			if(tile<100) nameStr << '0';
			if(tile<10) nameStr << '0';
			nameStr << tile;
			
			MWAAntenna &ant = antennae[antenna];
			ant.name = nameStr.str();
			Geometry::ENH2XYZ_local(east, north, height, MWAConfig::ArrayLattitudeRad(), ant.position[0], ant.position[1], ant.position[2]);
			ant.stationIndex = antenna;
			//std::cout << ant.name << ' ' << input << ' ' << antenna << ' ' << east << ' ' << north << ' ' << height << '\n';
		} else if(pol != 'Y')
			throw std::runtime_error("Error parsing polarization");
		
		length[80] = 0;
		lengthStr = length;
		inputs[input].inputIndex = input;
		inputs[input].antennaIndex = antenna;
		if(lengthStr.substr(0, 3) == "EL_")
			inputs[input].cableLenDelta = atof(&(lengthStr.c_str()[3]));
		else
			inputs[input].cableLenDelta = atof(lengthStr.c_str()) * MWAConfig::VelocityFactor();
		inputs[input].polarizationIndex = (pol=='X') ? 0 : 1;
		inputs[input].isFlagged = (flag!=0);
	}
}

void MetaFitsFile::parseKeyword(MWAHeader &header, MWAHeaderExt &headerExt, const char *keyName, const char *keyValue)
{
	std::string name(keyName);
	
	if(name == "GPSTIME")
		headerExt.gpsTime = atoi(keyValue);
	else if(name == "FILENAME")
		header.fieldName = parseFitsString(keyValue);
	else if(name == "DATE-OBS")
		parseFitsDate(keyValue, header.year, header.month, header.day, header.refHour, header.refMinute, header.refSecond);
	else if(name == "RA")
		header.raHrs = atof(keyValue);
	else if(name == "DEC")
		header.decDegs = atof(keyValue);
	else if(name == "GRIDNAME")
		headerExt.gridName = parseFitsString(keyValue);
	else if(name == "CREATOR")
		headerExt.observerName = parseFitsString(keyValue);
	else if(name == "PROJECT")
		headerExt.projectName = parseFitsString(keyValue);
	else if(name == "MODE")
		headerExt.mode = parseFitsString(keyValue);
	else if(name == "DELAYS")
		parseIntArray(keyValue, headerExt.delays, 16);
	else if(name == "CALIBRAT")
		headerExt.hasCalibrator = parseBool(keyValue);
	else if(name == "CENTCHAN")
		headerExt.centreSBNumber = atoi(keyValue);
	else if(name == "CHANGAIN")
		parseIntArray(keyValue, headerExt.subbandGains, 24);
	else if(name == "FIBRFACT")
		headerExt.fibreFactor = atof(keyValue);
	else if(name == "INTTIME")
		header.integrationTime = atof(keyValue);
	else if(name == "NSCANS")
		header.nScans = atoi(keyValue);
	else if(name == "NINPUTS")
		header.nInputs = atoi(keyValue);
	else if(name == "NCHANS")
		header.nChannels = atoi(keyValue);
	else if(name == "BANDWDTH")
		header.bandwidthMHz = atof(keyValue);
	else if(name == "FREQCENT")
		header.centralFrequencyMHz = atof(keyValue);
	//else
	//	std::cout << "Ignored keyword: " << name << '\n';
	std::cout << "RA=" << header.raHrs << ", DEC=" << header.decDegs << '\n';
}

std::string MetaFitsFile::parseFitsString(const char* valueStr)
{
	if(valueStr[0] != '\'')
		throw std::runtime_error("Error parsing fits string");
	std::string value(valueStr+1);
	if((*value.rbegin())!='\'')
		throw std::runtime_error("Error parsing fits string");
	int s = value.size() - 1;
	while(s > 0 && value[s-1]==' ') --s;
	return value.substr(0, s);
}

void MetaFitsFile::parseFitsDate(const char* valueStr, int& year, int& month, int& day, int& hour, int& min, double& sec)
{
	std::string dateStr = parseFitsString(valueStr);
	if(dateStr.size() != 19)
		throw std::runtime_error("Error parsing fits date");
	year = (dateStr[0]-'0')*1000 + (dateStr[1]-'0')*100 +
		(dateStr[2]-'0')*10 + (dateStr[3]-'0');
	month = (dateStr[5]-'0')*10 + (dateStr[6]-'0');
	day = (dateStr[8]-'0')*10 + (dateStr[9]-'0');
	
	hour = (dateStr[11]-'0')*10 + (dateStr[12]-'0');
	min = (dateStr[14]-'0')*10 + (dateStr[15]-'0');
	sec = (dateStr[17]-'0')*10 + (dateStr[18]-'0');
	std::cout << "Date=" << year << '-' << month << '-' << day << ' ' << hour << ':' << min << ':' << sec <<'\n';
}

void MetaFitsFile::parseIntArray(const char* valueStr, int *delays, size_t count)
{
	std::string str = parseFitsString(valueStr);
	size_t pos = 0;
	for(size_t i=0; i!=count-1; ++i)
	{
		size_t next = str.find(',', pos);
		if(next == str.npos)
			throw std::runtime_error("Error parsing delays in fits file");
		*delays = atoi(str.substr(pos, next-1).c_str());
		++delays;
		pos = next;
	}
	*delays = atoi(str.substr(pos).c_str());
}

bool MetaFitsFile::parseBool(const char* valueStr)
{
	if(valueStr[0] != 'T' && valueStr[0] != 'F')
		throw std::runtime_error("Error parsing boolean in fits file");
	return valueStr[0]=='T';
}
