#include <new>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>

#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/glext.h>
#endif
#include <math.h>
#include "MButils.h"
#include <string.h>
#include "manuModel.h"

manuModel::manuModel()
{
	textureID = 3903;
	firstTexture = NULL;
	currentTexture = NULL;
	nVer = 0;
	nQuad = 0;
	nEdges = 0;
	nTrig = 0;
	verList = NULL;
	firstRun = true;
	TRIGTEXRES = 1;
	NUMTRIGPERROW = 1000;
	YL_UseTriangularTextureMap = false;
}

manuModel::~manuModel()
{
	if( verList != NULL ) delete verList;
	
	texture *next = firstTexture->nextTexture;
	delete firstTexture;
	while( next != NULL )
	{
		currentTexture = next;
		next = currentTexture->nextTexture;
		delete currentTexture;
	}
}

#ifndef max
float max( float first, float second )
{ // math.h does not having this function
	if( first > second ) return first;
	return second;
}
#endif

bool manuModel::readMesh(char *filename)
{	
	meshFile=filename; 
	FILE* fp = MBopenFile(filename, "r");
	if( YL_UseQuad )
	{
		//
		// Read in all the comments (lines that begin with a "#")
		//
		char buffer[256];  
		while(true)
		{
			fgets(buffer, 255, fp );
			if (buffer[0] != '#') break;
		}
		
		// Read in number of X and Y samples!
		sscanf(buffer, "%i %i \n", &xSamples, &ySamples );
		nVer = xSamples * ySamples;
		
		// Read in the points
		verList = new Point[ nVer ];
		originalList = new Point[ nVer ];
		for(int i=0; i < nVer; i++)
		{
			float x, y, z, u, v;
			int idx;
			
			fgets(buffer, 255, fp );
			sscanf(buffer, "%i %f %f %f %f %f \n", &idx, &x, &y, &z, &u, &v ); 
			verList[idx].x = x;
			verList[idx].y = y;
			verList[idx].z = z;
			verList[idx].u1 =  u;
			verList[idx].v1 =  v;
			
			//			verList[idx].v1 = imaH -  v;
			originalList[idx].u1 = (float) u;
			originalList[idx].v1 = (float) v;
		}
		
		// Adds quadList for self-collision
		fgets(buffer, 255, fp );
		if (buffer[0] == 'Q')
		{
			char token[10];
			sscanf(buffer, "%s %i \n", token, &nQuad);
			quadList = new Quad[nQuad];
			for (int i = 0; i< nQuad; i++)
			{
				int idx1, idx2, idx3, idx4;
				fgets(buffer, 255, fp );
				sscanf(buffer, "%i %i %i %i\n", &idx1, &idx2, &idx3, &idx4 ); 
				quadList[i].v1 = idx1-1;
				quadList[i].v2 = idx2-1;
				quadList[i].v3 = idx3-1;
				quadList[i].v4 = idx4-1;
			}
		
		}
		
		if( SMT_DEBUG ) printf("Read in %i Vertices \n", nVer );
		fclose(fp);
	}
	else
	{
		//
		// Read in all the comments (lines that begin with a "#")
		//
		char buffer[256], token[10]; 
		while(true)
		{
			fgets(buffer, 255, fp );
			if (buffer[0] != '#') break;
		}
		
		if (buffer[0] != 'V')
		{
			if( SMT_DEBUG ) fprintf(stderr, "The mesh file %s is not correct.\n", filename);
			return false;
		}
		else sscanf(buffer, "%s %i \n", token, &nVer);
		
		verList = new Point[nVer];
		originalList = new Point[nVer];
		for(int i=0; i < nVer; i++)
		{
			float x, y, z, u, v, w;
			
			fgets(buffer, 255, fp );
			if( YL_UseTriangularTextureMap ){
				sscanf(buffer, "%f %f %f\n", &x, &y, &z);
				verList[i].u1 = 0.0;
				verList[i].v1 = 0.0;
			}
			else{
				sscanf(buffer, "%f %f %f %f %f\n", &x, &y, &z, &u, &v);
				verList[i].u1 = u;
				verList[i].v1 = v;
			}
			
			verList[i].x = x;
			verList[i].y = y;
			verList[i].z = z;
			
		}
		if( SMT_DEBUG ) printf("Read in %i Vertices \n", nVer );
		
		fgets(buffer, 255, fp );
		if (buffer[0] != 'T')
		{
			if( SMT_DEBUG ) fprintf(stderr, "The mesh file %s is not correct.\n", filename);
			return false;
		}
		else sscanf(buffer, "%s %i \n", token, &nTrig);
		trigList = new Triangle[nTrig];
		for (int i = 0; i< nTrig; i++)
		{
			int idx1, idx2, idx3;
			fgets(buffer, 255, fp );
			sscanf(buffer, "%i %i %i\n", &idx1, &idx2, &idx3); 
			trigList[i].idx1 = idx1-1;
			trigList[i].idx2 = idx2-1;
			trigList[i].idx3 = idx3-1;
		}
		
		if( SMT_DEBUG ) printf("Read in %i Triangles \n", nTrig );
		fclose(fp);
	}
	//
	// !Translate points to the geometric center!
	// !Scale the points to fit in unit sphere!
	//
	float minx, miny, minz, maxx, maxy, maxz, scale = 1;
	cz = cx = cy = 0;
	for(int i=0; i < nVer; i++)
	{
		if (i==0)
		{
			minx = maxx = verList[i].x;
			miny = maxy = verList[i].y;
			minz = maxz = verList[i].z;
		}
		else
		{
			if (verList[i].x < minx) minx = verList[i].x;
			if (verList[i].y < miny) miny = verList[i].y;
			if (verList[i].z < minz) minz = verList[i].z;
			
			if (verList[i].x > maxx) maxx = verList[i].x;
			if (verList[i].y > maxy) maxy = verList[i].y;
			if (verList[i].z > maxz) maxz = verList[i].z;
		}
		
		cx += verList[i].x;
		cy += verList[i].y;
		cz += verList[i].z;
	}
	
	cx /= nVer; minx -= cx; maxx -= cx;
	cy /= nVer; miny -= cy; maxy -= cy;
	cz /= nVer; minz -= cz; maxz -= cz;
	
	float ww, hh, dd;
	/* calculate model width, height, and depth */
	ww = fabs(maxx) + fabs(minx);
	hh = fabs(maxy) + fabs(miny);
	dd = fabs(maxz) + fabs(minz);
	
	/* calculate center of the model */
	//cx = (maxx + minx) / 2.0;
	//cy = (maxy + miny) / 2.0;
	//cz = (maxz + minz) / 2.0;
	
	/* calculate unitizing scale factor */
	//scale = 12.0 / max(max(w, h), d);
	scale = 7.0 / max(max(ww, hh), dd);
	
	this->minx = 100000;
	this->miny = 100000;
	this->minz = 100000;
	this->maxx = -100000;
	this->maxy = -100000;
	this->maxz = -100000;
	for(int i=0; i < nVer; i++)
	{
		originalList[i].x = verList[i].x;
		originalList[i].y = verList[i].y;
		originalList[i].z = verList[i].z;
		originalList[i].u1 = verList[i].u1;
		originalList[i].v1 = verList[i].v1;
		
		verList[i].x -= cx;
		verList[i].y -= cy;
		verList[i].z -= cz;
		
		verList[i].x *= scale;
		verList[i].y *= scale;
		verList[i].z *= scale;
		
		if (verList[i].x > this->maxx)
			this->maxx = verList[i].x;
		if (verList[i].y > this->maxy)
			this->maxy = verList[i].y;
		if (verList[i].z > this->maxz)
			this->maxz = verList[i].z;


		if (verList[i].x < this->minx)
			this->minx = verList[i].x;
		if (verList[i].y < this->miny)
			this->miny = verList[i].y;
		if (verList[i].z < this->minz)
			this->minz = verList[i].z;
		
		// originalList[i].x = verList[i].x;
		// originalList[i].y = verList[i].y;
		// originalList[i].z = verList[i].z;
	}
	
	scaleFactor = 1.0 / scale;
	this->maxz += 0.3;
	edgeSum = edgeLengthSum();
	return true;
}

