#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h> 
#include <time.h> 
#include "CAENVMElib.h"
#include "CAENVMEtypes.h" 
#include "CAENVMEoslib.h"
#include "adc792_lib.h"
#include <iostream>
#include <vector>

using namespace std;

std::vector<unsigned long> adcaddrs;

unsigned short init_adc792(int32_t BHandle) {

  int status=1;
  unsigned long address;
  unsigned long  DataLong;
  int nZS = 1; int caenst;

  adcaddrs.clear();
  adcaddrs.push_back(V792N_ADDRESS);
  if(NUMADBOARDS >= 2) adcaddrs.push_back(V792N_ADDRESS2);
  if(NUMADBOARDS >= 3) adcaddrs.push_back(V792N_ADDRESS3);

  //Initialize all the boards
  for(int iBo = 0; iBo<NUMADBOARDS; iBo++) {

    /* QDC Reset */
    address =  adcaddrs.at(iBo) + 0x1000;
    caenst = CAENVME_ReadCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 

    if(status != 1) {
      printf("Error READING %d V792N firmware -> address=%lx \n",iBo,address);
      return status;
    }
    else {
      if(adc792_debug) printf("V792N %d firmware is version:  %lx \n",iBo,DataLong);
    }

    //Bit set register
    address = adcaddrs.at(iBo) + 0x1006;
    DataLong = 0x80; //Issue a software reset. To be cleared with bit clear register access
    caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 

    caenst = CAENVME_ReadCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    if(status != 1) {
      printf("Bit Set Register read: %li\n", DataLong);
    }
    
    //Control Register: enable BLK_end
    address = adcaddrs.at(iBo) + 0x1010;
    DataLong = 0x4; //Sets bit 2 to 1 [enable blkend]

    caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    caenst = CAENVME_ReadCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    if(status != 1) {
      printf("Bit Set Register read: %li\n", DataLong);
    }
    
    //Bit clear register
    address = adcaddrs.at(iBo) + 0x1008;
    DataLong = 0x80; //Release the software reset. 
    caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    caenst = CAENVME_ReadCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    if(status != 1) {
      printf("Bit Clear Register read: %li\n", DataLong);
    }
    
    //Bit set register 2
    //Enable/disable zero suppression
    if(nZS) {
      address = adcaddrs.at(iBo) + 0x1032;
      DataLong = 0x18; //Disable Zero Suppression + disable overfl
      caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
      status *= (1-caenst); 
      if(status != 1) {
	printf("Could not disable ZS: %li\n", DataLong);
      }
    } else {
      address = adcaddrs.at(iBo) + 0x1032;
      DataLong = 0x88; //Enable Zero Suppression + disable overfl
      caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
      status *= (1-caenst); 
      if(status != 1) {
	printf("Could not disable ZS: %li\n", DataLong);
      }
    }
    
    //Set the thresholds.
    for(int i=0; i<16; i++) {
      address = adcaddrs.at(iBo) + 0x1080 +4*i;
      if(adc792_debug) printf("Add : %lx\n",address);
      DataLong = 0x12; //Threshold
      caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
      status *= (1-caenst); 
      if(adc792_debug) printf("Iped register read: %li\n", DataLong);
      if(status != 1) {
	printf("Threshold register read: %li\n", DataLong);
      }
    }
    
    //Set the Iped value to XX value [180, defaults; >60 for coupled channels]
    address = adcaddrs.at(iBo) + 0x1060;
    //  status = vme_read_dt(address, &DataLong, AD32, D16);
    caenst = CAENVME_ReadCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    //    if(adc792_debug) printf("Iped register read: %li\n", DataLong);
    DataLong = 0xFF; //iped value
    caenst = CAENVME_WriteCycle(BHandle,address,&DataLong,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 
    printf("Iped register read: %li\n", DataLong);
    
    
     if(status != 1) {
      printf("Iped register read: %li\n", DataLong);
    }
  }//End of multiple boards loop
  return status;
 }




/*------------------------------------------------------------------*/

vector<int> read_adc792(int32_t BHandle, int status)
{
  /*
    reading of the V792N 
    returns vector with output
  */
  vector<int> adc_val; adc_val.clear(); adc_val.resize(V792N_CHANNEL);
  vector<int> outD;
  int caenst;
  unsigned long address, data, dt_type;
  unsigned long adc792_rdy, adc792_busy, evtnum;
  unsigned long full,empty, evc_lsb, evc_msb, adc_value, adc_chan;
  unsigned int ncha;
  status = 1; 

  /*
    check if the fifo has something inside: use status register 1
   */  
  address = V792N_ADDRESS + adc792_shift.statusreg1;
  //status *= vme_read_dt(address,&data,AD32,D16);
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status = (1-caenst); 

  if(adc792_debug) printf("ST (str1) :: %i, %lx, %lx \n",status,address,data);
  adc792_rdy = data & adc792_bitmask.rdy;
  adc792_busy = data & adc792_bitmask.busy;

  //Trigger status
  address = V792N_ADDRESS + 0x1020;
  //status *= vme_read_dt(address,&data,AD32,D16);
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status *= (1-caenst); 
  if(adc792_debug) printf("ST (trg1) :: %i, %lx, %lx \n",status,address,data);


  //Event counter register
  address = V792N_ADDRESS + 0x1024;
  //  status *= vme_read_dt(address,&data,AD32,D16);
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status *= (1-caenst); 
  evc_lsb = data;
  address = V792N_ADDRESS + 0x1026;
  // status *= vme_read_dt(address,&data,AD32,D16);
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status *= (1-caenst); 
  evc_msb = data & 0xFF;
  
  if(adc792_rdy == 1 && adc792_busy == 0) {
    /*
      Data Ready and board not busy
    */
    
    //Read the Event Header
    address = V792N_ADDRESS;
    //status = vme_read_dt(address,&data,AD32,D32);
    caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD32);
    status *= (1-caenst); 
    if(adc792_debug) printf("Data Header :: %i, %lx, %lx \n",status,address,data);
    ncha =  data>>8 & 0x3F;
    if(adc792_debug) cout<<"Going to Read "<<ncha<<" channels!"<<endl;
    full = 0; empty = 0;
    while(!full && !empty) {

      //Read MEB for each channel and get the ADC value
      address = V792N_ADDRESS;
      //status = vme_read_dt(address,&data,AD32,D32);
      caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD32);
      status *= (1-caenst); 
      if(adc792_debug) printf("Reading and got:: %i %lx %lx\n", status, data, address);

      dt_type = data>>24 & 0x7;
      if(adc792_debug) printf("typ:: %lx\n", dt_type);

      if(!(dt_type & 0x7)) {
	adc_value = data & 0xFFF;
	adc_chan = data>>17 & 0xF;
	if(adc792_debug) cout<<adc_value<<" "<<adc_chan<<" "<<endl;
	adc_val[adc_chan] = adc_value;
	if(data>>12 & 0x1) cout<<" Overflow, my dear !! "<<adc_value<<endl;
      } else if(dt_type & 0x4) {
	//EOB
	evtnum = data & 0xFFFFFF;
	if(adc792_debug) cout<<"EvtNum "<<evtnum<<endl;
      } else if(dt_type & 0x2) {
	//Header
	if(adc792_debug) cout<<" ERROR:: THIS SHOULD NOT HAPPEN!!!"<<endl;
      }

      //Check the status register to check the MEB status
      address = V792N_ADDRESS + adc792_shift.statusreg2;
      //status = vme_read_dt(address,&data,AD32,D16);
      caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
      status *= (1-caenst); 
  
      full = data &  adc792_bitmask.full;
      empty = data &  adc792_bitmask.empty;
      if(full) {
	cout<<"MEB Full:: "<<full<<" !!!"<<endl;
      }
    }

    //Write event in output vector
    for(int iC = 0; iC<(int)adc_val.size(); iC++) {
      outD.push_back(adc_val[iC]);
    }
  }
  
  return outD;
}

