#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../negentropy_wrapper.h"


int main(){
   void* st1 = storage_new("","");
   if(st1 == NULL){
    perror("failed to create storage");
   }
   void* ngn_inst1 = negentropy_new(st1, 153600);
   if(ngn_inst1 == NULL){
    perror("failed to create negentropy instance");
   }

      void* st2 = storage_new("","");
   if(st2 == NULL){
    perror("failed to create storage");
   }
   void* ngn_inst2 = negentropy_new(st2, 153600);
   if(ngn_inst2 == NULL){
    perror("failed to create negentropy instance");
   }

   unsigned char m1[] =  {0x6a, 0xdf, 0xaa, 0xe0, 0x31, 0xeb, 0x61, 0xa8, \
                          0x3c, 0xff, 0x9c, 0xfd, 0xd2, 0xae, 0xf6, 0xed, \
                          0x63, 0xda, 0xcf, 0xaa, 0x96, 0xd0, 0x51, 0x26, \
                          0x7e, 0xf1, 0x0c, 0x8b, 0x61, 0xae, 0x35, 0xe9};//"61dfaae031eb61a83cff9cfdd2aef6ed63dacfaa96d051267ef10c8b61ae35e9";
   buffer b1 ;
   b1.len = 32;
   b1.data = m1;

/*    char m2[] = "28798d295c30c7e6d9a4a96cdda7e020f7aa7168cce063302ed19b856332959e";
   buffer b2 ;
   b2.len = 32;
   b2.data = m2; */

   bool ret = storage_insert(st1,time(NULL),&b1);
   if (ret){
      printf("inserted hash successfully in st1\n");
   }
/* 
   ret = storage_insert(st2,time(NULL),&b2);
   if (ret){
      printf("inserted hash %s successfully in st2\n", m2);
   }

   ret = storage_insert(st2,time(NULL),&b1);
   if (ret){
      printf("inserted hash %s successfully in st2\n", m1);
   }
 */

/*    std::string out;
   negentropy_initiate(ngn_inst1, &out);
   if(out.size() == 0){
    perror("failed to initiate negentropy instance");
   }
   printf("initiated negentropy successfully with output of len %zu \n", out.size());
   buffer b3 ;
   b3.len = out.size();
   b3.data = (unsigned char*)malloc(b3.len);
   memcpy(b3.data, out.c_str(),out.size());

   const char* req2 = reconcile(ngn_inst2, &b3); */

   //free(b3.data);
   //b3.len = len;
   //b3.data = (char*)malloc(len);

   //reconcile_with_ids(ngn_inst1, &b3, )
   
}