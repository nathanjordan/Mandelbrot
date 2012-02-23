/*
 * main.cpp
 *
 *  Created on: Feb 15, 2012
 *      Author: njordan
 */

#include <iostream>
#include <mpi/mpi.h>
#include "pngwriter.h"
#include <vector>
#include <sys/time.h>

using namespace std;

const int IMAGE_WIDTH = 3000;
const int IMAGE_HEIGHT = 3000;
const int REAL_MIN = -2;
const int REAL_MAX = 2;
const int IMAG_MIN = -2;
const int IMAG_MAX = 2;

const int MASTER_NODE = 0;
const int ROWS_PER_JOB = 100;

const int TAG_NEW_JOB = 1;
const int TAG_FINISHED_JOB = 2;
const int TAG_KILL = 3;
const int BUFFER_INFO_OFFSET = 2;

const float REAL_SCALE = (REAL_MAX - REAL_MIN) / (float) IMAGE_WIDTH;
const float IMAG_SCALE = (IMAG_MAX - IMAG_MIN) / (float) IMAGE_HEIGHT;

const int BUFFER_SIZE = ROWS_PER_JOB * IMAGE_WIDTH + BUFFER_INFO_OFFSET;

struct complex {

	float real;

	float imag;

	};

int calcPixel(complex c);

void writeRows( int* values , int startRow, int numRows , pngwriter* p );

long long int getTimeInMicroseconds() ;

int main( int argc, char *argv[] ) {

	int id;

	int cores;

	MPI_Status stat;

	MPI_Init( &argc , &argv );

	MPI_Comm_size(MPI_COMM_WORLD,&cores);

	MPI_Comm_rank(MPI_COMM_WORLD,&id);

	if( id == MASTER_NODE ) {

		int j = 0;

		int activeJobs = 0;

		pngwriter* p = new pngwriter( IMAGE_WIDTH , IMAGE_HEIGHT , 0 , "output.png" );

		int t1 = getTimeInMicroseconds();

		for( int i = 0 ; i < cores - 1 && j < IMAGE_HEIGHT ; j += ROWS_PER_JOB, i++ ) {

			int* job = new int[2];

			job[0] = j;

			if( j + ROWS_PER_JOB > IMAGE_HEIGHT )

				job[1] = IMAGE_HEIGHT;

			else

				job[1] = j + ROWS_PER_JOB;

			MPI_Send( job , 2 , MPI_INT , i + 1 , TAG_NEW_JOB , MPI_COMM_WORLD );

			activeJobs++;

			delete [] job;

			}

		while( activeJobs > 0 ) {

			int* buf = new int[ BUFFER_SIZE ];

			MPI_Recv( buf , BUFFER_SIZE , MPI_INT , MPI_ANY_SOURCE , TAG_FINISHED_JOB , MPI_COMM_WORLD , &stat );

			writeRows( buf , buf[0] , buf[1] , p );

			activeJobs--;

			if( j < IMAGE_HEIGHT ) {

				int* job = new int[2];

				job[0] = j;

				if( j + ROWS_PER_JOB > IMAGE_HEIGHT ) {

					job[1] = IMAGE_HEIGHT;

					j += IMAGE_HEIGHT - j;

					}
				else {

					job[1] = j + ROWS_PER_JOB;

					j += ROWS_PER_JOB;

					}

				MPI_Send( job , 2 , MPI_INT , stat.MPI_SOURCE , TAG_NEW_JOB , MPI_COMM_WORLD );

				activeJobs++;

				delete [] job;

				}

			}

		for( int i = 0 ; i < cores - 1 ; i++ ) {

			int* job = new int[2];

			MPI_Send( job , 2 , MPI_INT , i + 1 , TAG_KILL , MPI_COMM_WORLD );

			}

		int t2 = getTimeInMicroseconds();

		p->close();

		cout << endl << "Process took " << (t2 - t1) / 1000.0 << " miliseconds." << endl;

		}

	else {

		while( true ) {

			int* jobInfo = new int[2];

			MPI_Recv( jobInfo , 2 , MPI_INT , MASTER_NODE , MPI_ANY_TAG , MPI_COMM_WORLD , &stat );

			if( stat.MPI_TAG == TAG_KILL )

				break;

			int* buf = new int[ BUFFER_SIZE ];

			for( int i = 0 ; i < jobInfo[1] - jobInfo[0] ; i++ )

				for( int j = 0 ; j < IMAGE_WIDTH ; j++ ) {

					int offset = i * IMAGE_WIDTH + j + BUFFER_INFO_OFFSET;

					complex c;

					c.real = REAL_MIN + (i + jobInfo[0]) * REAL_SCALE;

					c.imag = IMAG_MIN + j * IMAG_SCALE;

					buf[ offset ] = calcPixel( c );
				}

			buf[0] = jobInfo[0];

			buf[1] = jobInfo[1];

			MPI_Send( buf , BUFFER_SIZE , MPI_INT , MASTER_NODE , TAG_FINISHED_JOB , MPI_COMM_WORLD );

			}

		}

	MPI_Finalize();

	return 0;

	}

int calcPixel(complex c) {

	int count = 0, max = 256;

	complex z;

	float temp, lengthsq;

	z.real = 0; z.imag = 0;

	do {
		temp = z.real * z.real - z.imag * z.imag + c.real;

		z.imag = 2 * z.real * z.imag + c.imag;

		z.real = temp;

		lengthsq = z.real * z.real + z.imag * z.imag;

		count++;

		}

	while ( ( lengthsq < 4.0 ) && ( count < max ) );

	return count;
	}

void writeRows( int* values , int startRow, int endRow , pngwriter* p ) {

	for( int i = 0 ; i < endRow - startRow ; i++ )

		for( int j = 0 ; j < IMAGE_WIDTH ; j++ ) {

			int offset = i * IMAGE_WIDTH + j + BUFFER_INFO_OFFSET;

			int value = 65535 - values[ offset ] * 256;

			p->plot( i + startRow , j , value , value , value );

			}

	}

long long int getTimeInMicroseconds() {

	struct timeval tv;
	struct timezone tz;
	struct tm *tm;

	gettimeofday(&tv, &tz);
	tm=localtime(&tv.tv_sec);

	return tm->tm_hour * 60 * 60 * 1000000 + tm->tm_min * 60 * 1000000 + tm->tm_sec * 1000000 + tv.tv_usec;

	}