vector<int> readFastadc792(int32_t BHandle, int idB, int status)
{
  /*
    Implements Block Transfer Readout of V792N 
    returns vector with output
  */
  int nbytes_tran = 0;
  vector<int> outD; outD.clear();
  status = 1; 
  int caenst;
  unsigned long data,address;
  unsigned long dataV[34]; int wr;
  unsigned long adc792_rdy, adc792_busy;
  unsigned int ncha, idV;
  struct timeval tv;
  time_t curt, curt2, tdiff, tdiff1, curt3;

  if(idB<0 || idB>NUMADBOARDS-1) {
    cout<<" Accssing Board number"<<idB<<" while only "<<NUMADBOARDS<<" are initialized!!! Check your configuration!"<<endl;
    status = 0;
    return outD;
  }

  /*
    check if the fifo has something inside: use status register 1
  */  
  if(adc792_debug) {
    gettimeofday(&tv, NULL); 
    curt3=tv.tv_usec;
  }

  address = adcaddrs.at(idB) + adc792_shift.statusreg1;
  //status *= vme_read_dt(address,&data,AD32,D16);
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status *= (1-caenst); 
  if(adc792_debug) printf("ST (str1) :: %i, %lx, %lx \n",status,address,data);
  adc792_rdy = data & adc792_bitmask.rdy;
  adc792_busy = data & adc792_bitmask.busy;

  //Event counter register
  if(adc792_debug) {
    gettimeofday(&tv, NULL); 
    curt=tv.tv_usec;
  }

  if(adc792_rdy == 1 && adc792_busy == 0) {
    /*
      Data Ready and board not busy
    */

    //Read the Event Header
    address = adcaddrs.at(idB);
    //status = vme_read_dt(address,&data,AD32,D32);
    caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD32);
    status *= (1-caenst); 
    //I put the header in
    outD.push_back((int)data);

    if(adc792_debug) printf("Data Header :: %i, %lx, %lx \n",status,address,data);
    ncha =  data>>8 & 0x3F;
    if(adc792_debug) cout<<"Going to Read "<<ncha<<" channels!"<<endl;

    //Vector reset
    idV = 0; while(idV<34) { dataV[idV] = 0; idV++; }
    wr = (ncha+1)*4;
    //    printf("numchann %d \n",ncha);

    //status *= vme_read_blk(address,dataV,wr,AD32,D32);    
    caenst = CAENVME_BLTReadCycle(BHandle,address,dataV,wr,
				  cvA32_U_DATA,cvD32,&nbytes_tran);
    status *= (1-caenst); 

    //Vector dump into output
    idV = 0; while(idV<ncha+1) { 
      outD.push_back((int)dataV[idV]); 
      //      cout<<" "<<(int)dataV[idV]<<" "<<idV<<endl; 
      idV++;  }
  }

  if(adc792_debug) {
    gettimeofday(&tv, NULL); 
    curt2=tv.tv_usec;
    tdiff = (curt2-curt);
    tdiff1 = (curt-curt3);
    cout<<"Just BLT "<<curt<<" "<<curt2<<" "<<tdiff<<" "<<tdiff1<<endl;
  }
  return outD;
}

