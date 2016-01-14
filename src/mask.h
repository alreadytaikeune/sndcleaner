#define CROPPED   0x01
#define WRAPPED   0x02
#define EXTENDED  0x04

typedef struct Mask{
	int msk_w;
	int msk_h;
	double** msk;
	int pvt_x;
	int pvt_y;
	int msk_type;
} Mask;