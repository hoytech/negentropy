#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../negentropy_wrapper.h"

void printHexBuffer(buffer buf){
    for (uint64_t i = 0; i < buf.len; ++i) {
        printf("%0hhx", buf.data[i]);
    }
    printf("\n");
}

void rec_callback(buffer* have_ids, uint64_t have_ids_len, buffer* need_ids, uint64_t need_ids_len, buffer* output){
   printf("needIds count:%llu , haveIds count: %llu \n",need_ids_len, have_ids_len);

   for (int i=0; i < need_ids_len ; i++) {
      printf("need ID at %d :", i);
      printHexBuffer(need_ids[i]);
   }

   for (int j=0; j < have_ids_len ; j++) {
      printf("need ID at %d :", j);
      printHexBuffer(have_ids[j]);
   }
}


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

   unsigned char m2[] = {0x28 ,0x79 ,0x8d ,0x29 ,0x5c ,0x30 ,0xc7 ,0xe6 \
                  ,0xd9 ,0xa4 ,0xa9 ,0x6c ,0xdd ,0xa7 ,0xe0 ,0x20 \
                  ,0xf7 ,0xaa ,0x71 ,0x68 ,0xcc ,0xe0 ,0x63 ,0x30 \
                  ,0x2e ,0xd1 ,0x9b ,0x85 ,0x63 ,0x32 ,0x95 ,0x9e}; //28798d295c30c7e6d9a4a96cdda7e020f7aa7168cce063302ed19b856332959e
   buffer b2 ;
   b2.len = 32;
   b2.data = m2; 

   bool ret = storage_insert(st1,time(NULL),&b1);
   if (ret){
      printf("inserted hash successfully in st1\n");
   }
 
   ret = storage_insert(st2,time(NULL),&b2);
   if (ret){
      printf("inserted hash successfully in st2\n");
   }

   ret = storage_insert(st2,time(NULL),&b1);
   if (ret){
      printf("inserted hash successfully in st2\n");
   }
 
   buffer b4 ;
   b4.len = 153600;
   b4.data = (unsigned char*)malloc(153600); 

   size_t outSize = negentropy_initiate(ngn_inst1, &b4);
   if(outSize == 0){
    perror("failed to initiate negentropy instance");
   }
   printf("initiated negentropy successfully with output of len %zu \n", outSize);
   b4.len = outSize;

   buffer b3 ;
   b3.len = 153600;
   b3.data = (unsigned char*)malloc(153600);

   outSize = reconcile(ngn_inst2, &b4, &b3); 
   if(outSize == 0){
    perror("nothing to reconcile");
   }
   printf("reconcile returned with output of len %zu \n", outSize);
   b3.len = outSize;

   //outSize = reconcile_with_ids(ngn_inst1, &b3, &rec_callback);

   result res;
   reconcile_with_ids_no_cbk(ngn_inst1, &b3, &res);
   printf("needIds count:%llu , haveIds count: %llu \n",res.need_ids_len, res.have_ids_len);

   for (int i=0; i < res.need_ids_len ; i++) {
      printf("need ID at %d :", i);
      printHexBuffer(res.need_ids[i]);
   }

   for (int j=0; j < res.have_ids_len ; j++) {
      printf("need ID at %d :", j);
      printHexBuffer(res.have_ids[j]);
   }

   free(b3.data);
   free(b4.data);
}