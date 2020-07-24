/*
        BPM Polynomial Coefficients calculation
 *	10 Hz Slow x and y positions.
 
	** Guimei and Weixing
	For position correction, I use 1-D 5th order correction.

	example: horizontal  Kx: 11.08 (linear coeff)
	This is the correction coeff for 5th order              
	k1       k3       k5 

	11.11   1.76        24.25
	sum=x_raw/Kx = (a+c-b-d)/(a+b+c+d) or (a+b-c-d)/(a+b+c+d) 
	x_linear=Kx*(sum)+BBA_offset
	sum=(x_linear-BBA_offset)/Kx
	x_new=k1*sum+k3*sum^3+k5*sum^5+BBA_offset
	
	08/20/14 
 */
 

#include <stdio.h>

/* c includes */
#include <string.h>        
#include <math.h>          
#include <stdlib.h>       

#include <subRecord.h>       
#include <registryFunction.h> 
#include <epicsExport.h>      
#include <epicsTime.h>        
#include <dbAccess.h>         
#include <alarm.h>           


static int posPolyCoeffCalSupInit(subRecord *psub)
{
  return 0;
}

/*
 *
 *	a = pos X  mm
 *	b = pos B  mm
 *	c = K1   (K10
 *	d = K3   (K20
 *	e = K5   (K30
 *	
 *	f = SUM ?
 *	g = Kx
 *	h = Ky
 *  i = X, Y select
 *  J = BBA x offset
 *  K = BBA y offset
 *  L
 *	all mm unit
 
 
 record(sub, "$(P){BPM:$(NO)}Pos:PC-X-I") {
	field(INAM, "posPolyCoeffCalSupInit")
	field(SNAM, "posPolyCoeffCalSup")
	field(SCAN, "1 second")
	field(INPA, "$(P){BPM:$(NO)}Pos:X-I")
	field(INPB, "$(P){BPM:$(NO)}Pos:Y-I")
	field(INPC, "$(P){BPM:$(NO)}K:Kx10-SP")
	field(INPD, "$(P){BPM:$(NO)}K:Kx12-SP")
	field(INPE, "$(P){BPM:$(NO)}K:Kx30-SP")
	field(INPF, "$(P){BPM:$(NO)}Ampl:SSA-Calc")
	## mm input ?
	field(INPG, "$(P){BPM:$(NO)}Kx-SP")
	field(INPH, "$(P){BPM:$(NO)}Ky-SP")
	field(INPJ, "$(P){BPM:$(NO)}BbaXOff-SP")
	field(INPK, "$(P){BPM:$(NO)}BbaYOff-SP")
	field(INPI, "0")	## 0:X, 1:Y
	field(PREC, "6")
}

 */
static long posPolyCoeffCalSup(subRecord *psub)
{
double	Kx, Ky, bba_off_x, bba_off_y, x, y, K1, K3, K5, xp3, xp5, yp3, yp5, sum;
double	x_linear, sum_linear;
double	y_linear;

	sum = psub->f;	
	x = psub->a;	// mm x position with BBA offset 
	y = psub->b;	// mm y position 
	
	K1 =  psub->c;
	K3 =  psub->d;
	K5 =  psub->e;
	Kx = (psub->g);
	Ky = (psub->h);
	bba_off_x = (psub->j);
	bba_off_y = (psub->k);
	
	//printf("a=%f, b=%f, g=%f, h=%f\r\n", psub->a, psub->b, psub->g, psub->h);
	if(psub->i == 0) {
		x_linear = x;  // position x 
		sum_linear =(x_linear - bba_off_x)/ Kx;	
		xp3 = pow(sum_linear, 3);
		xp5 = pow(sum_linear, 5);
		psub->val = (K1*sum_linear + K3*xp3 + K5*xp5 ) + bba_off_x;
	//	printf("X=%f , x=%f, %f, %f, %f, %f , %f,  %f,  %f\n", psub->val, psub->a, psub->b, x, y, K5, sum_linear, xp3, xp5);
	}
  	else{			
		y_linear = y; 
		sum_linear = (y_linear-bba_off_y) / Ky;	
		yp3 = pow(sum_linear, 3);
		yp5 = pow(sum_linear, 5);
		psub->val = (K1*sum_linear + K3*yp3 + K5*yp5) + bba_off_y;
	//	printf("Y=%f , x=%f, %f, %f, %f, %f , %f,  %f\n", psub->val, psub->a, psub->b, x, y, K5, yp3, yp5);
	}
  	return 0;
}

epicsRegisterFunction(posPolyCoeffCalSupInit);
epicsRegisterFunction(posPolyCoeffCalSup);