int manuModel::readPPM( char* filename, unsigned char* &image, int &width, int &height )
{
//  Open .ppm of given name from local directory
	FILE* fp = fopen(filename, "rb");
	if( fp == NULL ) return -1;
	char buffer[100], buffer2[100];
	char temp;
	int intTemp;
	int maxVal;
	int readIntIndex = 0;
	
	fscanf( fp, "%s", buffer );
	if( strncmp( buffer, "P6", 2 ) ) return -1;
	
	TRIGTEXRES = -1;
	long int offset = -1;
	while( readIntIndex < 3 )
	{
		//read in header information
		temp = fgetc( fp );
		
		if( !( temp == ' ' || temp == '\t' || temp == '\r' || temp == '\n' ) )
		{
			fseek( fp, offset, SEEK_CUR ); // Move back one
			
			if( temp == '#' )
			{
				intTemp = -1;
				fgets( buffer, sizeof(buffer)-1, fp );
				sscanf( buffer, "#%s %d\n", buffer2, intTemp );
				
				if( strcmp( buffer2, "TRIGTEXRES" ) == 0 ) TRIGTEXRES = intTemp;
			}
			else
			{
				fscanf( fp, "%d", &intTemp );
				if( readIntIndex == 0 ) width = intTemp;
				else if( readIntIndex == 1 ) height = intTemp;
				else maxVal = intTemp;
				readIntIndex++;
			}
		}
	}
	temp = fgetc( fp ); // get newline character
	
	image = new unsigned char[width*height*3];
	fread( image, width*height*3, 1, fp );
	fclose( fp );
	
	return 0;
} 