vector<int> readFastNadc792(int32_t BHandle, int idB, int status, int nevts, vector<int> &outW)
{
  /*
    Implements Block Transfer Readout of V792N 
    returns vector with output
  */
  int nbytes_tran = 0;
  vector<int> outD; outD.clear();
  status = 1; 
  int caenst;
  unsigned long data,address;
  //AS  unsigned long dataV[34];
  unsigned long dataV[18*nevts];
  int wr;
  unsigned long adc792_rdy, adc792_busy;
  unsigned int ncha(16), idV;

  if(idB<0 || idB>NUMADBOARDS-1) {
    cout<<" Accssing Board number"<<idB<<" while only "<<NUMADBOARDS<<" are initialized!!! Check your configuration!"<<endl;
    status = 0;
    return outD;
  }

  /*
    check if the fifo has something inside: use status register 1
  */  
  address = adcaddrs.at(idB) + adc792_shift.statusreg1;
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status *= (1-caenst); 

  if(adc792_debug) printf("ST (str1) :: %i, %lx, %lx \n",status,address,data);
  adc792_rdy = data & adc792_bitmask.rdy;
  adc792_busy = data & adc792_bitmask.busy;
  int tmpW;
  unsigned long full,empty;

  //Check the status register to check the MEB status
  address = adcaddrs.at(idB) + adc792_shift.statusreg2;
  caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
  status *= (1-caenst); 

  full = data &  adc792_bitmask.full;
  empty = data &  adc792_bitmask.empty;

  //Event counter register
  if(adc792_debug)  cout<<" rdy:: "<<adc792_rdy<<" busy:: "<<adc792_busy<<endl;

  if(adc792_rdy == 1 && adc792_busy == 0) {
    /*
      Data Ready and board not busy
    */
    tmpW = 0;
    full = 0; empty = 0;
    //Read the Event Header
    address = adcaddrs.at(idB);

    //Vector reset
    idV = 0; while(idV<(ncha+2)*nevts) { dataV[idV] = 0; idV++; }
    wr = (ncha+2)*4*nevts; 

    caenst = CAENVME_BLTReadCycle(BHandle,address,dataV,wr,
				  cvA32_U_DATA,cvD32,&nbytes_tran);
    status *= (1-caenst); 

    //Check the status register to check the MEB status
    address = adcaddrs.at(idB) + adc792_shift.statusreg2;
    caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
    status *= (1-caenst); 

    //Vector dump into output
    idV = 0; while(idV<(ncha+2)*nevts) { 
      outD.push_back((int)dataV[idV]); 
      idV++;  
    }

    full = data &  adc792_bitmask.full;
    empty = data &  adc792_bitmask.empty;

    int ntry = 100, nt = 0;
    while(!empty && nt<ntry) {
      //Check the status register to check the MEB status
      address = adcaddrs.at(idB);
      caenst = CAENVME_BLTReadCycle(BHandle,address,dataV,(ncha+2)*4,
				    cvA32_U_DATA,cvD32,&nbytes_tran);
      status *= (1-caenst); 

      //Vector dump into output
      idV = 0; while(idV<(ncha+2)) { 
	outD.push_back((int)dataV[idV]); 
	idV++;  
      }

      //Check the status register to check the MEB status
      address = adcaddrs.at(idB) + adc792_shift.statusreg2;
      caenst = CAENVME_ReadCycle(BHandle,address,&data,cvA32_U_DATA,cvD16);
      status *= (1-caenst); 
      
      full = data &  adc792_bitmask.full;
      empty = data &  adc792_bitmask.empty;
      nt++;
    }
    if(nt) cout<<" Warning:: needed "<<nt<<" add read to clean up MEB"<<endl;

    outW.push_back(18);
  }

  return outD;
}

