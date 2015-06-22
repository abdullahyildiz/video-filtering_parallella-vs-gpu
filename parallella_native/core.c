
/*
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include "structures.h"
#include "e-lib.h"

volatile uint8_t tmp[WIDTH] ALIGN(8) SECTION(".data_bank3");
volatile message MSG ALIGN(8) SECTION(".data_bank2");
volatile uint8_t  frame[WIDTH*(HEIGHT/16+2)] ALIGN(8) SECTION(".data_bank1");

int received_row;
int rowcount=(HEIGHT/16-1);

void sort(uint8_t* window){ //Sort Function
	uint8_t temp;
	int j,i;
	for(i=1;i<9;i++){
		for(j=0;j<9-i;j++){
			if(window[j] > window[j+1]){
				temp=window[j];
				window[j]=window[j+1];
				window[j+1]=temp;
			}
		}
	}
}

_Bool medianFilter(unsigned int cnum){ // Median Filter
	int row,col,layer,wrow,wcol,s,flag=0,count;
	uint8_t temp;
	uint8_t window[9];
	if(cnum==0){
		for(row=0;row<received_row-1;row++){
			for(col=0;col<WIDTH;col++){
				//Fill Window
				for(wrow=-1;wrow<2;wrow++){
					if(wrow==-1 && row==0){
						wrow=0;
						flag=1;
					}
					for(wcol=-1;wcol<2;wcol++){
						if(col==0){
							if(wcol==-1){
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+(wcol+1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else if(col==WIDTH-1){
							if(wcol==2){
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+(wcol-1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else{
							window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
						}
					}
					if(flag){
						flag=0;
						wrow=-1;
					}
				}
				//Sort Window
				sort(window);
				s=1;
				if (window[4]==(uint8_t) 0){
					while(window[4+s]==(uint8_t) 0 && s!=5){
						s=s+1;
					}
					window[4]=window[4+s+(10-(5+s))/2];
				}
				if (window[4]==(uint8_t) 255){
					while(window[4-s]==(uint8_t) 255 && s!=5){
						s=s+1;
					}
					window[4]=window[(4-s)/2];
				}
				if(row==0){
					tmp[col]=window[4];
				}
				else{// Fix to not use changed bit in next iteration
					if(col==0){
						temp=tmp[col];
						tmp[col]=window[4];
					}
					else if(col==WIDTH-1){
						frame[(row-1)*WIDTH+col-1]=temp;
						frame[(row-1)*WIDTH+col]=tmp[col];
						tmp[col]=window[4];
					}
					else{
						frame[(row-1)*WIDTH+col-1]=temp;
						temp=tmp[col];
						tmp[col]=window[4];
					}
				}
			}
		}
	}
	else if(cnum==15){
		for(row=1;row<received_row;row++){
			for(col=0;col<WIDTH;col++){
				//Fill Window
				for(wrow=-1;wrow<2;wrow++){
					if(wrow==1 && row==received_row-1){
						wrow=0;
						flag=1;
					}
					for(wcol=-1;wcol<2;wcol++){
						if(col==0){
							if(wcol==-1){
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+(wcol+1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else if(col==WIDTH-1){
							if(wcol==2){
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+(wcol-1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else{
							window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
						}
					}
					if(flag){
						flag=0;
						break;
					}
				}
				//Sort Window
				sort(window);
				s=1;
				if (window[4]==(uint8_t) 0){
					while(window[4+s]==(uint8_t) 0 && s!=5){
						s=s+1;
					}
					window[4]=window[4+s+(10-(5+s))/2];
				}
				if (window[4]==(uint8_t) 255){
					while(window[4-s]==(uint8_t) 255 && s!=5){
						s=s+1;
					}
					window[4]=window[(4-s)/2];
				}
				if(row==1){
					tmp[col]=window[4];
				}
				else{
					frame[(row-2)*WIDTH+col]=tmp[col];
					tmp[col]=window[4];
				}
			}
		}
	}
	else{
		for(row=1;row<received_row-1;row++){
			for(col=0;col<WIDTH;col++){
				//Fill Window
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=-1;wcol<2;wcol++){
						if(col==0){
							if(wcol==-1){
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+(wcol+1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else if(col==WIDTH-1){
							if(wcol==2){
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+(wcol-1)];
							}
							else{
								window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
							}
						}
						else{
							window[(wrow+1)*3+(wcol+1)]=frame[row*WIDTH+col+wrow*WIDTH+wcol];
						}
					}
				}
				//Sort Window
				sort(window);
				s=1;
				if (window[4]==(uint8_t) 0){
					while(window[4+s]==(uint8_t) 0 && s!=5){
						s=s+1;
					}
					window[4]=window[4+s+(10-(5+s))/2];
				}
				if (window[4]==(uint8_t) 255){
					while(window[4-s]==(uint8_t) 255 && s!=5){
						s=s+1;
					}
					window[4]=window[(4-s)/2];
				}
				if(row==1){
					tmp[col]=window[4];
				}
				else{
					frame[(row-2)*WIDTH+col]=tmp[col];
					tmp[col]=window[4];
				}
			}
		}
	}
	for (col=0;col<WIDTH;col++){ //Final row written from tmp to frame
		frame[WIDTH*rowcount+col]=tmp[col];
	}
	return 1;
}

_Bool edgeDetection(){ // Laplacian filter as an Edge Detector
	int row=0,col,element,value,wrow,wcol;
	int window[9] = {-1,-1,-1,-1,8,-1,-1,-1,-1}; // Laplacian Kernel
	uint8_t temp;
	for(col=0;col<WIDTH;col++){ //row 0
		if(col==0){
			value=0;
			for(wrow=0;wrow<2;wrow++){
				for(wcol=0;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else if(col==WIDTH-1){
			value=0;
			for(wrow=0;wrow<2;wrow++){
				for(wcol=-1;wcol<1;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else{
			value=0;
			for(wrow=0;wrow<2;wrow++){
				for(wcol=-1;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					if(wrow==0){
						value+=window[(wrow)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
			}
			tmp[col]=(uint8_t) (value/9<0 ? 0 : 255);
		}
	}
	for(row=1;row<rowcount;row++){
		for(col=0;col<WIDTH;col++){
			if(col==0){
				value=0;
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=0;wcol<2;wcol++){
						value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
				temp=tmp[col];
				tmp[col]=(uint8_t) (value/6<0 ? 0 : 255);
			}
			else if(col==WIDTH-1){
				value=0;
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=-1;wcol<1;wcol++){
						value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
				frame[(row-1)*WIDTH+col-1]=temp;
				frame[(row-1)*WIDTH+col]=tmp[col];
				tmp[col]=(uint8_t) (value/6<0 ? 0 : 255);
			}
			else{
				value=0;
				for(wrow=-1;wrow<2;wrow++){
					for(wcol=-1;wcol<2;wcol++){
						value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
				frame[(row-1)*WIDTH+col-1]=temp;
				temp=tmp[col];
				tmp[col]=(uint8_t) (value/9<0 ? 0 : 255);
			}
		}
	}
	row=rowcount;
	for(col=0;col<WIDTH;col++){ //row equals rowcount
		if(col==0){
			value=0;
			for(wrow=-1;wrow<2;wrow++){
				for(wcol=0;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			temp=tmp[col];
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else if(col==WIDTH-1){
			value=0;
			for(wrow=-1;wrow<2;wrow++){
				for(wcol=-1;wcol<1;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
				}
			}
			frame[(row-1)*WIDTH+col-1]=temp;
			frame[(row-1)*WIDTH+col]=tmp[col];
			tmp[col]=(uint8_t) (value/4<0 ? 0 : 255);
		}
		else{
			value=0;
			for(wrow=-1;wrow<1;wrow++){
				for(wcol=-1;wcol<2;wcol++){
					value+=window[(wrow+1)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					if(wrow==0){
						value+=window[(wrow+2)*3+(wcol+1)]*(int)frame[row*WIDTH+col+wrow*WIDTH+wcol];
					}
				}
			}
			frame[(row-1)*WIDTH+col-1]=temp;
			temp=tmp[col];
			tmp[col]=(uint8_t) (value/9<0 ? 0 : 255);
		}
	}
	for(col=0;col<WIDTH;col++){ // Final row written from tmp to frame
		frame[rowcount*WIDTH+col]=tmp[col];
	}
}

int main(void){
	int i,j;
  	e_coreid_t coreid = e_get_coreid();
	unsigned int row, col, cnum;
	e_coords_from_coreid(coreid, &row, &col);
	cnum = row * e_group_config.group_cols + col;

	/* Get number of rows received according to core number */
	if(cnum==15 || cnum==0){
		received_row=HEIGHT/16+1;
	}
	else{
		received_row=HEIGHT/16+2;
	}

	MSG.core_ready=1;
	//Ready To receive frames
	while(!MSG.signal_terminate){
		while(!MSG.signal_go){}
		medianFilter(cnum);
		edgeDetection();
		MSG.core_complete=1;
		MSG.signal_go=0;
	}

	return 0;
}
