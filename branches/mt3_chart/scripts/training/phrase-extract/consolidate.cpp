// $Id: consolidate.cpp 1953 2008-12-05 10:01:05Z bojar $
// vim:tabstop=2

/***********************************************************************
  Moses - factored phrase-based language decoder
  Copyright (C) 2009 University of Edinburgh

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/

#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
using namespace std;

#define SAFE_GETLINE(_IS, _LINE, _SIZE, _DELIM) { \
		_IS.getline(_LINE, _SIZE, _DELIM);						\
		if(_IS.fail() && !_IS.bad() && !_IS.eof()) _IS.clear();	\
		if (_IS.gcount() == _SIZE-1) {								\
			cerr << "Line too long! Buffer overflow. Delete lines >="	\
					 << _SIZE << " chars or raise LINE_MAX_LENGTH in phrase-extract/scrore.cpp" \
					 << endl;																\
			exit(1);																		\
		}																							\
	}
#define LINE_MAX_LENGTH 10000

bool hierarchicalFlag = false;
bool logProbFlag = false;
char line[LINE_MAX_LENGTH];

void processFiles( char*, char*, char* );
bool getLine( istream &fileP, vector< string > &item );
vector< string > splitLine();

int main(int argc, char* argv[]) 
{
  cerr << "Consolidate v2.0 written by Philipp Koehn\n"
       << "consolidating direct and indirect rule tables\n";
	
  if (argc < 4) {
    cerr << "syntax: consolidate phrase-table.direct phrase-table.indirect phrase-table.consolidated [--Hierarchical]\n";
    exit(1);
  }
  char* &fileNameDirect = argv[1];
  char* &fileNameIndirect = argv[2];
  char* &fileNameConsolidated = argv[3];
	
	for(int i=4;i<argc;i++) {
		if (strcmp(argv[i],"--Hierarchical") == 0) {
			hierarchicalFlag = true;
			cerr << "processing hierarchical rules\n";
		}
		else if (strcmp(argv[i],"--LogProb") == 0) {
			logProbFlag = true;
			cerr << "using log-probabilities\n";
		}
		else {
			cerr << "ERROR: unknown option " << argv[i] << endl;
			exit(1);
		}
	}

	processFiles( fileNameDirect, fileNameIndirect, fileNameConsolidated );
}

void processFiles( char* fileNameDirect, char* fileNameIndirect, char* fileNameConsolidated ) {
	// open input files
	ifstream fileDirect,fileIndirect;

	fileDirect.open(fileNameDirect);
	if (fileDirect.fail()) {
    cerr << "ERROR: could not open extract file " << fileNameDirect << endl;
    exit(1);
  }
  istream &fileDirectP = fileDirect;

	fileIndirect.open(fileNameIndirect);
	if (fileIndirect.fail()) {
    cerr << "ERROR: could not open extract file " << fileNameIndirect << endl;
    exit(1);
  }
  istream &fileIndirectP = fileIndirect;

	// open output file: consolidated phrase table
	ofstream fileConsolidated;
	fileConsolidated.open(fileNameConsolidated);
  if (fileConsolidated.fail()) 
	{
    cerr << "ERROR: could not open output file " << fileNameConsolidated << endl;
    exit(1);
  }

  // loop through all extracted phrase translations
  int i=0;
  while(true) {
		if (++i % 100000 == 0) cerr << "." << flush;

		vector< string > itemDirect, itemIndirect;
		if (! getLine(fileIndirectP,itemIndirect) ||
				! getLine(fileDirectP,  itemDirect  ))
			break;

		// direct: target source alignment probabilities
    // indirect: source target probabilities

		// consistency checks
		if ((itemDirect.size() != 4 && hierarchicalFlag) ||
				(itemDirect.size() != 3 && !hierarchicalFlag))
		{
			cerr << "ERROR: expected " << (hierarchicalFlag ? "4" : "3") << " items in file " 
					 << fileNameDirect << ", line " << i << endl;
			exit(1);
		}

		if (itemIndirect.size() != 3)
		{
			cerr << "ERROR: expected 3 items in file " 
					 << fileNameIndirect << ", line " << i << endl;
			exit(1);
		}

		if (itemDirect[0].compare( itemIndirect[0] ) != 0)
		{
			cerr << "ERROR: target phrase does not match in line " << i << ": '" 
					 << itemDirect[0] << "' != '" << itemIndirect[0] << "'" << endl;
			exit(1);
		}

		if (itemDirect[1].compare( itemIndirect[1] ) != 0)
		{
			cerr << "ERROR: source phrase does not match in line " << i << ": '" 
					 << itemDirect[1] << "' != '" << itemIndirect[1] << "'" << endl;
			exit(1);
		}

		// output hierarchical phrase pair (with separated labels)
		if (hierarchicalFlag) {
			size_t spaceSource = itemDirect[0].find(" ");
			size_t spaceTarget = itemDirect[1].find(" ");
			if (spaceSource == string::npos) {
				cerr << "ERROR: expected source as 'label words' in line " 
						 << i << ": '" << itemDirect[1] << "'" << endl;
				exit(1);
			}
			if (spaceTarget == string::npos) {
				cerr << "ERROR: expected target as 'label words' in line " 
						 << i << ": '" << itemDirect[0] << "'" << endl;
				exit(1);
			}
			fileConsolidated 
				<< itemDirect[0].substr(0,spaceSource)            // label source
				<< " " << itemDirect[1].substr(0,spaceTarget)     // label target
				<< " ||| " << itemDirect[0].substr(spaceSource+1) // source
				<< " ||| " << itemDirect[1].substr(spaceTarget+1) // target
				<< " ||| ";
		}
		// output regular phrase pair
		else {
			fileConsolidated << itemDirect[0] << " ||| " << itemDirect[1] << " ||| ";
		}

		// output alignment and probabilities
		if (hierarchicalFlag) 
			fileConsolidated << itemDirect[2]   << " ||| " // alignment
											 << itemIndirect[2]      // prob indirect
											 << " " << itemDirect[3]; // prob direct
		else
			fileConsolidated << itemIndirect[2]      // prob indirect
											 << " " << itemDirect[2]; // prob direct
		fileConsolidated << " " << (logProbFlag ? 1 : 2.718) << endl; // phrase count feature
	}
	fileDirect.close();
	fileIndirect.close();
	fileConsolidated.close();
}

bool getLine( istream &fileP, vector< string > &item ) 
{
	if (fileP.eof()) 
		return false;

	SAFE_GETLINE((fileP), line, LINE_MAX_LENGTH, '\n');
	if (fileP.eof()) 
		return false;

	//cerr << line << endl;
	
	item = splitLine();
	return true;
} 

vector< string > splitLine() 
{
  vector< string > item;
  bool betweenWords = true;
  int start=0;
	int i=0;
  for(; line[i] != '\0'; i++) {
		if (line[i] == ' ' &&
				line[i+1] == '|' &&
				line[i+2] == '|' &&
				line[i+3] == '|' &&
				line[i+4] == ' ')
		{
			if (start > i) start = i; // empty item
			item.push_back( string( line+start, i-start ) );
			start = i+5;
			i += 3;
		}
	}
	item.push_back( string( line+start, i-start ) );

  return item;
}