void manuModel::readTexture(char *filename)
{
	/**************************************************************/

 //	int imaW, imaH;
 pixel *colorIma;
	
 textureFile = filename;
 readPPM( filename, colorIma, imaW, imaH );

 if (YL_UseTriangularTextureMap){
	
	if( TRIGTEXRES < 0 || TRIGTEXRES > imaW )
	{
		TRIGTEXRES = 39;
		//TRIGTEXRES = 9;
		NUMTRIGPERROW = imaW / (TRIGTEXRES+1);
		printf( "Using TRIGTEXRES: %d, NUMTRIGPERROW: %d\n", TRIGTEXRES, NUMTRIGPERROW );
	}
	else
	{
		NUMTRIGPERROW = imaW / ( TRIGTEXRES + 1 );
	}
	numberOfTrianglesInATexture = NUMTRIGPERROW * NUMTRIGPERROW;
	int maxTextureHeight = NUMTRIGPERROW * ( TRIGTEXRES + 1 );
	
	int ppmHeightDone = 0, imaMinHeight = 0, imaMaxHeight = 0;
	
	if( colorIma != NULL )
	{
		if( SMT_DEBUG ) printf( "Copying surface into a texture structure. " );
		while( ppmHeightDone < imaH )
		{
			if( firstTexture == NULL )
			{
				firstTexture = new texture;
				currentTexture = firstTexture;
				currentTexture->nextTexture = NULL;
				currentTexture->id = textureID;
				textureID++;
			}
			else
			{
				texture *newTexture = new texture;
				currentTexture->nextTexture = newTexture;
				currentTexture = newTexture;
				currentTexture->nextTexture = NULL;
				currentTexture->id = textureID;
				textureID++;
			}
			currentTexture->ima = new pixel[ TEXW * 3 * TEXH ];
			
			imaMinHeight = imaMaxHeight;
			imaMaxHeight = imaH;
			
			if( imaMaxHeight - imaMinHeight > maxTextureHeight )
			{
				imaMaxHeight = imaMinHeight + maxTextureHeight;
			}
			
			if( SMT_DEBUG ) printf( "Image height %d to %d.\n", imaMinHeight, imaMaxHeight );
			for( int h = imaMinHeight; h < imaMaxHeight; h++ )
			{
				long offset1 = ( h - imaMinHeight ) * TEXW * 3;
				long offset2 = h * imaW * 3;
				for(int w=0; w < imaW * 3; w++)
				{
					currentTexture->ima[offset1 + w] = colorIma[offset2 + w];
				}
			}
			
			currentTexture->subIma = colorIma;
			currentTexture->w = TEXW;
			currentTexture->h = TEXH;
			currentTexture->ww = imaW;
			currentTexture->hh = imaMaxHeight - imaMinHeight;
			textureFormat = COLOR;
			initTexture( currentTexture );

			ppmHeightDone = imaMaxHeight;
		}
		if( SMT_DEBUG ) printf("Read texture file %s size %i %i \n", filename, imaW, imaH );
	}
	else if( SMT_DEBUG ) printf("Can't open file %s \n", filename );

	// Important
	currentTexture = firstTexture;
 }
 else{
  if( colorIma != NULL ){
	firstTexture = new texture;
	currentTexture = firstTexture;
	currentTexture->nextTexture = NULL;
	currentTexture->id = textureID;
    currentTexture->ima = new pixel[ TEXW * 3 * TEXH ];
	for( int h = 0; h < imaH; h++ ){
		for(int w=0; w < imaW; w++){
			long offset1 = (h * TEXW + w) * 3;
			long offset2 = (h * imaW + w) * 3;
			currentTexture->ima[offset1] = colorIma[offset2];
			currentTexture->ima[offset1+1] = colorIma[offset2+1];
			currentTexture->ima[offset1+2] = colorIma[offset2+2];
		}
	}
	currentTexture->subIma = colorIma;
	currentTexture->w = TEXW;
	currentTexture->h = TEXH;
	currentTexture->ww = imaW;
	currentTexture->hh = imaH;
	textureFormat = COLOR;
	initTexture( currentTexture );
	if( SMT_DEBUG ) printf("Read texture file %s size %i %i \n", filename, imaW, imaH );
  }
  else if( SMT_DEBUG ) printf("Can't open file %s \n", filename );
 } // end else

}


