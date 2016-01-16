#ifndef _MASK_H_
#define _MASK_H_

#define CROPPED   0x01
#define WRAPPED   0x02
#define EXTENDED  0x04
#define M1		  0x10
#define M2		  0x20
#define M3        0x40
#define M4        0x80

#include <stdint.h>
#include <stdlib.h> 

typedef struct Mask{
	int msk_w;
	int msk_h;
	double** msk;
	int pvt_x;
	int pvt_y;
	int msk_type;
} Mask;


inline Mask* alloc_mask(int mask_type){
	Mask* m = (Mask*) malloc(sizeof(Mask));
	if(mask_type==M1 || mask_type==M2){
		m->msk = (double**) malloc(sizeof(double*)*2);
		m->msk[0] = (double*) malloc(sizeof(double)*2);
		m->msk[1] = (double*) malloc(sizeof(double)*2);

		if(mask_type==M1){
			m->msk[0][0]=-1;
			m->msk[1][0]=-1;
			m->msk[0][1]=1;
			m->msk[1][1]=1;
		}
		else if(mask_type==M2){
			m->msk[0][0]=1;
			m->msk[1][0]=-1;
			m->msk[0][1]=-1;
			m->msk[1][1]=1;
		}
		m->msk_w = 2;
		m->msk_h = 2;
		m->msk_type = mask_type;
		m->pvt_x=1;
		m->pvt_y=1;
	}
	else if(mask_type==M3){
		m->msk = (double**) malloc(sizeof(double*)*2);
		m->msk[0] = (double*) malloc(sizeof(double)*3);
		m->msk[1] = (double*) malloc(sizeof(double)*3);
		
		m->msk[0][0]=-1;
		m->msk[1][0]=-1;
		m->msk[0][1]=2;
		m->msk[1][1]=2;
		m->msk[0][2]=-1;
		m->msk[1][2]=-1;
		
		m->msk_w = 2;
		m->msk_h = 3;
		m->msk_type = mask_type;
		m->pvt_x=1;
		m->pvt_y=1;
	}

	else if(mask_type==M4){
		m->msk = (double**) malloc(sizeof(double*)*3);
		m->msk[0] = (double*) malloc(sizeof(double)*3);
		m->msk[1] = (double*) malloc(sizeof(double)*3);
		
		m->msk[0][0]=-1;
		m->msk[1][0]=-1;
		m->msk[2][0]=-1;
		m->msk[0][1]=2;
		m->msk[1][1]=2;
		m->msk[2][1]=2;
		m->msk[0][2]=-1;
		m->msk[1][2]=-1;
		m->msk[2][2]=-1;
		
		m->msk_w = 3;
		m->msk_h = 3;
		m->msk_type = mask_type;
		m->pvt_x=1;
		m->pvt_y=1;
	}
	else{
		free(m);
	}

	return m;
}


inline void free_msk(Mask* m){
	for(int i =0; i < m->msk_w; i++){
		free(m->msk[i]);
	}
	free(m->msk);
	free(m);
}

#endif