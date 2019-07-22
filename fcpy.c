#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/wait.h>
#include <fcntl.h>

struct fcpy {
	struct source {
		int fd;
		char filename[PATH_MAX+1];
		struct stat stat;
	} source;
	struct dest {
		char filename[PATH_MAX+1];
		int fd;
	} dest;
} data;

int MAX_SPLIT = 5;

struct splits {
	off_t offset;
	size_t length;
};

void init() {
	if( ( data.source.fd = open( data.source.filename, O_RDONLY ) ) < 0 ) {
		perror( data.source.filename );
		exit(EXIT_FAILURE);
	}

	if( ( data.dest.fd = open( data.dest.filename, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0664 ) ) < 0 ) {
		perror( data.dest.filename );
		exit( EXIT_FAILURE );
	}

	if( stat( data.source.filename, &data.source.stat ) < 0 ) {
		perror( "stat" );
		exit(EXIT_FAILURE);
	}

	if( ftruncate( data.dest.fd, data.source.stat.st_size ) < 0 ) {
		perror( "truncate" );
		exit(EXIT_FAILURE);
	}
}

void split_( struct splits buf[] ) {
	unsigned int split = data.source.stat.st_size / MAX_SPLIT ;
	unsigned int mod = data.source.stat.st_size % MAX_SPLIT;

	for( int i = 0; i < MAX_SPLIT; i++ ) {
		if( i == 0 ) {
			buf[i].offset = lseek( data.source.fd, 0, SEEK_CUR );
			buf[i].length = split;
			continue;
		}

		if( lseek( data.source.fd, split , SEEK_CUR ) < 0 ) {
			perror( "offset" );
			exit( EXIT_FAILURE );
		}

		buf[i].offset = lseek( data.source.fd, 0, SEEK_CUR );
		buf[i].length = ( i == ( MAX_SPLIT - 1 ) ? split+mod : split );
	}

	lseek( data.source.fd, 0, SEEK_SET );
}

void copy_chunk( off_t offset, size_t length ) {
	char *buf = (char*) malloc( length );
	size_t virtual_len = length;

	size_t r = 0;
	size_t read_bytes = 0;
	while( ( r = pread( data.source.fd, buf, virtual_len, offset ) ) != 0 ) {
		pwrite( data.dest.fd, buf, r, offset );
		if( r < length ) {
			offset = r+offset;
			virtual_len = virtual_len-r;
		}
		read_bytes += r;
		printf( "Written: %u\n", read_bytes );
		if( read_bytes >= length )
			break;
	}
	free( buf );
}

void copy_large() {
	struct splits splits[MAX_SPLIT];
	split_( splits );
	size_t total = 0;
	
	for( int i = 0; i < MAX_SPLIT; i++ ) {
		printf( "Offset %u till %u\n", splits[i].offset, splits[i].offset+splits[i].length );
		total += splits[i].length;
		copy_chunk(splits[i].offset, splits[i].length);
	}
	printf( "Total Bytes written: %lu\n", total );	
}

int main( int argc, char *argv[] ) {
	if( argc < 3 ) {
		fprintf(stderr, "Incomplete arguments\n" );
		exit(EXIT_FAILURE);
	}
	else {
		strcpy( data.dest.filename, argv[2] );
		strcpy( data.source.filename, argv[1]);
	}

	init();
	copy_large();
	// if( data.source.stat.st_size >= 209715200 ) {
	// 	printf( "This file is over 200MB\n" );
	// 	copy_large();
	// }
	// else {
	// 	printf("This is a small file\n");
	// 	copy_large();
	// }

	close( data.source.fd );
	close( data.dest.fd );

	return 0;
}