void
manuModel::replaceTexture(char *filename)
{
/*
	if( SMT_DEBUG )
		printf("Replacing texture with %s \n", filename );
	glBindTexture( GL_TEXTURE_2D, textureID );
	pixel *colorIma = (pixel *) MBLoadBitmap2(filename,  imaW, imaH);
	if( SMT_DEBUG )
		printf("Size %i %i \n", imaW, imaH );
	glTexSubImage2D(  GL_TEXTURE_2D, 0, 0, 0, imaW, imaH, GL_RGB, GL_UNSIGNED_BYTE, colorIma );
*/
}

void manuModel::initTexture( texture *inTexture )
{
	glBindTexture(GL_TEXTURE_2D, inTexture->id );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	if(textureFormat == COLOR)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (int)inTexture->w, (int)inTexture->h,
						0, GL_RGB, GL_UNSIGNED_BYTE, inTexture->ima );
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, (int)inTexture->w, (int)inTexture->h,
						0, GL_LUMINANCE, GL_UNSIGNED_BYTE, inTexture->ima );
}

void manuModel::BindNextTexture()
{
	texture *next = currentTexture->nextTexture;
	if( next == NULL ) next = firstTexture;
	
	glEnable(GL_TEXTURE_2D);
	glBindTexture( GL_TEXTURE_2D, currentTexture->id );
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

	currentTexture = next;
}

double manuModel::edgeLengthSum()
{
	double sum = 0;
	#define sq(x) ((x)*(x))
	if (YL_UseQuad){

		for(int j=0; j < ySamples-1; j++){
			for(int i=0; i < xSamples-1; i++){
			// v1 ---  v2     d1 = v1+v2
			//                d2 = v2+v3
			//  |      |      d3 = v3+v4
			//  |      |      d4 = v4+v1
			// 
			// v4 ---  v3

			int offset_v1, offset_v2, offset_v3, offset_v4;

			offset_v1 = j*(xSamples)+i;
			offset_v2 = j * (xSamples)+i+1;
			offset_v3 = (j+1) * (xSamples)+i+1;
			offset_v4 = (j+1) * (xSamples)+i;

			double d1=0, d2=0, d3=0, d4=0;

	

			// v1--v2  compute only if at the top
			if (j==0)
				d1 = sqrt( sq( verList[offset_v1].x - verList[ offset_v2 ].x ) +
						   sq( verList[offset_v1].y - verList[ offset_v2 ].y ) +
						   sq( verList[offset_v1].z - verList[ offset_v2 ].z )     );

			
			d2 = sqrt( sq( verList[offset_v3].x - verList[ offset_v2 ].x ) +
					   sq( verList[offset_v3].y - verList[ offset_v2 ].y ) +
					   sq( verList[offset_v3].z - verList[ offset_v2 ].z )     );

			d3 = sqrt( sq( verList[offset_v3].x - verList[ offset_v4 ].x ) +
					   sq( verList[offset_v3].y - verList[ offset_v4 ].y ) +
					   sq( verList[offset_v3].z - verList[ offset_v4 ].z )     );

			// v1--v3 compute only if at the begining
			if (i==0)
				d4 = sqrt( sq( verList[offset_v1].x - verList[ offset_v4 ].x ) +
						   sq( verList[offset_v1].y - verList[ offset_v4 ].y ) +
						   sq( verList[offset_v1].z - verList[ offset_v4 ].z )     );

  
			sum += (d1 + d2 + d3 + d4);
					
			}
		}
	}
	else{
		for(int i=0; i < nTrig; i++){
			Triangle t = trigList[i];
			sum += sqrt( sq(verList[t.idx1].x - verList[t.idx2].x)   + 
					 sq(verList[t.idx1].y - verList[t.idx2].y)   +
					 sq(verList[t.idx1].z - verList[t.idx2].z));
			sum += sqrt( sq(verList[t.idx2].x - verList[t.idx3].x)   + 
					 sq(verList[t.idx2].y - verList[t.idx3].y)   +
					 sq(verList[t.idx2].z - verList[t.idx3].z));
			sum += sqrt( sq(verList[t.idx1].x - verList[t.idx3].x)   + 
					 sq(verList[t.idx1].y - verList[t.idx3].y)   +
					 sq(verList[t.idx1].z - verList[t.idx3].z));
		}
	}

	return sum*scaleFactor;

}

bool IsVertexInEdge(void *vv, void *edge)
{
	long *v = (long *)vv;
	Edge *e = (Edge *)edge;
	
	if ( (e->v1 == *v) || (e->v2 == *v) ) return true;
	else return false;
};